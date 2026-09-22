-- Run this once on an existing database created before refund_paid_cents was added.
ALTER TABLE sales_transactions
  ADD COLUMN IF NOT EXISTS refund_paid_cents INTEGER NOT NULL DEFAULT 0
  CHECK (refund_paid_cents >= 0);

CREATE OR REPLACE FUNCTION machine_record_failed_dispense_refund(p_transaction_id UUID)
RETURNS TABLE(tr_number TEXT, refund_paid_cents INTEGER, remaining_refund_cents INTEGER)
LANGUAGE plpgsql AS $$
DECLARE
    v_tx sales_transactions%ROWTYPE;
    v_amount INTEGER;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND THEN RAISE EXCEPTION 'Transaction % not found', p_transaction_id; END IF;
    IF v_tx.status <> 'FAILED_DISPENSE' THEN
        RAISE EXCEPTION 'Only failed-dispense transactions can receive a credit refund';
    END IF;
    IF EXISTS (SELECT 1 FROM sales_transaction_lines WHERE transaction_id = p_transaction_id AND qty_dispensed > 0) THEN
        RAISE EXCEPTION 'A credit refund cannot be recorded because product was physically dispensed';
    END IF;
    v_amount := GREATEST(0, v_tx.credit_received_cents - v_tx.change_paid_cents - v_tx.refund_paid_cents);
    IF v_amount = 0 THEN
        RETURN QUERY SELECT v_tx.tr_number, v_tx.refund_paid_cents, 0;
        RETURN;
    END IF;
    UPDATE sales_transactions
       SET refund_paid_cents = refund_paid_cents + v_amount,
           completed_at = COALESCE(completed_at, NOW())
     WHERE id = p_transaction_id;
    INSERT INTO machine_logs (level, source, event_type, message, transaction_id, tr_number, metadata)
    VALUES ('INFO', 'DASHBOARD', 'FAILED_DISPENSE_REFUND_RECORDED',
            'Administrator recorded a paid-credit refund after a complete dispense failure',
            p_transaction_id, v_tx.tr_number, jsonb_build_object('refund_cents', v_amount));
    RETURN QUERY SELECT v_tx.tr_number, v_tx.refund_paid_cents + v_amount, 0;
END;
$$;

GRANT EXECUTE ON FUNCTION machine_record_failed_dispense_refund(UUID) TO anon, authenticated;
