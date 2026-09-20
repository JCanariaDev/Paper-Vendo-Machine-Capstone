-- Safe migration for an existing database.
-- This does not drop transactions, inventory, or machine status data.

CREATE TABLE IF NOT EXISTS machine_options (
    id INTEGER PRIMARY KEY DEFAULT 1 CHECK (id = 1),
    minimum_credits INTEGER NOT NULL DEFAULT 1 CHECK (minimum_credits >= 0),
    maximum_credits INTEGER NOT NULL DEFAULT 30 CHECK (maximum_credits >= minimum_credits),
    minimum_ballpens_per_transaction INTEGER NOT NULL DEFAULT 1 CHECK (minimum_ballpens_per_transaction BETWEEN 1 AND 5),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_by INTEGER REFERENCES admins(id)
);

ALTER TABLE machine_options
  ADD COLUMN IF NOT EXISTS maximum_credits INTEGER NOT NULL DEFAULT 30;

ALTER TABLE machine_options
  ADD COLUMN IF NOT EXISTS minimum_ballpens_per_transaction INTEGER NOT NULL DEFAULT 1;

INSERT INTO machine_options (id, minimum_credits, maximum_credits, minimum_ballpens_per_transaction)
VALUES (1, 1, 30, 1)
ON CONFLICT (id) DO NOTHING;

GRANT SELECT, UPDATE ON machine_options TO anon, authenticated, service_role;
