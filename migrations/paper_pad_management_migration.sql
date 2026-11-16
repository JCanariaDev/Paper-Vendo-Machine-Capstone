-- Additive paper-pad management update. Does not drop or rewrite existing rows.
-- Existing installed pads are intentionally left with NULL sheet counts; an admin
-- must physically count and confirm those values before the new sale guard allows them.

ALTER TABLE public.paper_compartments
  ADD COLUMN IF NOT EXISTS pad_sheets_loaded INTEGER,
  ADD COLUMN IF NOT EXISTS current_pad_sheets_remaining INTEGER,
  ADD COLUMN IF NOT EXISTS reserved_pad_sheets INTEGER NOT NULL DEFAULT 0;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM pg_constraint
     WHERE conrelid = 'public.paper_compartments'::regclass
       AND conname = 'paper_compartments_pad_sheet_counts_check'
  ) THEN
    ALTER TABLE public.paper_compartments
      ADD CONSTRAINT paper_compartments_pad_sheet_counts_check
      CHECK (
        (pad_sheets_loaded IS NULL OR pad_sheets_loaded >= 0)
        AND (current_pad_sheets_remaining IS NULL OR current_pad_sheets_remaining >= 0)
        AND (reserved_pad_sheets >= 0)
        AND (
          pad_sheets_loaded IS NULL
          OR current_pad_sheets_remaining IS NULL
          OR current_pad_sheets_remaining <= pad_sheets_loaded
        )
      );
  END IF;
END;
$$;

