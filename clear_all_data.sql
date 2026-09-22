-- ============================================================================
-- Paper Vendo Machine - Clear All Application Data
-- ============================================================================
-- This clears rows only. Tables, columns, functions, triggers, and permissions
-- remain in place.
--
-- WARNING:
-- - This deletes transaction history, machine logs, inventory, administrators,
--   machine options, Wi-Fi configuration, and machine status data.
-- - Clearing admins will remove dashboard login accounts. Recreate an admin
--   account before logging in again.
-- - This does not delete Supabase Auth users outside the public.admins table.
--
-- Review the script before running it in Supabase SQL Editor.
-- ============================================================================

BEGIN;

TRUNCATE TABLE
    machine_network_config,
    machine_logs,
    sales_transaction_lines,
    sales_transactions,
    change_inventory,
    paper_compartments,
    paper_inventory,
    ballpen_compartments,
    ballpen_inventory,
    machine_options,
    machine_status,
    machine_online_status,
    admins
RESTART IDENTITY CASCADE;

COMMIT;

-- Optional default records can be recreated separately from the main schema.
-- Do not uncomment these unless you intentionally want fresh test accounts:
--
-- INSERT INTO admins (username, password, role)
-- VALUES
-- ('admin', crypt('admin123', gen_salt('bf')), 'superadmin'),
-- ('staff', crypt('staff123', gen_salt('bf')), 'staff');
