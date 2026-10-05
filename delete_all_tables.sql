-- ==============================================================================
-- Paper Vendo Machine Capstone - Database Teardown Script
-- Description: Completely removes all tables, functions, and triggers created 
--              for the Paper Vendo Machine system from the Supabase public schema.
-- ==============================================================================

-- 1. Drop Tables (CASCADE removes all foreign keys, triggers, and dependent objects)
DROP TABLE IF EXISTS machine_network_config CASCADE;
DROP TABLE IF EXISTS sales_transaction_lines CASCADE;
DROP TABLE IF EXISTS sales_transactions CASCADE;
DROP TABLE IF EXISTS change_inventory CASCADE;
DROP TABLE IF EXISTS paper_compartments CASCADE;
DROP TABLE IF EXISTS paper_inventory CASCADE;
DROP TABLE IF EXISTS ballpen_compartments CASCADE;
DROP TABLE IF EXISTS ballpen_inventory CASCADE;
DROP TABLE IF EXISTS machine_status CASCADE;
DROP TABLE IF EXISTS machine_online_status CASCADE;
DROP TABLE IF EXISTS machine_options CASCADE;
DROP TABLE IF EXISTS machine_logs CASCADE;
DROP TABLE IF EXISTS inventory_refill_history CASCADE;
DROP TABLE IF EXISTS admins CASCADE;

-- Legacy table cleanup (if created in earlier versions)
DROP TABLE IF EXISTS paper_channels CASCADE;
DROP TABLE IF EXISTS paper_settings CASCADE;
DROP TABLE IF EXISTS ballpen_settings CASCADE;

-- 2. Explicitly drop known stored procedures and RPC functions
DROP FUNCTION IF EXISTS machine_reserve_transaction(INTEGER, JSONB) CASCADE;
DROP FUNCTION IF EXISTS machine_finish_transaction(UUID, JSONB, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS machine_finish_transaction(UUID, JSONB) CASCADE;
DROP FUNCTION IF EXISTS machine_mark_change_paid(UUID, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS machine_release_change(UUID) CASCADE;
DROP FUNCTION IF EXISTS machine_record_failed_dispense_refund(UUID) CASCADE;
DROP FUNCTION IF EXISTS machine_cancel_reserved_transaction(UUID, TEXT) CASCADE;
DROP FUNCTION IF EXISTS machine_recover_interrupted_reservations(TEXT) CASCADE;
DROP FUNCTION IF EXISTS machine_recover_interrupted_reservations() CASCADE;
DROP FUNCTION IF EXISTS admin_reassign_paper_bay(INTEGER, INTEGER, INTEGER, TEXT) CASCADE;
DROP FUNCTION IF EXISTS admin_reassign_pen_bay(INTEGER, INTEGER, INTEGER, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS update_machine_online_heartbeat() CASCADE;
DROP FUNCTION IF EXISTS log_sales_transaction_event() CASCADE;
DROP FUNCTION IF EXISTS trg_sync_paper_compartment_presence() CASCADE;

-- 3. Dynamic cleanup: Drop ALL overloads of vendo functions regardless of signature
DO $$
DECLARE
    r RECORD;
BEGIN
    FOR r IN (
        SELECT p.oid::regprocedure::text AS func_sig
        FROM pg_proc p
        JOIN pg_namespace n ON p.pronamespace = n.oid
        WHERE n.nspname = 'public'
          AND p.proname IN (
              'machine_reserve_transaction',
              'machine_finish_transaction',
              'machine_mark_change_paid',
              'machine_release_change',
              'machine_record_failed_dispense_refund',
              'machine_cancel_reserved_transaction',
              'machine_recover_interrupted_reservations',
              'admin_reassign_paper_bay',
              'admin_reassign_pen_bay',
              'update_machine_online_heartbeat',
              'log_sales_transaction_event',
              'trg_sync_paper_compartment_presence'
          )
    ) LOOP
        EXECUTE 'DROP FUNCTION IF EXISTS ' || r.func_sig || ' CASCADE';
    END LOOP;
END $$;

-- Confirmation notice
DO $$
BEGIN
    RAISE NOTICE 'All Paper Vendo Machine tables, views, and functions have been dropped successfully.';
END $$;
