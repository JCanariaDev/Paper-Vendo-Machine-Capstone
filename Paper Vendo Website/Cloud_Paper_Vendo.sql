-- NUKE EVERYTHING (For a clean reset)
DROP TRIGGER IF EXISTS trg_deduct_inventory ON sales_transactions;
DROP FUNCTION IF EXISTS deduct_inventory_stock();
DROP TABLE IF EXISTS sales_transactions;
DROP TABLE IF EXISTS paper_settings;
DROP TABLE IF EXISTS ballpen_settings;
DROP TABLE IF EXISTS realtime_status;
DROP TABLE IF EXISTS machine_status;
DROP TABLE IF EXISTS admins;


-- 1. Create Admins Table
CREATE TABLE IF NOT EXISTS admins (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL,
    role TEXT DEFAULT 'staff',
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- 2. Create Paper Settings Table
CREATE TABLE IF NOT EXISTS paper_settings (
    id SERIAL PRIMARY KEY,
    brand_name TEXT NOT NULL,
    paper_size TEXT NOT NULL,
    cost_per_unit DECIMAL(10,2) NOT NULL DEFAULT 1.00,
    sheets_per_unit INTEGER NOT NULL DEFAULT 1,
    current_stock INTEGER DEFAULT 0,
    max_capacity INTEGER DEFAULT 500,
    physical_status TEXT DEFAULT 'Good',
    image TEXT
);

-- 3. Create Ballpen Settings Table
CREATE TABLE IF NOT EXISTS ballpen_settings (
    id SERIAL PRIMARY KEY,
    item_name TEXT NOT NULL DEFAULT 'Ballpen',
    cost_per_unit DECIMAL(10,2) NOT NULL DEFAULT 10.00,
    current_stock INTEGER DEFAULT 0,
    max_capacity INTEGER DEFAULT 100,
    physical_status TEXT DEFAULT 'Good'
);

-- 4. Create Sales Transactions Table
CREATE TABLE IF NOT EXISTS sales_transactions (
    id SERIAL PRIMARY KEY,
    item_type TEXT NOT NULL, -- 'paper' or 'ballpen'
    brand_id INTEGER REFERENCES paper_settings(id),
    paper_size TEXT,
    amount_paid DECIMAL(10,2) NOT NULL,
    qty_dispensed INTEGER NOT NULL,
    transaction_date TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- 5. Create Machine Status Table
CREATE TABLE IF NOT EXISTS machine_status (
    id SERIAL PRIMARY KEY,
    status_key TEXT UNIQUE NOT NULL,
    status_value TEXT NOT NULL,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- 5.1. Create Realtime Status Table (Single row representing the machine state)
CREATE TABLE IF NOT EXISTS realtime_status (
    id INTEGER PRIMARY KEY DEFAULT 1,
    coins_inserted DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    credits_remaining DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    selected_type TEXT DEFAULT 'None', -- 'paper', 'pen', 'None'
    selected_brand TEXT DEFAULT 'None',
    selected_size TEXT DEFAULT 'None',
    oled_display_text TEXT NOT NULL DEFAULT 'Smart Vendo V3\nInsert Coin',
    scale_weight_grams DECIMAL(10,2) DEFAULT 0.00,
    ir_sensor_blocked BOOLEAN DEFAULT FALSE,
    stepper_position_steps INTEGER DEFAULT 0,
    servo_angle_change INTEGER DEFAULT 0,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- 6. Insert Original Paper Vendo Data (Legacy Sync)
INSERT INTO admins (username, password, role) VALUES 
('admin', 'admin123', 'superadmin'),
('staff', 'staff123', 'staff')
ON CONFLICT (username) DO NOTHING;

INSERT INTO realtime_status (id, coins_inserted, credits_remaining, selected_type, selected_brand, selected_size, oled_display_text, scale_weight_grams, ir_sensor_blocked, stepper_position_steps, servo_angle_change) 
VALUES (1, 0.00, 0.00, 'None', 'None', 'None', 'Smart Vendo V3\nInsert Coin', 0.00, FALSE, 0, 0)
ON CONFLICT (id) DO NOTHING;


INSERT INTO paper_settings (brand_name, paper_size, cost_per_unit, sheets_per_unit, current_stock, max_capacity) VALUES
('Budget Brand (White)', '1/4',       1.00, 4, 100, 500),
('Budget Brand (White)', 'crosswise', 1.00, 3, 100, 500),
('Budget Brand (White)', 'lengthwise',1.00, 3, 100, 500),
('Budget Brand (White)', '1_whole',   1.00, 2, 100, 500),
('Standard Brand (Yellow)', '1/4',       2.00, 4, 100, 500),
('Standard Brand (Yellow)', 'crosswise', 2.00, 3, 100, 500),
('Standard Brand (Yellow)', 'lengthwise',2.00, 3, 100, 500),
('Standard Brand (Yellow)', '1_whole',   2.00, 2, 100, 500);

INSERT INTO ballpen_settings (item_name, cost_per_unit, current_stock, max_capacity) VALUES
('Budget Ballpen', 5.00, 50, 100),
('Standard Ballpen', 10.00, 50, 100);

INSERT INTO machine_status (status_key, status_value) VALUES 
('is_running', 'Connected'),
('wifi_signal', 'Excellent')
ON CONFLICT (status_key) DO NOTHING;

-- Enable Realtime for Cloud Dashboard (Run only once, uncomment if needed)
-- ALTER PUBLICATION supabase_realtime ADD TABLE paper_settings;
-- ALTER PUBLICATION supabase_realtime ADD TABLE ballpen_settings;
-- ALTER PUBLICATION supabase_realtime ADD TABLE machine_status;
-- ALTER PUBLICATION supabase_realtime ADD TABLE realtime_status;
-- ALTER PUBLICATION supabase_realtime ADD TABLE sales_transactions;


-- 7. Triggers for Auto-Decrementing Stock
-- Function to subtract stock automatically
CREATE OR REPLACE FUNCTION deduct_inventory_stock()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.item_type = 'paper' THEN
        UPDATE paper_settings 
        SET current_stock = GREATEST(current_stock - NEW.qty_dispensed, 0)
        WHERE id = NEW.brand_id AND paper_size = NEW.paper_size;
    ELSIF NEW.item_type = 'pen' THEN
        UPDATE ballpen_settings
        SET current_stock = GREATEST(current_stock - NEW.qty_dispensed, 0)
        WHERE id = NEW.brand_id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger to fire the function after inserting a sale
DROP TRIGGER IF EXISTS trg_deduct_inventory ON sales_transactions;
CREATE TRIGGER trg_deduct_inventory
AFTER INSERT ON sales_transactions
FOR EACH ROW
EXECUTE FUNCTION deduct_inventory_stock();
