-- Dummy Data for Analytics Testing
-- This will populate your dashboard with fake sales to test charts and reports.

-- 1. Insert some dummy sales across different times of the day
INSERT INTO sales_transactions (item_type, brand_id, paper_size, amount_paid, qty_dispensed, transaction_date) VALUES
-- Morning Rush (8 AM - 10 AM)
('paper', 1, '1/4', 1.00, 4, NOW() - INTERVAL '1 day 16 hours'),
('paper', 1, '1/4', 1.00, 4, NOW() - INTERVAL '1 day 15 hours 30 minutes'),
('pen', 1, NULL, 5.00, 1, NOW() - INTERVAL '1 day 15 hours'),
('paper', 5, '1/4', 2.00, 4, NOW() - INTERVAL '1 day 14 hours'),

-- Lunch Time (12 PM - 1 PM)
('paper', 2, 'crosswise', 1.00, 3, NOW() - INTERVAL '1 day 12 hours'),
('paper', 6, 'crosswise', 2.00, 3, NOW() - INTERVAL '1 day 11 hours 45 minutes'),
('pen', 2, NULL, 10.00, 1, NOW() - INTERVAL '1 day 11 hours 30 minutes'),
('paper', 1, '1/4', 1.00, 4, NOW() - INTERVAL '1 day 11 hours'),

-- Afternoon Peak (2 PM - 4 PM)
('paper', 3, 'lengthwise', 1.00, 3, NOW() - INTERVAL '1 day 10 hours'),
('paper', 3, 'lengthwise', 1.00, 3, NOW() - INTERVAL '1 day 9 hours 45 minutes'),
('paper', 7, 'lengthwise', 2.00, 3, NOW() - INTERVAL '1 day 9 hours 30 minutes'),
('paper', 7, 'lengthwise', 2.00, 3, NOW() - INTERVAL '1 day 9 hours'),
('pen', 1, NULL, 5.00, 1, NOW() - INTERVAL '1 day 8 hours 30 minutes'),
('pen', 1, NULL, 5.00, 1, NOW() - INTERVAL '1 day 8 hours 15 minutes'),
('pen', 2, NULL, 10.00, 1, NOW() - INTERVAL '1 day 8 hours'),

-- Late Afternoon (5 PM - 6 PM)
('paper', 4, '1_whole', 1.00, 2, NOW() - INTERVAL '1 day 7 hours'),
('paper', 8, '1_whole', 2.00, 2, NOW() - INTERVAL '1 day 6 hours 30 minutes'),
('paper', 1, '1/4', 1.00, 4, NOW() - INTERVAL '1 day 6 hours'),

-- Today's Sales
('paper', 1, '1/4', 1.00, 4, NOW() - INTERVAL '4 hours'),
('pen', 2, NULL, 10.00, 1, NOW() - INTERVAL '3 hours'),
('paper', 5, '1/4', 2.00, 4, NOW() - INTERVAL '2 hours'),
('paper', 2, 'crosswise', 1.00, 3, NOW() - INTERVAL '1 hour');

-- 2. Update the machine status to show it is online
UPDATE machine_status SET status_value = 'Online', updated_at = NOW() WHERE status_key = 'is_running';
UPDATE machine_status SET status_value = 'Excellent (-45 dBm)', updated_at = NOW() WHERE status_key = 'wifi_signal';
