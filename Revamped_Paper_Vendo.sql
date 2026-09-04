-- ==============================================================================
-- Revamped Paper Vendo Production Database Schema
-- Separates Master Product Inventory (Tracked in PADs for paper, Pieces for pen)
-- from Physical Compartment Management (4 Paper Bays with L5290 Presence, 3 Pen Bays)
-- ==============================================================================

CREATE EXTENSION IF NOT EXISTS pgcrypto;

DROP TABLE IF EXISTS sales_transaction_lines CASCADE;
DROP TABLE IF EXISTS sales_transactions CASCADE;
DROP TABLE IF EXISTS change_inventory CASCADE;
DROP TABLE IF EXISTS paper_compartments CASCADE;
DROP TABLE IF EXISTS paper_inventory CASCADE;
DROP TABLE IF EXISTS ballpen_compartments CASCADE;
DROP TABLE IF EXISTS ballpen_inventory CASCADE;
DROP TABLE IF EXISTS machine_status CASCADE;
DROP TABLE IF EXISTS admins CASCADE;

-- Drop all existing RPC functions so signature changes are applied cleanly
DROP FUNCTION IF EXISTS machine_reserve_transaction(INTEGER, JSONB) CASCADE;
DROP FUNCTION IF EXISTS machine_finish_transaction(UUID, JSONB, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS machine_finish_transaction(UUID, JSONB) CASCADE;
DROP FUNCTION IF EXISTS machine_mark_change_paid(UUID, INTEGER) CASCADE;
DROP FUNCTION IF EXISTS machine_cancel_reserved_transaction(UUID, TEXT) CASCADE;
DROP FUNCTION IF EXISTS admin_reassign_paper_bay(INTEGER, INTEGER, INTEGER, TEXT) CASCADE;
DROP FUNCTION IF EXISTS admin_reassign_pen_bay(INTEGER, INTEGER, INTEGER, INTEGER) CASCADE;

-- ------------------------------------------------------------------------------
-- 1. Admins Table
-- ------------------------------------------------------------------------------
CREATE TABLE admins (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'staff' CHECK (role IN ('superadmin', 'staff')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ------------------------------------------------------------------------------
-- 2. Paper Master Inventory (All 8 Paper Types Tracked in Whole PADs)
-- ------------------------------------------------------------------------------
CREATE TABLE paper_inventory (
    id SERIAL PRIMARY KEY,
    brand_name TEXT NOT NULL,
    paper_size TEXT NOT NULL,
    cost_per_unit_cents INTEGER NOT NULL CHECK (cost_per_unit_cents > 0),
    sheets_per_unit INTEGER NOT NULL CHECK (sheets_per_unit > 0),
    stock_pads INTEGER NOT NULL DEFAULT 0 CHECK (stock_pads >= 0),
    location_status TEXT NOT NULL DEFAULT 'In stock' CHECK (location_status IN ('In compartment', 'In stock', 'Out of stock')),
    active BOOLEAN NOT NULL DEFAULT TRUE,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (brand_name, paper_size)
);

-- ------------------------------------------------------------------------------
-- 3. Paper Physical Compartments (Exactly 2 Hardware Bays with L5290 Sensors)
-- ------------------------------------------------------------------------------
CREATE TABLE paper_compartments (
    id SERIAL PRIMARY KEY,
    compartment_number INTEGER UNIQUE NOT NULL CHECK (compartment_number BETWEEN 1 AND 2),
    assigned_product_id INTEGER REFERENCES paper_inventory(id) ON DELETE SET NULL,
    current_pad_stock INTEGER NOT NULL DEFAULT 0 CHECK (current_pad_stock >= 0),
    presence_status TEXT NOT NULL DEFAULT 'HIGH' CHECK (presence_status IN ('HIGH', 'LOW')), -- HIGH: Paper Present, LOW: Empty
    motor_channel INTEGER NOT NULL UNIQUE CHECK (motor_channel BETWEEN 1 AND 2),
    sensor_channel INTEGER NOT NULL UNIQUE CHECK (sensor_channel BETWEEN 1 AND 2),
    physical_status TEXT NOT NULL DEFAULT 'Good',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ------------------------------------------------------------------------------
-- 4. Ballpen Master Inventory (All Pen Types Tracked in Storage Pieces)
-- ------------------------------------------------------------------------------
CREATE TABLE ballpen_inventory (
    id SERIAL PRIMARY KEY,
    item_name TEXT NOT NULL UNIQUE,
    cost_per_unit_cents INTEGER NOT NULL CHECK (cost_per_unit_cents > 0),
    storage_stock_pieces INTEGER NOT NULL DEFAULT 0 CHECK (storage_stock_pieces >= 0),
    location_status TEXT NOT NULL DEFAULT 'In compartment' CHECK (location_status IN ('In compartment', 'In stock', 'Out of stock')),
    active BOOLEAN NOT NULL DEFAULT TRUE,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ------------------------------------------------------------------------------
-- 5. Ballpen Physical Compartments (Exactly 1 Hardware Dispenser Bay)
-- ------------------------------------------------------------------------------
CREATE TABLE ballpen_compartments (
    id SERIAL PRIMARY KEY,
    compartment_number INTEGER UNIQUE NOT NULL CHECK (compartment_number = 1),
    assigned_product_id INTEGER REFERENCES ballpen_inventory(id) ON DELETE SET NULL,
    current_piece_stock INTEGER NOT NULL DEFAULT 0 CHECK (current_piece_stock >= 0),
    reserved_piece_stock INTEGER NOT NULL DEFAULT 0 CHECK (reserved_piece_stock >= 0),
    max_piece_capacity INTEGER NOT NULL DEFAULT 100 CHECK (max_piece_capacity > 0),
    dispenser_channel INTEGER NOT NULL UNIQUE CHECK (dispenser_channel = 1),
    physical_status TEXT NOT NULL DEFAULT 'Good',
    active BOOLEAN NOT NULL DEFAULT TRUE,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ------------------------------------------------------------------------------
-- 6. Coin Hopper Change Inventory
-- ------------------------------------------------------------------------------
CREATE TABLE change_inventory (
    id SERIAL PRIMARY KEY,
    denomination_cents INTEGER NOT NULL UNIQUE CHECK (denomination_cents > 0),
    current_coin_count INTEGER NOT NULL DEFAULT 0 CHECK (current_coin_count >= 0),
    reserved_coin_count INTEGER NOT NULL DEFAULT 0 CHECK (reserved_coin_count >= 0),
    hopper_channel INTEGER NOT NULL UNIQUE CHECK (hopper_channel BETWEEN 1 AND 3),
    max_capacity INTEGER NOT NULL DEFAULT 200 CHECK (max_capacity > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ------------------------------------------------------------------------------
-- 7. Sales Transactions & Transaction Lines
-- ------------------------------------------------------------------------------
CREATE TABLE sales_transactions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    transaction_number BIGSERIAL UNIQUE,
    tr_number TEXT GENERATED ALWAYS AS ('TR-' || LPAD(transaction_number::text, 5, '0')) STORED,
    machine_id TEXT NOT NULL DEFAULT 'paper-vendo-01',
    status TEXT NOT NULL DEFAULT 'RESERVED'
      CHECK (status IN ('RESERVED', 'CHANGE_PAID', 'COMPLETED', 'CANCELLED', 'FAILED_CHANGE', 'FAILED_DISPENSE', 'COMPLETED_CHANGE_OWED')),
    credit_received_cents INTEGER NOT NULL CHECK (credit_received_cents >= 0),
    subtotal_cents INTEGER NOT NULL CHECK (subtotal_cents >= 0),
    change_due_cents INTEGER NOT NULL CHECK (change_due_cents >= 0),
    change_paid_cents INTEGER NOT NULL DEFAULT 0 CHECK (change_paid_cents >= 0),
    change_plan JSONB NOT NULL DEFAULT '[]'::jsonb,
    failure_reason TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMPTZ
);

CREATE TABLE sales_transaction_lines (
    id BIGSERIAL PRIMARY KEY,
    transaction_id UUID NOT NULL REFERENCES sales_transactions(id) ON DELETE CASCADE,
    item_type TEXT NOT NULL CHECK (item_type IN ('paper', 'pen')),
    product_id INTEGER NOT NULL,
    product_name TEXT NOT NULL,
    paper_size TEXT,
    physical_channel INTEGER NOT NULL, -- Physical bay motor channel (1-2 for paper, 1 for pen)
    units_requested INTEGER NOT NULL CHECK (units_requested > 0),
    unit_price_cents INTEGER NOT NULL CHECK (unit_price_cents > 0),
    sheets_per_unit_snapshot INTEGER NOT NULL DEFAULT 1 CHECK (sheets_per_unit_snapshot > 0),
    qty_requested INTEGER NOT NULL CHECK (qty_requested > 0), -- Total sheets for paper, pieces for pen
    qty_dispensed INTEGER NOT NULL DEFAULT 0 CHECK (qty_dispensed >= 0),
    line_status TEXT NOT NULL DEFAULT 'RESERVED'
      CHECK (line_status IN ('RESERVED', 'DISPENSED', 'FAILED')),
    UNIQUE (transaction_id, item_type, product_id)
);

CREATE TABLE machine_status (
    id SERIAL PRIMARY KEY,
    status_key TEXT UNIQUE NOT NULL,
    status_value TEXT NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_revamped_tx_created ON sales_transactions(created_at DESC);
CREATE INDEX idx_revamped_tx_lines ON sales_transaction_lines(item_type, product_id);

-- ------------------------------------------------------------------------------
-- Seed Initial Data
-- ------------------------------------------------------------------------------
INSERT INTO admins (username, password, role) VALUES
('admin', crypt('admin123', gen_salt('bf')), 'superadmin'),
('staff', crypt('staff123', gen_salt('bf')), 'staff');

-- 8 Paper Products in Master Inventory
INSERT INTO paper_inventory (brand_name, paper_size, cost_per_unit_cents, sheets_per_unit, stock_pads, location_status) VALUES
('Standard', 'crosswise', 200, 3, 10, 'In compartment'),
('Budget', '1_whole', 100, 2, 8, 'In compartment'),
('Standard', '1/4', 200, 4, 12, 'In stock'),
('Budget', 'lengthwise', 100, 3, 7, 'In stock'),
('Budget', '1/4', 100, 4, 15, 'In stock'),
('Budget', 'crosswise', 100, 3, 15, 'In stock'),
('Standard', 'lengthwise', 200, 3, 10, 'In stock'),
('Standard', '1_whole', 200, 2, 10, 'In stock');

-- 2 Physical Paper Compartments
INSERT INTO paper_compartments (compartment_number, assigned_product_id, current_pad_stock, presence_status, motor_channel, sensor_channel) VALUES
(1, 1, 1, 'HIGH', 1, 1), -- Standard - Crosswise
(2, 2, 1, 'HIGH', 2, 2); -- Budget - 1 Whole

-- 3 Ballpen Products in Master Inventory
INSERT INTO ballpen_inventory (item_name, cost_per_unit_cents, storage_stock_pieces, location_status) VALUES
('Black Ballpen', 500, 100, 'In compartment'),
('Red Ballpen', 500, 100, 'In stock'),
('Blue Ballpen', 500, 100, 'In stock');

-- 1 Physical Ballpen Compartment
INSERT INTO ballpen_compartments (compartment_number, assigned_product_id, current_piece_stock, max_piece_capacity, dispenser_channel) VALUES
(1, 1, 50, 100, 1); -- Black Ballpen

-- Coin Hopper Change
INSERT INTO change_inventory (denomination_cents, current_coin_count, max_capacity, hopper_channel) VALUES
(100, 100, 200, 1);

-- Machine Health Status
INSERT INTO machine_status (status_key, status_value) VALUES
('is_running', 'Offline'),
('wifi_signal', 'Unknown'),
('last_transaction_status', 'None');

-- ------------------------------------------------------------------------------
-- Stored Procedures: Reservation, Change, Completion & Bay Management
-- ------------------------------------------------------------------------------

-- Reserve Transaction
CREATE OR REPLACE FUNCTION machine_reserve_transaction(p_credit_cents INTEGER, p_lines JSONB)
RETURNS TABLE(transaction_id UUID, tr_number TEXT, subtotal_cents INTEGER, change_due_cents INTEGER, dispense_plan JSONB)
LANGUAGE plpgsql
AS $$
DECLARE
    v_line JSONB;
    v_type TEXT;
    v_product_id INTEGER;
    v_units INTEGER;
    v_price INTEGER;
    v_sheets INTEGER;
    v_qty INTEGER;
    v_channel INTEGER;
    v_name TEXT;
    v_size TEXT;
    v_presence TEXT;
    v_available INTEGER;
    v_subtotal INTEGER := 0;
    v_change INTEGER;
    v_remaining INTEGER;
    v_coin RECORD;
    v_take INTEGER;
    v_change_plan JSONB := '[]'::jsonb;
    v_dispense_plan JSONB := '[]'::jsonb;
    v_tx UUID := gen_random_uuid();
    v_tr_number TEXT;
BEGIN
    IF p_credit_cents <= 0 OR jsonb_typeof(p_lines) <> 'array' OR jsonb_array_length(p_lines) = 0 THEN
        RAISE EXCEPTION 'A positive credit and at least one cart line are required';
    END IF;

    FOR v_line IN SELECT value FROM jsonb_array_elements(p_lines) LOOP
        v_type := v_line->>'item_type';
        v_product_id := (v_line->>'product_id')::INTEGER;
        v_units := (v_line->>'units')::INTEGER;
        IF v_type NOT IN ('paper', 'pen') OR v_product_id IS NULL OR v_units IS NULL OR v_units <= 0 THEN
            RAISE EXCEPTION 'Invalid cart line';
        END IF;

        IF v_type = 'paper' THEN
            -- Check if paper product is actively assigned to an active bay and L5290 sensor is HIGH (Paper Present)
            SELECT p.cost_per_unit_cents, p.sheets_per_unit, p.brand_name || ' ' || p.paper_size, p.paper_size,
                   c.motor_channel, c.presence_status
              INTO v_price, v_sheets, v_name, v_size, v_channel, v_presence
              FROM paper_inventory p
              JOIN paper_compartments c ON c.assigned_product_id = p.id
             WHERE p.id = v_product_id AND p.active
             FOR UPDATE OF c;
            
            IF NOT FOUND THEN 
                RAISE EXCEPTION 'Paper product % is not assigned to any physical compartment', v_product_id; 
            END IF;
            
            IF v_presence <> 'HIGH' THEN
                RAISE EXCEPTION 'Paper compartment for product % is currently empty', v_product_id;
            END IF;

            v_qty := v_units * v_sheets;
        ELSE
            -- Pen validation by exact piece count in compartment
            SELECT p.cost_per_unit_cents, 1, p.item_name, NULL::TEXT, c.dispenser_channel,
                   c.current_piece_stock - c.reserved_piece_stock
              INTO v_price, v_sheets, v_name, v_size, v_channel, v_available
              FROM ballpen_inventory p
              JOIN ballpen_compartments c ON c.assigned_product_id = p.id
             WHERE p.id = v_product_id AND p.active AND c.active
             FOR UPDATE OF c;

            IF NOT FOUND THEN 
                RAISE EXCEPTION 'Pen product % is not assigned to an active compartment', v_product_id; 
            END IF;

            v_qty := v_units;
            IF v_available < v_qty THEN 
                RAISE EXCEPTION 'Insufficient pen stock in compartment for product %', v_product_id; 
            END IF;

            UPDATE ballpen_compartments 
               SET reserved_piece_stock = reserved_piece_stock + v_qty, updated_at = NOW() 
             WHERE assigned_product_id = v_product_id;
        END IF;

        v_subtotal := v_subtotal + (v_price * v_units);
    END LOOP;

    IF v_subtotal > p_credit_cents THEN 
        RAISE EXCEPTION 'Insufficient credit'; 
    END IF;
    
    v_change := p_credit_cents - v_subtotal;
    v_remaining := v_change;

    -- Calculate Change from Coin Hopper if available (does not block purchase if hopper is short)
    FOR v_coin IN SELECT * FROM change_inventory ORDER BY denomination_cents DESC FOR UPDATE LOOP
        v_take := LEAST(v_remaining / v_coin.denomination_cents, v_coin.current_coin_count - v_coin.reserved_coin_count);
        IF v_take > 0 THEN
            v_change_plan := v_change_plan || jsonb_build_array(jsonb_build_object('hopper_channel', v_coin.hopper_channel, 'denomination_cents', v_coin.denomination_cents, 'count', v_take));
            v_remaining := v_remaining - (v_take * v_coin.denomination_cents);
        END IF;
    END LOOP;

    INSERT INTO sales_transactions (id, credit_received_cents, subtotal_cents, change_due_cents, change_plan)
    VALUES (v_tx, p_credit_cents, v_subtotal, v_change, v_change_plan)
    RETURNING sales_transactions.tr_number INTO v_tr_number;

    -- Record transaction lines
    FOR v_line IN SELECT value FROM jsonb_array_elements(p_lines) LOOP
        v_type := v_line->>'item_type';
        v_product_id := (v_line->>'product_id')::INTEGER;
        v_units := (v_line->>'units')::INTEGER;
        
        IF v_type = 'paper' THEN
            SELECT p.cost_per_unit_cents, p.sheets_per_unit, p.brand_name || ' ' || p.paper_size, p.paper_size, c.motor_channel
              INTO v_price, v_sheets, v_name, v_size, v_channel
              FROM paper_inventory p
              JOIN paper_compartments c ON c.assigned_product_id = p.id
             WHERE p.id = v_product_id;
            v_qty := v_units * v_sheets;
        ELSE
            SELECT p.cost_per_unit_cents, 1, p.item_name, NULL::TEXT, c.dispenser_channel
              INTO v_price, v_sheets, v_name, v_size, v_channel
              FROM ballpen_inventory p
              JOIN ballpen_compartments c ON c.assigned_product_id = p.id
             WHERE p.id = v_product_id;
            v_qty := v_units;
        END IF;

        INSERT INTO sales_transaction_lines (transaction_id, item_type, product_id, product_name, paper_size, physical_channel, units_requested, unit_price_cents, sheets_per_unit_snapshot, qty_requested)
        VALUES (v_tx, v_type, v_product_id, v_name, v_size, v_channel, v_units, v_price, v_sheets, v_qty);
        
        v_dispense_plan := v_dispense_plan || jsonb_build_array(jsonb_build_object('item_type', v_type, 'product_id', v_product_id, 'physical_channel', v_channel, 'qty_requested', v_qty));
    END LOOP;

    -- Reserve Coins in Hopper (if available)
    FOR v_coin IN SELECT * FROM jsonb_array_elements(v_change_plan) LOOP
        UPDATE change_inventory
           SET reserved_coin_count = reserved_coin_count + ((v_coin.value->>'count')::INTEGER), updated_at = NOW()
         WHERE hopper_channel = ((v_coin.value->>'hopper_channel')::INTEGER);
    END LOOP;

    RETURN QUERY SELECT v_tx, v_tr_number, v_subtotal, v_change, v_dispense_plan;
END;
$$;

-- Mark Change Paid
CREATE OR REPLACE FUNCTION machine_mark_change_paid(p_transaction_id UUID, p_change_paid_cents INTEGER)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE v_tx sales_transactions%ROWTYPE; v_coin JSONB;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND OR v_tx.status <> 'RESERVED' THEN RAISE EXCEPTION 'Transaction is not reserved'; END IF;
    FOR v_coin IN SELECT value FROM jsonb_array_elements(v_tx.change_plan) LOOP
        UPDATE change_inventory
           SET current_coin_count = GREATEST(0, current_coin_count - ((v_coin->>'count')::INTEGER)),
               reserved_coin_count = GREATEST(0, reserved_coin_count - ((v_coin->>'count')::INTEGER)), updated_at = NOW()
         WHERE hopper_channel = ((v_coin->>'hopper_channel')::INTEGER);
    END LOOP;
    UPDATE sales_transactions SET status = 'CHANGE_PAID', change_paid_cents = p_change_paid_cents WHERE id = p_transaction_id;
END;
$$;

-- Cancel Reserved Transaction
CREATE OR REPLACE FUNCTION machine_cancel_reserved_transaction(p_transaction_id UUID, p_reason TEXT)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE v_line RECORD; v_coin JSONB; v_tx sales_transactions%ROWTYPE;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND OR v_tx.status <> 'RESERVED' THEN RAISE EXCEPTION 'Only reserved transactions can be cancelled'; END IF;
    FOR v_line IN SELECT * FROM sales_transaction_lines WHERE transaction_id = p_transaction_id LOOP
        IF v_line.item_type = 'pen' THEN
            UPDATE ballpen_compartments 
               SET reserved_piece_stock = reserved_piece_stock - v_line.qty_requested, updated_at = NOW() 
             WHERE dispenser_channel = v_line.physical_channel;
        END IF;
    END LOOP;
    FOR v_coin IN SELECT value FROM jsonb_array_elements(v_tx.change_plan) LOOP
        UPDATE change_inventory SET reserved_coin_count = GREATEST(0, reserved_coin_count - ((v_coin->>'count')::INTEGER)), updated_at = NOW() WHERE hopper_channel = ((v_coin->>'hopper_channel')::INTEGER);
    END LOOP;
    UPDATE sales_transactions SET status = 'CANCELLED', failure_reason = p_reason, completed_at = NOW() WHERE id = p_transaction_id;
END;
$$;

-- Finish Transaction (Product-First with accurate Change Logging)
CREATE OR REPLACE FUNCTION machine_finish_transaction(
    p_transaction_id UUID, 
    p_results JSONB, 
    p_change_paid_cents INTEGER DEFAULT 0
)
RETURNS TABLE(tr_number TEXT, final_status TEXT, change_due_cents INTEGER, change_paid_cents INTEGER)
LANGUAGE plpgsql AS $$
DECLARE 
    v_tx sales_transactions%ROWTYPE; 
    v_line RECORD; 
    v_result JSONB; 
    v_actual INTEGER; 
    v_all_success BOOLEAN := TRUE;
    v_coin JSONB;
    v_paid INTEGER;
    v_final_status TEXT;
    v_reason TEXT := NULL;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND THEN RAISE EXCEPTION 'Transaction % not found', p_transaction_id; END IF;
    
    v_paid := COALESCE(p_change_paid_cents, 0);
    IF v_paid > v_tx.change_due_cents THEN v_paid := v_tx.change_due_cents; END IF;

    -- Deduct physical item inventory
    FOR v_line IN SELECT * FROM sales_transaction_lines WHERE transaction_id = p_transaction_id FOR UPDATE LOOP
        SELECT value INTO v_result FROM jsonb_array_elements(p_results)
         WHERE (value->>'item_type') = v_line.item_type AND (value->>'product_id')::INTEGER = v_line.product_id;
        v_actual := COALESCE((v_result->>'qty_dispensed')::INTEGER, 0);
        IF v_actual > v_line.qty_requested THEN v_actual := v_line.qty_requested; END IF;
        
        UPDATE sales_transaction_lines 
           SET qty_dispensed = v_actual, 
               line_status = CASE WHEN v_actual = v_line.qty_requested THEN 'DISPENSED' ELSE 'FAILED' END 
         WHERE id = v_line.id;
        
        IF v_line.item_type = 'pen' THEN
            UPDATE ballpen_compartments 
               SET current_piece_stock = current_piece_stock - v_actual, 
                   reserved_piece_stock = reserved_piece_stock - v_line.qty_requested, 
                   updated_at = NOW() 
             WHERE dispenser_channel = v_line.physical_channel;
        END IF;
        
        IF v_actual <> v_line.qty_requested THEN v_all_success := FALSE; END IF;
    END LOOP;

    -- Adjust change inventory (deduct actually released coins, free unused reservations)
    FOR v_coin IN SELECT value FROM jsonb_array_elements(v_tx.change_plan) LOOP
        UPDATE change_inventory 
           SET reserved_coin_count = GREATEST(0, reserved_coin_count - ((v_coin->>'count')::INTEGER)),
               current_coin_count = GREATEST(0, current_coin_count - LEAST(v_paid / ((v_coin->>'denomination_cents')::INTEGER), (v_coin->>'count')::INTEGER)),
               updated_at = NOW() 
         WHERE hopper_channel = ((v_coin->>'hopper_channel')::INTEGER);
    END LOOP;

    IF NOT v_all_success THEN
        v_final_status := 'FAILED_DISPENSE';
        v_reason := 'Physical dispense sensor did not confirm all requested output';
    ELSIF v_paid < v_tx.change_due_cents THEN
        v_final_status := 'COMPLETED_CHANGE_OWED';
        v_reason := 'Unreleased change of PHP ' || TO_CHAR((v_tx.change_due_cents - v_paid) / 100.0, 'FM999,990.00') || ' owed to student';
    ELSE
        v_final_status := 'COMPLETED';
    END IF;

    UPDATE sales_transactions 
       SET status = v_final_status,
           change_paid_cents = v_paid,
           failure_reason = v_reason,
           completed_at = NOW() 
     WHERE id = p_transaction_id;
     
    RETURN QUERY SELECT v_tx.tr_number, v_final_status, v_tx.change_due_cents, v_paid;
END;
$$;

-- ------------------------------------------------------------------------------
-- Reassign / Refill Paper Compartment
-- Deducts PADs from main inventory stock and assigns product to physical bay
-- ------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION admin_reassign_paper_bay(
    p_compartment_number INTEGER,
    p_new_product_id INTEGER,
    p_pads_refilled INTEGER DEFAULT 1,
    p_presence_status TEXT DEFAULT 'HIGH'
)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE
    v_old_product_id INTEGER;
    v_old_presence TEXT;
    v_current_pads INTEGER;
    v_available_pads INTEGER;
    v_is_reassign BOOLEAN;
    v_new_pad_stock INTEGER;
BEGIN
    SELECT assigned_product_id, presence_status, current_pad_stock
      INTO v_old_product_id, v_old_presence, v_current_pads
      FROM paper_compartments
     WHERE compartment_number = p_compartment_number
     FOR UPDATE;
     
    IF NOT FOUND THEN RAISE EXCEPTION 'Compartment % does not exist', p_compartment_number; END IF;

    -- Handle existing rows where current_pad_stock might be NULL
    v_current_pads := COALESCE(v_current_pads, CASE WHEN v_old_presence = 'HIGH' THEN 1 ELSE 0 END);
    v_is_reassign := (v_old_product_id IS NOT NULL AND v_old_product_id <> p_new_product_id);

    -- 1. If REASSIGNING to a different product:
    IF v_is_reassign THEN
        -- Return all N pads currently in the bay back to the old product's master storage!
        IF v_current_pads > 0 THEN
            UPDATE paper_inventory
               SET stock_pads = stock_pads + v_current_pads,
                   updated_at = NOW()
             WHERE id = v_old_product_id;
        END IF;

        -- If the old product is not assigned to any other paper compartment, update its location_status
        IF NOT EXISTS (SELECT 1 FROM paper_compartments WHERE assigned_product_id = v_old_product_id AND compartment_number <> p_compartment_number) THEN
            UPDATE paper_inventory
               SET location_status = CASE WHEN stock_pads > 0 THEN 'In stock' ELSE 'Out of stock' END,
                   updated_at = NOW()
             WHERE id = v_old_product_id;
        END IF;
    END IF;

    -- 2. Validate and process new product loading/refill:
    IF p_new_product_id IS NOT NULL THEN
        SELECT stock_pads INTO v_available_pads
          FROM paper_inventory
         WHERE id = p_new_product_id
         FOR UPDATE;
         
        IF NOT FOUND THEN RAISE EXCEPTION 'Paper product % not found', p_new_product_id; END IF;
        
        IF p_pads_refilled > 0 THEN
            IF v_available_pads < p_pads_refilled THEN
                RAISE EXCEPTION 'Insufficient stock pads in storage (% available, % requested)', v_available_pads, p_pads_refilled;
            END IF;
            -- Deduct refilled pads from master storage
            UPDATE paper_inventory
               SET stock_pads = stock_pads - p_pads_refilled,
                   location_status = 'In compartment',
                   updated_at = NOW()
             WHERE id = p_new_product_id;
        ELSE
            UPDATE paper_inventory
               SET location_status = 'In compartment',
                   updated_at = NOW()
             WHERE id = p_new_product_id;
        END IF;
    END IF;

    -- 3. Calculate resulting bay pad stock:
    -- If reassigned: bay base stock resets to 0, so stock is exactly p_pads_refilled.
    -- If same product refill: adds refilled pads to existing bay pads.
    IF v_is_reassign THEN
        v_new_pad_stock := p_pads_refilled;
    ELSE
        v_new_pad_stock := v_current_pads + p_pads_refilled;
    END IF;

    -- If explicitly marked LOW and 0 pads refilled, stock is 0
    IF p_presence_status = 'LOW' AND p_pads_refilled = 0 THEN
        v_new_pad_stock := 0;
    END IF;

    -- 4. Update compartment state
    UPDATE paper_compartments
       SET assigned_product_id = p_new_product_id,
           current_pad_stock = v_new_pad_stock,
           presence_status = CASE WHEN v_new_pad_stock > 0 THEN 'HIGH' ELSE p_presence_status END,
           updated_at = NOW()
     WHERE compartment_number = p_compartment_number;
END;
$$;

-- Trigger to keep presence_status and current_pad_stock in sync
CREATE OR REPLACE FUNCTION trg_sync_paper_compartment_presence()
RETURNS TRIGGER LANGUAGE plpgsql AS $$
BEGIN
    IF NEW.presence_status = 'LOW' AND OLD.presence_status = 'HIGH' AND NEW.current_pad_stock = OLD.current_pad_stock THEN
        -- Sensor detected tray empty (L5290 LOW)
        NEW.current_pad_stock := 0;
    ELSIF NEW.current_pad_stock > 0 AND NEW.presence_status = 'LOW' THEN
        NEW.presence_status := 'HIGH';
    END IF;
    RETURN NEW;
END;
$$;

DROP TRIGGER IF EXISTS trg_paper_compartment_presence ON paper_compartments;
CREATE TRIGGER trg_paper_compartment_presence
BEFORE UPDATE ON paper_compartments
FOR EACH ROW
EXECUTE FUNCTION trg_sync_paper_compartment_presence();

-- ------------------------------------------------------------------------------
-- Reassign / Refill Ballpen Compartment
-- Deducts pieces from main storage and adds them to the physical bay stock
-- ------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION admin_reassign_pen_bay(
    p_compartment_number INTEGER,
    p_new_product_id INTEGER,
    p_pieces_refilled INTEGER DEFAULT 0,
    p_direct_stock INTEGER DEFAULT NULL
)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE
    v_old_product_id INTEGER;
    v_available_pieces INTEGER;
    v_current_stock INTEGER;
    v_max_capacity INTEGER;
    v_new_stock INTEGER;
    v_is_reassign BOOLEAN;
BEGIN
    SELECT assigned_product_id, current_piece_stock, max_piece_capacity
      INTO v_old_product_id, v_current_stock, v_max_capacity
      FROM ballpen_compartments
     WHERE compartment_number = p_compartment_number
     FOR UPDATE;
     
    IF NOT FOUND THEN RAISE EXCEPTION 'Ballpen compartment % does not exist', p_compartment_number; END IF;

    v_is_reassign := (v_old_product_id IS NOT NULL AND v_old_product_id <> p_new_product_id);

    -- 1. If REASSIGNING to a different product:
    -- Return any pens physically in the bay back to the old product's storage stock!
    IF v_is_reassign THEN
        IF v_current_stock > 0 THEN
            UPDATE ballpen_inventory
               SET storage_stock_pieces = storage_stock_pieces + v_current_stock,
                   updated_at = NOW()
             WHERE id = v_old_product_id;
        END IF;

        -- If the old product is not assigned to any other compartment, update its location_status
        IF NOT EXISTS (SELECT 1 FROM ballpen_compartments WHERE assigned_product_id = v_old_product_id AND compartment_number <> p_compartment_number) THEN
            UPDATE ballpen_inventory
               SET location_status = 'In stock', updated_at = NOW()
             WHERE id = v_old_product_id;
        END IF;

        -- For the newly assigned product, the bay base stock starts clean at 0
        v_current_stock := 0;
    END IF;

    -- 2. Handle loading stock for the newly assigned or existing product
    IF p_new_product_id IS NOT NULL THEN
        SELECT storage_stock_pieces INTO v_available_pieces
          FROM ballpen_inventory
         WHERE id = p_new_product_id
         FOR UPDATE;
         
        IF NOT FOUND THEN RAISE EXCEPTION 'Ballpen product % not found', p_new_product_id; END IF;
        
        IF p_pieces_refilled > 0 THEN
            IF v_available_pieces < p_pieces_refilled THEN
                RAISE EXCEPTION 'Insufficient storage pieces (% available, % requested)', v_available_pieces, p_pieces_refilled;
            END IF;
            -- Deduct refilled pieces from master storage
            UPDATE ballpen_inventory
               SET storage_stock_pieces = storage_stock_pieces - p_pieces_refilled,
                   location_status = 'In compartment',
                   updated_at = NOW()
             WHERE id = p_new_product_id;
             
            -- If reassigned: 0 + p_pieces_refilled = p_pieces_refilled.
            -- If same product refill: v_current_stock + p_pieces_refilled.
            v_new_stock := LEAST(v_current_stock + p_pieces_refilled, v_max_capacity);
        ELSE
            UPDATE ballpen_inventory
               SET location_status = 'In compartment',
                   updated_at = NOW()
             WHERE id = p_new_product_id;
             
            IF p_direct_stock IS NOT NULL THEN
                v_new_stock := LEAST(p_direct_stock, v_max_capacity);
            ELSE
                v_new_stock := v_current_stock;
            END IF;
        END IF;
    ELSE
        v_new_stock := 0;
    END IF;

    -- 3. Update compartment state
    UPDATE ballpen_compartments
       SET assigned_product_id = p_new_product_id,
           current_piece_stock = v_new_stock,
           updated_at = NOW()
     WHERE compartment_number = p_compartment_number;
END;
$$;

GRANT EXECUTE ON FUNCTION machine_reserve_transaction(INTEGER, JSONB) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_mark_change_paid(UUID, INTEGER) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_cancel_reserved_transaction(UUID, TEXT) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_finish_transaction(UUID, JSONB, INTEGER) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION admin_reassign_paper_bay(INTEGER, INTEGER, INTEGER, TEXT) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION admin_reassign_pen_bay(INTEGER, INTEGER, INTEGER, INTEGER) TO anon, authenticated;
