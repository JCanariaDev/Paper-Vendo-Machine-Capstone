-- ==============================================================================
-- MIGRATION: Automatic Power-Loss / Reboot Recovery of Interrupted Transactions
-- ==============================================================================
-- When a machine suffers an unexpected reboot or power outage during an active
-- transaction, the transaction can remain stuck in the 'RESERVED' state, locking
-- coin hopper change and compartment stock.
--
-- This function is called by the ESP32 gateway upon boot / Wi-Fi reconnection:
-- 1. Clears locked coin counts from change_inventory (reserved_coin_count).
-- 2. Clears locked pen stock from ballpen_compartments (reserved_piece_stock).
-- 3. Marks transactions as 'FAILED_DISPENSE' with failure_reason 'POWER_OUTAGE_OR_REBOOT'.
-- 4. Enables the administrator dashboard to click "Record Refund" for any credit inserted.
-- ==============================================================================

CREATE OR REPLACE FUNCTION machine_recover_interrupted_reservations(p_reason TEXT DEFAULT 'POWER_OUTAGE_OR_REBOOT')
RETURNS INTEGER
LANGUAGE plpgsql AS $$
DECLARE
    v_tx RECORD;
    v_line RECORD;
    v_coin JSONB;
    v_recovered_count INTEGER := 0;
BEGIN
    FOR v_tx IN 
        SELECT * FROM sales_transactions 
        WHERE status = 'RESERVED' 
        FOR UPDATE 
    LOOP
        -- 1. Unreserve any ballpen stock allocated to this transaction
        FOR v_line IN SELECT * FROM sales_transaction_lines WHERE transaction_id = v_tx.id LOOP
            IF v_line.item_type = 'pen' THEN
                UPDATE ballpen_compartments
                   SET reserved_piece_stock = GREATEST(0, reserved_piece_stock - v_line.qty_requested),
                       updated_at = NOW()
                 WHERE dispenser_channel = v_line.physical_channel;
            END IF;
        END LOOP;

        -- 2. Unreserve coin hopper change allocated to this transaction
        FOR v_coin IN SELECT value FROM jsonb_array_elements(COALESCE(v_tx.change_plan, '[]'::jsonb)) LOOP
            UPDATE change_inventory
               SET reserved_coin_count = GREATEST(0, reserved_coin_count - ((v_coin->>'count')::INTEGER)),
                   updated_at = NOW()
             WHERE hopper_channel = ((v_coin->>'hopper_channel')::INTEGER);
        END LOOP;

        -- 3. Transition status to FAILED_DISPENSE so dashboard can record cash refund
        UPDATE sales_transactions
           SET status = 'FAILED_DISPENSE',
               failure_reason = COALESCE(p_reason, 'POWER_OUTAGE_OR_REBOOT'),
               completed_at = NOW()
         WHERE id = v_tx.id;

        -- 4. Log the recovery event in machine_logs
        INSERT INTO machine_logs (level, source, event_type, message, transaction_id, tr_number, metadata)
        VALUES ('WARN', 'ESP32', 'POWER_OUTAGE_RECOVERY',
                'Unresolved reserved transaction cleared upon machine reboot/startup',
                v_tx.id, v_tx.tr_number,
                jsonb_build_object(
                    'credit_received_cents', v_tx.credit_received_cents,
                    'subtotal_cents', v_tx.subtotal_cents,
                    'change_due_cents', v_tx.change_due_cents
                ));

        v_recovered_count := v_recovered_count + 1;
    END LOOP;

    RETURN v_recovered_count;
END;
$$;

GRANT EXECUTE ON FUNCTION machine_recover_interrupted_reservations(TEXT) TO anon, authenticated;

-- Execute immediately to recover any currently stuck transactions (like TR-00016):
SELECT machine_recover_interrupted_reservations('INITIAL_MIGRATION_CLEANUP') AS recovered_transactions_count;
