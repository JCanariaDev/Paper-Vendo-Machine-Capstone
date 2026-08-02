-- Paper Vendo production schema
-- This is a clean-reset script. Run it only on a development database because it drops tables.

CREATE EXTENSION IF NOT EXISTS pgcrypto;

DROP TABLE IF EXISTS sales_transaction_lines;
DROP TABLE IF EXISTS sales_transactions;
DROP TABLE IF EXISTS change_inventory;
DROP TABLE IF EXISTS paper_channels;
DROP TABLE IF EXISTS paper_settings;
DROP TABLE IF EXISTS ballpen_settings;
DROP TABLE IF EXISTS machine_status;
DROP TABLE IF EXISTS admins;

CREATE TABLE admins (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'staff' CHECK (role IN ('superadmin', 'staff')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- One physical paper feeder per paper size. Two logical brands may share a channel
-- only when they are physically the same paper stock.
CREATE TABLE paper_channels (
    id SERIAL PRIMARY KEY,
    channel_code TEXT UNIQUE NOT NULL,
    paper_size TEXT UNIQUE NOT NULL,
    current_sheet_stock INTEGER NOT NULL DEFAULT 0 CHECK (current_sheet_stock >= 0),
    reserved_sheet_stock INTEGER NOT NULL DEFAULT 0 CHECK (reserved_sheet_stock >= 0),
    max_sheet_capacity INTEGER NOT NULL DEFAULT 500 CHECK (max_sheet_capacity > 0),
    motor_channel INTEGER NOT NULL UNIQUE CHECK (motor_channel BETWEEN 1 AND 4),
    sensor_channel INTEGER NOT NULL UNIQUE CHECK (sensor_channel BETWEEN 1 AND 4),
    physical_status TEXT NOT NULL DEFAULT 'Good',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE paper_settings (
    id SERIAL PRIMARY KEY,
    brand_name TEXT NOT NULL,
    paper_size TEXT NOT NULL,
    cost_per_unit_cents INTEGER NOT NULL CHECK (cost_per_unit_cents > 0),
    sheets_per_unit INTEGER NOT NULL CHECK (sheets_per_unit > 0),
    paper_channel_id INTEGER NOT NULL REFERENCES paper_channels(id),
    active BOOLEAN NOT NULL DEFAULT TRUE,
    UNIQUE (brand_name, paper_size)
);

CREATE TABLE ballpen_settings (
    id SERIAL PRIMARY KEY,
    item_name TEXT NOT NULL UNIQUE,
    cost_per_unit_cents INTEGER NOT NULL CHECK (cost_per_unit_cents > 0),
    current_stock INTEGER NOT NULL DEFAULT 0 CHECK (current_stock >= 0),
    reserved_stock INTEGER NOT NULL DEFAULT 0 CHECK (reserved_stock >= 0),
    max_capacity INTEGER NOT NULL DEFAULT 100 CHECK (max_capacity > 0),
    dispenser_channel INTEGER NOT NULL UNIQUE CHECK (dispenser_channel BETWEEN 1 AND 3),
    physical_status TEXT NOT NULL DEFAULT 'Good',
    active BOOLEAN NOT NULL DEFAULT TRUE,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- The initial design uses one peso coins in hopper channel 1. Add more rows/hoppers
-- when physical denomination modules are installed.
CREATE TABLE change_inventory (
    id SERIAL PRIMARY KEY,
    denomination_cents INTEGER NOT NULL UNIQUE CHECK (denomination_cents > 0),
    current_coin_count INTEGER NOT NULL DEFAULT 0 CHECK (current_coin_count >= 0),
    reserved_coin_count INTEGER NOT NULL DEFAULT 0 CHECK (reserved_coin_count >= 0),
    hopper_channel INTEGER NOT NULL UNIQUE CHECK (hopper_channel BETWEEN 1 AND 3),
    max_capacity INTEGER NOT NULL DEFAULT 200 CHECK (max_capacity > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE sales_transactions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    machine_id TEXT NOT NULL DEFAULT 'paper-vendo-01',
    status TEXT NOT NULL DEFAULT 'RESERVED'
      CHECK (status IN ('RESERVED', 'CHANGE_PAID', 'COMPLETED', 'CANCELLED', 'FAILED_CHANGE', 'FAILED_DISPENSE')),
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
    physical_channel INTEGER NOT NULL,
    units_requested INTEGER NOT NULL CHECK (units_requested > 0),
    unit_price_cents INTEGER NOT NULL CHECK (unit_price_cents > 0),
    sheets_per_unit_snapshot INTEGER NOT NULL DEFAULT 1 CHECK (sheets_per_unit_snapshot > 0),
    qty_requested INTEGER NOT NULL CHECK (qty_requested > 0),
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

CREATE INDEX idx_transaction_created_at ON sales_transactions(created_at DESC);
CREATE INDEX idx_transaction_lines_product ON sales_transaction_lines(item_type, product_id);

INSERT INTO admins (username, password, role) VALUES
('admin', crypt('admin123', gen_salt('bf')), 'superadmin'),
('staff', crypt('staff123', gen_salt('bf')), 'staff');

INSERT INTO paper_channels (channel_code, paper_size, current_sheet_stock, max_sheet_capacity, motor_channel, sensor_channel) VALUES
('paper-whole', '1_whole', 200, 500, 1, 1),
('paper-quarter', '1/4', 200, 500, 2, 2),
('paper-crosswise', 'crosswise', 200, 500, 3, 3),
('paper-lengthwise', 'lengthwise', 200, 500, 4, 4);

INSERT INTO paper_settings (brand_name, paper_size, cost_per_unit_cents, sheets_per_unit, paper_channel_id) VALUES
('Budget', '1/4', 100, 4, 2),
('Budget', 'crosswise', 100, 3, 3),
('Budget', 'lengthwise', 100, 3, 4),
('Budget', '1_whole', 100, 2, 1),
('Standard', '1/4', 200, 4, 2),
('Standard', 'crosswise', 200, 3, 3),
('Standard', 'lengthwise', 200, 3, 4),
('Standard', '1_whole', 200, 2, 1);

INSERT INTO ballpen_settings (item_name, cost_per_unit_cents, current_stock, max_capacity, dispenser_channel) VALUES
('Black Ballpen', 500, 50, 100, 1),
('Blue Ballpen', 500, 50, 100, 2),
('Red Ballpen', 500, 50, 100, 3);

INSERT INTO change_inventory (denomination_cents, current_coin_count, max_capacity, hopper_channel) VALUES
(100, 100, 200, 1);

INSERT INTO machine_status (status_key, status_value) VALUES
('is_running', 'Offline'),
('wifi_signal', 'Unknown'),
('last_transaction_status', 'None');

-- Atomically validate/reserve stock and exact change for a complete cart.
-- p_lines format: [{"item_type":"paper","product_id":1,"units":2}, ...]
CREATE OR REPLACE FUNCTION machine_reserve_transaction(p_credit_cents INTEGER, p_lines JSONB)
RETURNS TABLE(transaction_id UUID, subtotal_cents INTEGER, change_due_cents INTEGER, dispense_plan JSONB)
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
    v_available INTEGER;
    v_subtotal INTEGER := 0;
    v_change INTEGER;
    v_remaining INTEGER;
    v_coin RECORD;
    v_take INTEGER;
    v_change_plan JSONB := '[]'::jsonb;
    v_dispense_plan JSONB := '[]'::jsonb;
    v_tx UUID := gen_random_uuid();
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
            SELECT p.cost_per_unit_cents, p.sheets_per_unit, p.brand_name || ' ' || p.paper_size, p.paper_size,
                   c.id, c.current_sheet_stock - c.reserved_sheet_stock
              INTO v_price, v_sheets, v_name, v_size, v_channel, v_available
              FROM paper_settings p
              JOIN paper_channels c ON c.id = p.paper_channel_id
             WHERE p.id = v_product_id AND p.active
             FOR UPDATE OF c;
            IF NOT FOUND THEN RAISE EXCEPTION 'Paper product % is unavailable', v_product_id; END IF;
            v_qty := v_units * v_sheets;
            IF v_available < v_qty THEN RAISE EXCEPTION 'Insufficient paper stock for product %', v_product_id; END IF;
            -- Reserve while the row is locked.  This makes a mixed Budget /
            -- Standard cart that shares one physical feeder safe as well.
            UPDATE paper_channels SET reserved_sheet_stock = reserved_sheet_stock + v_qty, updated_at = NOW() WHERE id = v_channel;
        ELSE
            SELECT cost_per_unit_cents, 1, item_name, NULL::TEXT, dispenser_channel,
                   current_stock - reserved_stock
              INTO v_price, v_sheets, v_name, v_size, v_channel, v_available
              FROM ballpen_settings
             WHERE id = v_product_id AND active
             FOR UPDATE;
            IF NOT FOUND THEN RAISE EXCEPTION 'Pen product % is unavailable', v_product_id; END IF;
            v_qty := v_units;
            IF v_available < v_qty THEN RAISE EXCEPTION 'Insufficient pen stock for product %', v_product_id; END IF;
            UPDATE ballpen_settings SET reserved_stock = reserved_stock + v_qty, updated_at = NOW() WHERE id = v_product_id;
        END IF;
        v_subtotal := v_subtotal + (v_price * v_units);
    END LOOP;

    IF v_subtotal > p_credit_cents THEN RAISE EXCEPTION 'Insufficient credit'; END IF;
    v_change := p_credit_cents - v_subtotal;
    v_remaining := v_change;

    FOR v_coin IN SELECT * FROM change_inventory ORDER BY denomination_cents DESC FOR UPDATE LOOP
        v_take := LEAST(v_remaining / v_coin.denomination_cents, v_coin.current_coin_count - v_coin.reserved_coin_count);
        IF v_take > 0 THEN
            v_change_plan := v_change_plan || jsonb_build_array(jsonb_build_object('hopper_channel', v_coin.hopper_channel, 'denomination_cents', v_coin.denomination_cents, 'count', v_take));
            v_remaining := v_remaining - (v_take * v_coin.denomination_cents);
        END IF;
    END LOOP;
    IF v_remaining <> 0 THEN RAISE EXCEPTION 'Exact change is unavailable'; END IF;

    INSERT INTO sales_transactions (id, credit_received_cents, subtotal_cents, change_due_cents, change_plan)
    VALUES (v_tx, p_credit_cents, v_subtotal, v_change, v_change_plan);

    FOR v_line IN SELECT value FROM jsonb_array_elements(p_lines) LOOP
        v_type := v_line->>'item_type';
        v_product_id := (v_line->>'product_id')::INTEGER;
        v_units := (v_line->>'units')::INTEGER;
        IF v_type = 'paper' THEN
            SELECT p.cost_per_unit_cents, p.sheets_per_unit, p.brand_name || ' ' || p.paper_size, p.paper_size, p.paper_channel_id
              INTO v_price, v_sheets, v_name, v_size, v_channel FROM paper_settings p WHERE p.id = v_product_id;
            v_qty := v_units * v_sheets;
        ELSE
            SELECT cost_per_unit_cents, 1, item_name, NULL::TEXT, dispenser_channel
              INTO v_price, v_sheets, v_name, v_size, v_channel FROM ballpen_settings WHERE id = v_product_id;
            v_qty := v_units;
        END IF;
        INSERT INTO sales_transaction_lines (transaction_id, item_type, product_id, product_name, paper_size, physical_channel, units_requested, unit_price_cents, sheets_per_unit_snapshot, qty_requested)
        VALUES (v_tx, v_type, v_product_id, v_name, v_size, v_channel, v_units, v_price, v_sheets, v_qty);
        v_dispense_plan := v_dispense_plan || jsonb_build_array(jsonb_build_object('item_type', v_type, 'product_id', v_product_id, 'physical_channel', v_channel, 'qty_requested', v_qty));
    END LOOP;

    FOR v_coin IN SELECT * FROM jsonb_array_elements(v_change_plan) LOOP
        UPDATE change_inventory
           SET reserved_coin_count = reserved_coin_count + ((v_coin.value->>'count')::INTEGER), updated_at = NOW()
         WHERE hopper_channel = ((v_coin.value->>'hopper_channel')::INTEGER);
    END LOOP;

    RETURN QUERY SELECT v_tx, v_subtotal, v_change, v_dispense_plan;
END;
$$;

CREATE OR REPLACE FUNCTION machine_mark_change_paid(p_transaction_id UUID, p_change_paid_cents INTEGER)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE v_tx sales_transactions%ROWTYPE; v_coin JSONB;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND OR v_tx.status <> 'RESERVED' THEN RAISE EXCEPTION 'Transaction is not reserved'; END IF;
    IF p_change_paid_cents <> v_tx.change_due_cents THEN RAISE EXCEPTION 'Change confirmation does not match transaction'; END IF;
    FOR v_coin IN SELECT value FROM jsonb_array_elements(v_tx.change_plan) LOOP
        UPDATE change_inventory
           SET current_coin_count = current_coin_count - ((v_coin->>'count')::INTEGER),
               reserved_coin_count = reserved_coin_count - ((v_coin->>'count')::INTEGER), updated_at = NOW()
         WHERE hopper_channel = ((v_coin->>'hopper_channel')::INTEGER);
    END LOOP;
    UPDATE sales_transactions SET status = 'CHANGE_PAID', change_paid_cents = p_change_paid_cents WHERE id = p_transaction_id;
END;
$$;

CREATE OR REPLACE FUNCTION machine_cancel_reserved_transaction(p_transaction_id UUID, p_reason TEXT)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE v_line RECORD; v_coin JSONB; v_tx sales_transactions%ROWTYPE;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND OR v_tx.status <> 'RESERVED' THEN RAISE EXCEPTION 'Only reserved transactions can be cancelled'; END IF;
    FOR v_line IN SELECT * FROM sales_transaction_lines WHERE transaction_id = p_transaction_id LOOP
        IF v_line.item_type = 'paper' THEN
            UPDATE paper_channels SET reserved_sheet_stock = reserved_sheet_stock - v_line.qty_requested, updated_at = NOW() WHERE id = v_line.physical_channel;
        ELSE
            UPDATE ballpen_settings SET reserved_stock = reserved_stock - v_line.qty_requested, updated_at = NOW() WHERE id = v_line.product_id;
        END IF;
    END LOOP;
    FOR v_coin IN SELECT value FROM jsonb_array_elements(v_tx.change_plan) LOOP
        UPDATE change_inventory SET reserved_coin_count = reserved_coin_count - ((v_coin->>'count')::INTEGER), updated_at = NOW() WHERE hopper_channel = ((v_coin->>'hopper_channel')::INTEGER);
    END LOOP;
    UPDATE sales_transactions SET status = 'CANCELLED', failure_reason = p_reason, completed_at = NOW() WHERE id = p_transaction_id;
END;
$$;

CREATE OR REPLACE FUNCTION machine_finish_transaction(p_transaction_id UUID, p_results JSONB)
RETURNS TEXT LANGUAGE plpgsql AS $$
DECLARE v_tx sales_transactions%ROWTYPE; v_line RECORD; v_result JSONB; v_actual INTEGER; v_all_success BOOLEAN := TRUE;
BEGIN
    SELECT * INTO v_tx FROM sales_transactions WHERE id = p_transaction_id FOR UPDATE;
    IF NOT FOUND OR v_tx.status <> 'CHANGE_PAID' THEN RAISE EXCEPTION 'Transaction is not ready for completion'; END IF;
    FOR v_line IN SELECT * FROM sales_transaction_lines WHERE transaction_id = p_transaction_id FOR UPDATE LOOP
        SELECT value INTO v_result FROM jsonb_array_elements(p_results)
         WHERE (value->>'item_type') = v_line.item_type AND (value->>'product_id')::INTEGER = v_line.product_id;
        v_actual := COALESCE((v_result->>'qty_dispensed')::INTEGER, 0);
        IF v_actual > v_line.qty_requested THEN v_actual := v_line.qty_requested; END IF;
        UPDATE sales_transaction_lines SET qty_dispensed = v_actual, line_status = CASE WHEN v_actual = v_line.qty_requested THEN 'DISPENSED' ELSE 'FAILED' END WHERE id = v_line.id;
        IF v_line.item_type = 'paper' THEN
            UPDATE paper_channels SET current_sheet_stock = current_sheet_stock - v_actual, reserved_sheet_stock = reserved_sheet_stock - v_line.qty_requested, updated_at = NOW() WHERE id = v_line.physical_channel;
        ELSE
            UPDATE ballpen_settings SET current_stock = current_stock - v_actual, reserved_stock = reserved_stock - v_line.qty_requested, updated_at = NOW() WHERE id = v_line.product_id;
        END IF;
        IF v_actual <> v_line.qty_requested THEN v_all_success := FALSE; END IF;
    END LOOP;
    UPDATE sales_transactions SET status = CASE WHEN v_all_success THEN 'COMPLETED' ELSE 'FAILED_DISPENSE' END,
           failure_reason = CASE WHEN v_all_success THEN NULL ELSE 'Physical dispense sensor did not confirm all requested output' END,
           completed_at = NOW() WHERE id = p_transaction_id;
    RETURN CASE WHEN v_all_success THEN 'COMPLETED' ELSE 'FAILED_DISPENSE' END;
END;
$$;

GRANT EXECUTE ON FUNCTION machine_reserve_transaction(INTEGER, JSONB) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_mark_change_paid(UUID, INTEGER) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_cancel_reserved_transaction(UUID, TEXT) TO anon, authenticated;
GRANT EXECUTE ON FUNCTION machine_finish_transaction(UUID, JSONB) TO anon, authenticated;
