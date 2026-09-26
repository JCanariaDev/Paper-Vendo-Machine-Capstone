-- Persistent credit sessions.
-- Run this once against an existing database. It does not delete sales data.

ALTER TABLE sales_transactions DROP CONSTRAINT IF EXISTS sales_transactions_status_check;
ALTER TABLE sales_transactions
  ADD CONSTRAINT sales_transactions_status_check CHECK (
    status IN ('CREDIT_HELD', 'RESERVED', 'CHANGE_PAID', 'COMPLETED',
               'CANCELLED', 'FAILED_CHANGE', 'FAILED_DISPENSE',
               'PARTIAL_SUCCESS', 'REFUNDED', 'COMPLETED_CHANGE_OWED')
  );

CREATE OR REPLACE FUNCTION machine_update_credit_session(p_credit_cents INTEGER)
RETURNS TABLE(transaction_id UUID, tr_number TEXT, session_status TEXT, credit_received_cents INTEGER)
LANGUAGE plpgsql AS $$
DECLARE
  v_tx sales_transactions%ROWTYPE;
BEGIN
  SELECT * INTO v_tx
    FROM sales_transactions
   WHERE machine_id = 'paper-vendo-01' AND status = 'CREDIT_HELD'
   ORDER BY created_at DESC LIMIT 1 FOR UPDATE;

  IF p_credit_cents <= 0 THEN
    IF FOUND THEN
      UPDATE sales_transactions
         SET status = 'CANCELLED', subtotal_cents = 0,
             change_due_cents = credit_received_cents,
             failure_reason = 'Session reset before checkout; credits await administrator release',
             completed_at = NOW()
       WHERE id = v_tx.id;
      RETURN QUERY SELECT v_tx.id, v_tx.tr_number, 'CANCELLED'::TEXT, v_tx.credit_received_cents;
    END IF;
    RETURN;
  END IF;

  IF FOUND THEN
    UPDATE sales_transactions
       SET credit_received_cents = p_credit_cents, subtotal_cents = 0,
           change_due_cents = p_credit_cents
     WHERE id = v_tx.id
     RETURNING * INTO v_tx;
  ELSE
    INSERT INTO sales_transactions (machine_id, status, credit_received_cents,
                                    subtotal_cents, change_due_cents,
                                    change_paid_cents, change_plan)
    VALUES ('paper-vendo-01', 'CREDIT_HELD', p_credit_cents, 0,
            p_credit_cents, 0, '[]'::jsonb)
    RETURNING * INTO v_tx;
  END IF;

  RETURN QUERY SELECT v_tx.id, v_tx.tr_number, v_tx.status, v_tx.credit_received_cents;
END;
$$;

CREATE OR REPLACE FUNCTION machine_reserve_transaction_with_session(p_credit_cents INTEGER, p_lines JSONB)
RETURNS TABLE(transaction_id UUID, tr_number TEXT, subtotal_cents INTEGER,
              change_due_cents INTEGER, dispense_plan JSONB)
LANGUAGE plpgsql AS $$
DECLARE
  v_session_id UUID;
  v_session_tr TEXT;
  v_reserved RECORD;
BEGIN
  SELECT st.id, st.tr_number INTO v_session_id, v_session_tr
    FROM sales_transactions AS st
   WHERE st.machine_id = 'paper-vendo-01' AND st.status = 'CREDIT_HELD'
   ORDER BY st.created_at DESC LIMIT 1 FOR UPDATE;

  SELECT * INTO v_reserved
    FROM machine_reserve_transaction(p_credit_cents, p_lines);

  IF v_session_id IS NULL THEN
    RETURN QUERY SELECT v_reserved.transaction_id, v_reserved.tr_number,
                        v_reserved.subtotal_cents, v_reserved.change_due_cents,
                        v_reserved.dispense_plan;
    RETURN;
  END IF;

  UPDATE sales_transactions
     SET status = 'RESERVED', credit_received_cents = p_credit_cents,
         subtotal_cents = v_reserved.subtotal_cents,
         change_due_cents = v_reserved.change_due_cents,
         change_plan = (SELECT st.change_plan
                          FROM sales_transactions AS st
                         WHERE st.id = v_reserved.transaction_id),
         failure_reason = NULL, completed_at = NULL
   WHERE id = v_session_id;

  UPDATE machine_logs SET transaction_id = v_session_id, tr_number = v_session_tr
   WHERE transaction_id = v_reserved.transaction_id;
  UPDATE sales_transaction_lines SET transaction_id = v_session_id
   WHERE transaction_id = v_reserved.transaction_id;
  DELETE FROM sales_transactions WHERE id = v_reserved.transaction_id;

  RETURN QUERY SELECT v_session_id, v_session_tr,
                      v_reserved.subtotal_cents, v_reserved.change_due_cents,
                      v_reserved.dispense_plan;
END;
$$;

CREATE OR REPLACE FUNCTION machine_release_change(p_transaction_id UUID)
RETURNS TABLE(tr_number TEXT, final_status TEXT, change_paid_cents INTEGER)
LANGUAGE plpgsql AS $$
DECLARE
  v_tx sales_transactions%ROWTYPE;
  v_status TEXT;
BEGIN
  SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
  IF NOT FOUND THEN RAISE EXCEPTION 'Transaction % not found', p_transaction_id; END IF;
  IF v_tx.change_due_cents <= v_tx.change_paid_cents THEN
    RETURN QUERY SELECT v_tx.tr_number, v_tx.status, v_tx.change_paid_cents;
    RETURN;
  END IF;
  IF v_tx.status NOT IN ('CREDIT_HELD', 'COMPLETED_CHANGE_OWED', 'PARTIAL_SUCCESS',
                         'FAILED_DISPENSE', 'FAILED_CHANGE', 'CANCELLED') THEN
    RAISE EXCEPTION 'Transaction % has no outstanding change to release', v_tx.tr_number;
  END IF;

  v_status := CASE
    WHEN v_tx.status = 'COMPLETED_CHANGE_OWED' THEN 'COMPLETED'
    WHEN v_tx.status = 'CREDIT_HELD' THEN 'REFUNDED'
    ELSE v_tx.status
  END;
  UPDATE sales_transactions
     SET status = v_status, change_paid_cents = change_due_cents,
         completed_at = COALESCE(completed_at, NOW())
   WHERE id = p_transaction_id;

  INSERT INTO machine_logs (level, source, event_type, message, transaction_id, tr_number, metadata)
  VALUES ('INFO', 'DASHBOARD', 'CHANGE_RELEASED',
          'Remaining unused customer credits were handed to the customer',
          p_transaction_id, v_tx.tr_number,
          jsonb_build_object('change_due_cents', v_tx.change_due_cents,
                             'previously_paid_cents', v_tx.change_paid_cents,
                             'unused_credits_cents', GREATEST(0, v_tx.change_due_cents - v_tx.change_paid_cents),
                             'release_type', 'MANUAL_HANDOUT'));

  RETURN QUERY SELECT v_tx.tr_number, v_status, v_tx.change_due_cents;
END;
$$;

GRANT EXECUTE ON FUNCTION machine_update_credit_session(INTEGER) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_reserve_transaction_with_session(INTEGER, JSONB) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_release_change(UUID) TO anon, authenticated;
