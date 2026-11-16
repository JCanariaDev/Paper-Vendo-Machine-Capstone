-- Roll back only the retry-idempotency guard added by
-- transaction_finish_idempotency_migration.sql.
-- This restores the prior machine_finish_transaction implementation and does
-- not modify existing transaction, line, or inventory rows.
-- Warning: retries after a committed finish may apply inventory updates again.

CREATE OR REPLACE FUNCTION machine_finish_transaction(
    p_transaction_id UUID,
    p_results JSONB,
    p_change_paid_cents INTEGER DEFAULT 0
)
RETURNS TABLE(tr_number TEXT, final_status TEXT, change_due_cents INTEGER, change_paid_cents INTEGER)
LANGUAGE plpgsql AS $$
DECLARE
    v_tx sales_transactions%ROWTYPE;
    v_line RECORD;
    v_result JSONB;
    v_actual INTEGER;
    v_all_success BOOLEAN := TRUE;
    v_any_success BOOLEAN := FALSE;
    v_any_failure BOOLEAN := FALSE;
    v_coin JSONB;
    v_paid INTEGER;
    v_final_status TEXT;
    v_reason TEXT := NULL;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND THEN RAISE EXCEPTION 'Transaction % not found', p_transaction_id; END IF;

    v_paid := COALESCE(p_change_paid_cents, 0);
    IF v_paid > v_tx.change_due_cents THEN v_paid := v_tx.change_due_cents; END IF;

    FOR v_line IN SELECT * FROM sales_transaction_lines WHERE transaction_id = p_transaction_id FOR UPDATE LOOP
        SELECT value INTO v_result FROM jsonb_array_elements(p_results)
         WHERE (value->>'item_type') = v_line.item_type AND (value->>'product_id')::INTEGER = v_line.product_id;
        v_actual := COALESCE((v_result->>'qty_dispensed')::INTEGER, 0);
        IF v_actual > v_line.qty_requested THEN v_actual := v_line.qty_requested; END IF;

        UPDATE sales_transaction_lines
           SET qty_dispensed = v_actual,
               line_status = CASE WHEN v_actual = v_line.qty_requested THEN 'DISPENSED' ELSE 'FAILED' END
         WHERE id = v_line.id;

        IF v_line.item_type = 'pen' THEN
            UPDATE ballpen_compartments
               SET current_piece_stock = current_piece_stock - v_actual,
                   reserved_piece_stock = GREATEST(0, reserved_piece_stock - v_line.qty_requested),
                   updated_at = NOW()
             WHERE dispenser_channel = v_line.physical_channel;
        END IF;

        IF v_actual <> v_line.qty_requested THEN
            v_all_success := FALSE;
            v_any_failure := TRUE;
            IF v_reason IS NULL THEN
                v_reason := CASE v_line.item_type
                    WHEN 'paper' THEN 'Paper Bay ' || v_line.physical_channel || ' failed to confirm the requested paper output'
                    WHEN 'pen' THEN 'Ballpen Uno / Bay ' || v_line.physical_channel || ' failed to confirm the requested pen output'
                    ELSE 'Unknown dispenser failed to confirm the requested output'
                END;
            END IF;
        END IF;
        IF v_actual > 0 THEN v_any_success := TRUE; END IF;
    END LOOP;

    FOR v_coin IN SELECT value FROM jsonb_array_elements(COALESCE(v_tx.change_plan, '[]'::jsonb)) LOOP
        UPDATE change_inventory
           SET reserved_coin_count = GREATEST(0, reserved_coin_count - ((v_coin->>'count')::INTEGER)),
               current_coin_count = GREATEST(0, current_coin_count - LEAST(v_paid / ((v_coin->>'denomination_cents')::INTEGER), (v_coin->>'count')::INTEGER)),
               updated_at = NOW()
         WHERE hopper_channel = ((v_coin->>'hopper_channel')::INTEGER);
    END LOOP;

    IF v_any_success AND v_any_failure THEN
        v_final_status := 'PARTIAL_SUCCESS';
        v_reason := COALESCE(v_reason, 'Some products dispensed successfully while other products failed');
    ELSIF NOT v_all_success THEN
        v_final_status := 'FAILED_DISPENSE';
        v_reason := COALESCE(v_reason, 'Physical dispense sensor did not confirm all requested output');
    ELSIF v_paid < v_tx.change_due_cents THEN
        v_final_status := 'COMPLETED_CHANGE_OWED';
        v_reason := 'Unreleased change of PHP ' || TO_CHAR((v_tx.change_due_cents - v_paid) / 100.0, 'FM999,990.00') || ' owed to student';
    ELSE
        v_final_status := 'COMPLETED';
    END IF;

    UPDATE sales_transactions
       SET status = v_final_status,
           change_paid_cents = v_paid,
           failure_reason = v_reason,
           completed_at = NOW()
     WHERE id = p_transaction_id;

    RETURN QUERY SELECT v_tx.tr_number, v_final_status, v_tx.change_due_cents, v_paid;
END;
$$;
