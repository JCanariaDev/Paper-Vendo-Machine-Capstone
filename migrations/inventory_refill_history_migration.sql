-- Run this once on an existing database to add refill history support.
CREATE TABLE IF NOT EXISTS inventory_refill_history (
    id BIGSERIAL PRIMARY KEY,
    item_type TEXT NOT NULL CHECK (item_type IN ('paper', 'pen')),
    compartment_number INTEGER NOT NULL CHECK (compartment_number > 0),
    product_id INTEGER,
    product_name TEXT NOT NULL,
    operation TEXT NOT NULL DEFAULT 'REFILL' CHECK (operation IN ('REFILL', 'ADJUSTMENT', 'REASSIGNMENT')),
    quantity_added INTEGER NOT NULL DEFAULT 0 CHECK (quantity_added >= 0),
    quantity_unit TEXT NOT NULL CHECK (quantity_unit IN ('pads', 'pieces')),
    previous_compartment_stock INTEGER NOT NULL DEFAULT 0 CHECK (previous_compartment_stock >= 0),
    resulting_compartment_stock INTEGER NOT NULL DEFAULT 0 CHECK (resulting_compartment_stock >= 0),
    performed_by TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_inventory_refill_history_created
  ON inventory_refill_history(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_inventory_refill_history_type
  ON inventory_refill_history(item_type);
GRANT SELECT ON inventory_refill_history TO anon, authenticated, service_role;
