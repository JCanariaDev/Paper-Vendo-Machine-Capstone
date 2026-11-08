-- Add nominal sheets-per-pad capacity without modifying existing inventory rows.
-- Existing capacities remain NULL until an administrator verifies them.

ALTER TABLE public.paper_inventory
  ADD COLUMN IF NOT EXISTS sheets_per_pad INTEGER;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
      FROM pg_constraint
     WHERE conrelid = 'public.paper_inventory'::regclass
       AND conname = 'paper_inventory_sheets_per_pad_check'
  ) THEN
    ALTER TABLE public.paper_inventory
      ADD CONSTRAINT paper_inventory_sheets_per_pad_check
      CHECK (sheets_per_pad IS NULL OR sheets_per_pad > 0);
  END IF;
END;
$$;

COMMENT ON COLUMN public.paper_inventory.sheets_per_pad IS
  'Nominal total sheet capacity of one physical pad; NULL means not yet verified.';