CREATE TABLE IF NOT EXISTS public.paper_pad_history (
  id BIGSERIAL PRIMARY KEY,
  compartment_number INTEGER NOT NULL CHECK (compartment_number BETWEEN 1 AND 2),
  event_type TEXT NOT NULL CHECK (event_type IN ('LOAD', 'REPLACE', 'REMOVE', 'COUNT_SET', 'DISPENSE', 'LEGACY_RECONCILE')),
  previous_product_id INTEGER REFERENCES public.paper_inventory(id) ON DELETE SET NULL,
  product_id INTEGER REFERENCES public.paper_inventory(id) ON DELETE SET NULL,
  previous_pad_count INTEGER NOT NULL DEFAULT 0 CHECK (previous_pad_count >= 0),
  previous_sheets INTEGER CHECK (previous_sheets IS NULL OR previous_sheets >= 0),
  resulting_sheets INTEGER CHECK (resulting_sheets IS NULL OR resulting_sheets >= 0),
  quantity_sheets INTEGER NOT NULL DEFAULT 0 CHECK (quantity_sheets >= 0),
  reason TEXT,
  transaction_id UUID REFERENCES public.sales_transactions(id) ON DELETE SET NULL,
  performed_by TEXT,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_paper_pad_history_created
  ON public.paper_pad_history(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_paper_pad_history_bay
  ON public.paper_pad_history(compartment_number, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_paper_pad_history_transaction
  ON public.paper_pad_history(transaction_id);

GRANT SELECT ON public.paper_pad_history TO authenticated, service_role;
GRANT USAGE, SELECT ON SEQUENCE public.paper_pad_history_id_seq TO service_role;

CREATE OR REPLACE FUNCTION public.paper_size_allowed_for_bay(p_bay_number INTEGER, p_paper_size TEXT)
RETURNS BOOLEAN
LANGUAGE SQL
IMMUTABLE
AS $$
  SELECT CASE p_bay_number
    WHEN 1 THEN regexp_replace(lower(trim(coalesce(p_paper_size, ''))), '[[:space:]-]+', '_', 'g') IN ('1_whole', 'crosswise')
    WHEN 2 THEN regexp_replace(lower(trim(coalesce(p_paper_size, ''))), '[[:space:]-]+', '_', 'g') IN ('lengthwise', '1/4')
    ELSE FALSE
  END;
$$;

CREATE OR REPLACE FUNCTION public.admin_manage_paper_pad(
  p_compartment_number INTEGER,
  p_action TEXT,
  p_product_id INTEGER DEFAULT NULL,
  p_sheets INTEGER DEFAULT NULL,
  p_reason TEXT DEFAULT NULL,
  p_performed_by TEXT DEFAULT NULL
)
RETURNS public.paper_compartments
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  v_bay public.paper_compartments%ROWTYPE;
  v_old_product public.paper_inventory%ROWTYPE;
  v_new_product public.paper_inventory%ROWTYPE;
  v_action TEXT := upper(trim(coalesce(p_action, '')));
  v_reason TEXT := nullif(trim(coalesce(p_reason, '')), '');
  v_new_sheets INTEGER;
  v_event TEXT;
  v_previous_product_id INTEGER;
  v_previous_pad_count INTEGER;
  v_previous_sheets INTEGER;
BEGIN
  IF p_compartment_number NOT BETWEEN 1 AND 2 THEN
    RAISE EXCEPTION 'Paper bay must be Bay 1 or Bay 2.' USING ERRCODE = '22023';
  END IF;
  IF v_action NOT IN ('LOAD', 'REPLACE', 'REMOVE', 'COUNT_SET') THEN
    RAISE EXCEPTION 'Unsupported paper pad action.' USING ERRCODE = '22023';
  END IF;
  IF v_action IN ('REPLACE', 'REMOVE', 'COUNT_SET') AND v_reason IS NULL THEN
    RAISE EXCEPTION 'A reason is required for replacement, removal, or sheet-count correction.' USING ERRCODE = '22023';
  END IF;

  SELECT * INTO v_bay
    FROM public.paper_compartments
   WHERE compartment_number = p_compartment_number
   FOR UPDATE;
  IF NOT FOUND THEN RAISE EXCEPTION 'Paper bay % does not exist.', p_compartment_number; END IF;

  IF v_bay.assigned_product_id IS NOT NULL THEN
    SELECT * INTO v_old_product FROM public.paper_inventory WHERE id = v_bay.assigned_product_id;
  END IF;
  v_previous_product_id := v_bay.assigned_product_id;
  v_previous_pad_count := v_bay.current_pad_stock;
  v_previous_sheets := v_bay.current_pad_sheets_remaining;

  IF v_action <> 'COUNT_SET' AND v_bay.reserved_pad_sheets > 0 THEN
    RAISE EXCEPTION 'This paper bay has sheets reserved for an active transaction.' USING ERRCODE = '22023';
  END IF;

  IF v_action = 'COUNT_SET' THEN
    IF v_bay.assigned_product_id IS NULL THEN
      RAISE EXCEPTION 'Load a pad before setting its sheet count.' USING ERRCODE = '22023';
    END IF;
    IF p_sheets IS NULL OR p_sheets < 0 THEN
      RAISE EXCEPTION 'Remaining sheets must be zero or greater.' USING ERRCODE = '22023';
    END IF;
    IF p_sheets < v_bay.reserved_pad_sheets THEN
      RAISE EXCEPTION 'Remaining sheets cannot be lower than sheets reserved for active orders.' USING ERRCODE = '22023';
    END IF;
    v_new_sheets := p_sheets;
    UPDATE public.paper_compartments
       SET pad_sheets_loaded = CASE WHEN pad_sheets_loaded IS NULL THEN p_sheets ELSE pad_sheets_loaded END,
           current_pad_sheets_remaining = v_new_sheets,
           updated_at = NOW()
     WHERE id = v_bay.id
     RETURNING * INTO v_bay;
    v_event := 'COUNT_SET';

  ELSIF v_action = 'REMOVE' THEN
    IF v_bay.assigned_product_id IS NULL THEN
      RAISE EXCEPTION 'Paper bay % has no assigned pad to remove.', p_compartment_number USING ERRCODE = '22023';
    END IF;
    UPDATE public.paper_compartments
       SET assigned_product_id = NULL,
           current_pad_stock = 0,
           pad_sheets_loaded = NULL,
           current_pad_sheets_remaining = NULL,
           reserved_pad_sheets = 0,
           presence_status = 'LOW',
           updated_at = NOW()
     WHERE id = v_bay.id
     RETURNING * INTO v_bay;
    IF NOT EXISTS (
      SELECT 1 FROM public.paper_compartments
       WHERE assigned_product_id = v_old_product.id
    ) THEN
      UPDATE public.paper_inventory
         SET location_status = CASE WHEN stock_pads > 0 THEN 'In stock' ELSE 'Out of stock' END,
             updated_at = NOW()
       WHERE id = v_old_product.id;
    END IF;
    v_event := 'REMOVE';

  ELSE
    IF p_product_id IS NULL OR p_sheets IS NULL OR p_sheets <= 0 THEN
      RAISE EXCEPTION 'Choose a paper product and enter a positive starting sheet count.' USING ERRCODE = '22023';
    END IF;
    IF v_action = 'LOAD' AND v_bay.assigned_product_id IS NOT NULL THEN
      RAISE EXCEPTION 'Remove the currently assigned pad before loading another.' USING ERRCODE = '22023';
    END IF;
    IF v_action = 'REPLACE' AND v_bay.assigned_product_id IS NULL THEN
      RAISE EXCEPTION 'This bay is empty; use LOAD instead of REPLACE.' USING ERRCODE = '22023';
    END IF;

    SELECT * INTO v_new_product
      FROM public.paper_inventory
     WHERE id = p_product_id
     FOR UPDATE;
    IF NOT FOUND OR NOT v_new_product.active THEN
      RAISE EXCEPTION 'Selected paper product is missing or archived.' USING ERRCODE = '22023';
    END IF;
    IF NOT public.paper_size_allowed_for_bay(p_compartment_number, v_new_product.paper_size) THEN
      RAISE EXCEPTION 'Paper size % cannot be assigned to Paper Bay %.', v_new_product.paper_size, p_compartment_number USING ERRCODE = '23514';
    END IF;
    IF v_new_product.stock_pads < 1 THEN
      RAISE EXCEPTION 'No pads of the selected product remain in storage.' USING ERRCODE = '23514';
    END IF;

    UPDATE public.paper_inventory
       SET stock_pads = stock_pads - 1,
           location_status = 'In compartment',
           updated_at = NOW()
     WHERE id = p_product_id;

    UPDATE public.paper_compartments
       SET assigned_product_id = p_product_id,
           current_pad_stock = 1,
           pad_sheets_loaded = p_sheets,
           current_pad_sheets_remaining = p_sheets,
           reserved_pad_sheets = 0,
           presence_status = 'HIGH',
           updated_at = NOW()
     WHERE id = v_bay.id
     RETURNING * INTO v_bay;
    v_event := CASE WHEN v_action = 'REPLACE' THEN 'REPLACE' ELSE 'LOAD' END;

    IF v_old_product.id IS NOT NULL AND v_old_product.id <> p_product_id
       AND NOT EXISTS (
         SELECT 1 FROM public.paper_compartments
          WHERE assigned_product_id = v_old_product.id
       ) THEN
      UPDATE public.paper_inventory
         SET location_status = CASE WHEN stock_pads > 0 THEN 'In stock' ELSE 'Out of stock' END,
             updated_at = NOW()
       WHERE id = v_old_product.id;
    END IF;
  END IF;

  INSERT INTO public.paper_pad_history (
    compartment_number, event_type, previous_product_id, product_id,
    previous_pad_count, previous_sheets, resulting_sheets, quantity_sheets,
    reason, performed_by
  ) VALUES (
    p_compartment_number, v_event, v_previous_product_id, v_bay.assigned_product_id,
    COALESCE(v_previous_pad_count, 0),
    v_previous_sheets,
    v_bay.current_pad_sheets_remaining,
    CASE
      WHEN v_action IN ('LOAD', 'REPLACE', 'COUNT_SET') THEN p_sheets
      WHEN v_action = 'REMOVE' THEN COALESCE(v_previous_sheets, 0)
      ELSE 0
    END,
    v_reason, p_performed_by
  );

  RETURN v_bay;
END;
$$;

REVOKE ALL ON FUNCTION public.admin_manage_paper_pad(INTEGER, TEXT, INTEGER, INTEGER, TEXT, TEXT) FROM PUBLIC, anon, authenticated;
GRANT EXECUTE ON FUNCTION public.admin_manage_paper_pad(INTEGER, TEXT, INTEGER, INTEGER, TEXT, TEXT) TO service_role;

CREATE OR REPLACE FUNCTION public.guard_paper_pad_reservation()
RETURNS TRIGGER
LANGUAGE plpgsql
SET search_path = public
AS $$
DECLARE
  v_bay public.paper_compartments%ROWTYPE;
BEGIN
  IF NEW.item_type <> 'paper' THEN RETURN NEW; END IF;

  SELECT * INTO v_bay
    FROM public.paper_compartments
   WHERE motor_channel = NEW.physical_channel
     AND assigned_product_id = NEW.product_id
   FOR UPDATE;
  IF NOT FOUND THEN
    RAISE EXCEPTION 'Paper product is not currently assigned to this bay.' USING ERRCODE = '23514';
  END IF;
  IF v_bay.current_pad_stock <= 0 THEN
    RAISE EXCEPTION 'Paper bay is reporting empty.' USING ERRCODE = '23514';
  END IF;
  IF v_bay.current_pad_stock <> 1 THEN
    RAISE EXCEPTION 'Paper bay must be reconciled to exactly one installed pad before checkout.' USING ERRCODE = '23514';
  END IF;
  IF v_bay.current_pad_sheets_remaining IS NULL THEN
    RAISE EXCEPTION 'The installed pad sheet count has not been confirmed by an administrator.' USING ERRCODE = '23514';
  END IF;
  IF v_bay.current_pad_sheets_remaining - v_bay.reserved_pad_sheets < NEW.qty_requested THEN
    RAISE EXCEPTION 'Insufficient sheets remain in the installed pad.' USING ERRCODE = '23514';
  END IF;

  UPDATE public.paper_compartments
     SET reserved_pad_sheets = reserved_pad_sheets + NEW.qty_requested,
         updated_at = NOW()
   WHERE id = v_bay.id;
  RETURN NEW;
END;
$$;

DROP TRIGGER IF EXISTS trg_guard_paper_pad_reservation ON public.sales_transaction_lines;
CREATE TRIGGER trg_guard_paper_pad_reservation
BEFORE INSERT ON public.sales_transaction_lines
FOR EACH ROW EXECUTE FUNCTION public.guard_paper_pad_reservation();

CREATE OR REPLACE FUNCTION public.apply_paper_pad_dispense()
RETURNS TRIGGER
LANGUAGE plpgsql
SET search_path = public
AS $$
DECLARE
  v_bay public.paper_compartments%ROWTYPE;
  v_remaining INTEGER;
BEGIN
  IF OLD.item_type <> 'paper'
     OR OLD.line_status <> 'RESERVED'
     OR NEW.line_status NOT IN ('DISPENSED', 'FAILED') THEN
    RETURN NEW;
  END IF;

  SELECT * INTO v_bay
    FROM public.paper_compartments
   WHERE motor_channel = NEW.physical_channel
     AND assigned_product_id = NEW.product_id
   FOR UPDATE;
  IF NOT FOUND THEN
    RAISE EXCEPTION 'Paper pad assignment changed before dispense completion.' USING ERRCODE = '23514';
  END IF;
  IF v_bay.current_pad_sheets_remaining IS NULL
     OR v_bay.current_pad_sheets_remaining < NEW.qty_dispensed THEN
    RAISE EXCEPTION 'Confirmed paper output exceeds the recorded remaining sheets.' USING ERRCODE = '23514';
  END IF;

  v_remaining := v_bay.current_pad_sheets_remaining - NEW.qty_dispensed;
  UPDATE public.paper_compartments
     SET current_pad_sheets_remaining = v_remaining,
         reserved_pad_sheets = GREATEST(0, reserved_pad_sheets - OLD.qty_requested),
         updated_at = NOW()
   WHERE id = v_bay.id;

  INSERT INTO public.paper_pad_history (
    compartment_number, event_type, previous_product_id, product_id,
    previous_pad_count, previous_sheets, resulting_sheets, quantity_sheets,
    transaction_id, performed_by
  ) VALUES (
    v_bay.compartment_number, 'DISPENSE', NEW.product_id, NEW.product_id,
    v_bay.current_pad_stock, v_bay.current_pad_sheets_remaining, v_remaining,
    NEW.qty_dispensed, NEW.transaction_id, 'MACHINE'
  );
  RETURN NEW;
END;
$$;

DROP TRIGGER IF EXISTS trg_apply_paper_pad_dispense ON public.sales_transaction_lines;
CREATE TRIGGER trg_apply_paper_pad_dispense
AFTER UPDATE OF line_status ON public.sales_transaction_lines
FOR EACH ROW EXECUTE FUNCTION public.apply_paper_pad_dispense();

CREATE OR REPLACE FUNCTION public.release_paper_pad_reservations_on_cancel()
RETURNS TRIGGER
LANGUAGE plpgsql
SET search_path = public
AS $$
BEGIN
  IF OLD.status IS DISTINCT FROM NEW.status
     AND NEW.status IN ('COMPLETED', 'PARTIAL_SUCCESS', 'FAILED_DISPENSE', 'COMPLETED_CHANGE_OWED', 'CANCELLED') THEN
    UPDATE public.paper_compartments bay
       SET reserved_pad_sheets = GREATEST(0, bay.reserved_pad_sheets - reserved.qty),
           updated_at = NOW()
      FROM (
        SELECT physical_channel, SUM(qty_requested)::INTEGER AS qty
          FROM public.sales_transaction_lines
         WHERE transaction_id = NEW.id
           AND item_type = 'paper'
           AND line_status = 'RESERVED'
         GROUP BY physical_channel
      ) reserved
     WHERE bay.motor_channel = reserved.physical_channel;
  END IF;
  RETURN NEW;
END;
$$;

DROP TRIGGER IF EXISTS trg_release_paper_pad_reservations_on_cancel ON public.sales_transactions;
CREATE TRIGGER trg_release_paper_pad_reservations_on_cancel
AFTER UPDATE OF status ON public.sales_transactions
FOR EACH ROW EXECUTE FUNCTION public.release_paper_pad_reservations_on_cancel();

