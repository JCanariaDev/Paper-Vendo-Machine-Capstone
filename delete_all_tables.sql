-- ==============================================================================
-- Paper Vendo Machine Capstone - Database Teardown Script
-- Description: Completely removes all tables, functions, and triggers created 
--              for the Paper Vendo Machine system from the Supabase public schema.
-- ==============================================================================

-- 1. Drop Tables (CASCADE removes all foreign keys, triggers, and dependent objects)
DROP TABLE IF EXISTS sales_transaction_lines CASCADE;
DROP TABLE IF EXISTS sales_transactions CASCADE;
DROP TABLE IF EXISTS change_inventory CASCADE;
DROP TABLE IF EXISTS paper_compartments CASCADE;
DROP TABLE IF EXISTS paper_inventory CASCADE;
DROP TABLE IF EXISTS ballpen_compartments CASCADE;
DROP TABLE IF EXISTS ballpen_inventory CASCADE;
DROP TABLE IF EXISTS machine_status CASCADE;
DROP TABLE IF EXISTS machine_online_status CASCADE;
DROP TABLE IF EXISTS machine_logs CASCADE;
DROP TABLE IF EXISTS machine_network_config CASCADE;
DROP TABLE IF EXISTS admins CASCADE;

-- 2. Drop Stored Procedures and RPC Functions
DROP FUNCTION IF EXISTS machine_reserve_transaction(INTEGER, JSONB) CASCADE;
DROP FUNCTION IF EXISTS machine_finish_transaction(UUID, JSONB, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS machine_finish_transaction(UUID, JSONB) CASCADE;
DROP FUNCTION IF EXISTS machine_mark_change_paid(UUID, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS machine_cancel_reserved_transaction(UUID, TEXT) CASCADE;
DROP FUNCTION IF EXISTS admin_reassign_paper_bay(INTEGER, INTEGER, INTEGER, TEXT) CASCADE;
DROP FUNCTION IF EXISTS admin_reassign_pen_bay(INTEGER, INTEGER, INTEGER, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS update_machine_online_heartbeat() CASCADE;

-- Confirmation notice
DO $$
BEGIN
    RAISE NOTICE 'All Paper Vendo Machine tables, views, and functions have been dropped successfully.';
END $$;
