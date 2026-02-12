-- Database: paper_vendo_db

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+08:00";

-- --------------------------------------------------------

--
-- Table structure for table `admins`
--

CREATE TABLE `admins` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `username` varchar(50) NOT NULL,
  `password` varchar(255) NOT NULL, -- Store hashed passwords
  `role` enum('superadmin','staff') DEFAULT 'staff',
  `created_at` timestamp DEFAULT current_timestamp(),
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `admins` (`username`, `password`, `role`) VALUES
('admin', 'admin123', 'superadmin'), -- Plain text for demo simplicity
('staff', 'staff123', 'staff');

-- --------------------------------------------------------

--
-- Table structure for table `paper_settings`
-- Stores the configuration for each brand (price and sheets)
--

CREATE TABLE `paper_settings` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `brand_name` varchar(100) NOT NULL,
  `paper_size` enum('1/4','crosswise','lengthwise','1_whole') NOT NULL DEFAULT '1_whole',
  `cost_per_unit` decimal(10,2) NOT NULL DEFAULT 1.00, -- e.g., 1 Peso
  `sheets_per_unit` int(11) NOT NULL DEFAULT 1, -- e.g., 3 sheets for 1 Peso (depends on brand)
  `current_stock` int(11) NOT NULL DEFAULT 0, -- Total sheets available
  `max_capacity` int(11) DEFAULT 500,
  `image` varchar(255) DEFAULT NULL, -- URL to brand image
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Dumping data for table `paper_settings`
-- Brand 1: Budget Brand (low price, more sheets per peso)
INSERT INTO `paper_settings` (`brand_name`, `paper_size`, `cost_per_unit`, `sheets_per_unit`, `current_stock`, `max_capacity`) VALUES
('Budget Brand (White)', '1/4',       1.00, 4, 500, 500),
('Budget Brand (White)', 'crosswise', 1.00, 3, 500, 500),
('Budget Brand (White)', 'lengthwise',1.00, 3, 500, 500),
('Budget Brand (White)', '1_whole',   1.00, 2, 500, 500),
-- Brand 2: Premium Brand (higher quality, fewer sheets per peso)
('Premium Brand (Yellow)', '1/4',       1.00, 3, 500, 500),
('Premium Brand (Yellow)', 'crosswise', 1.00, 2, 500, 500),
('Premium Brand (Yellow)', 'lengthwise',1.00, 2, 500, 500),
('Premium Brand (Yellow)', '1_whole',   1.00, 1, 500, 500);

-- --------------------------------------------------------

--
-- Table structure for table `sales_transactions`
--

CREATE TABLE `sales_transactions` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `brand_id` int(11) NOT NULL,
  `paper_size` enum('1/4','crosswise','lengthwise','1_whole') NOT NULL,
  `amount_paid` decimal(10,2) NOT NULL,
  `sheets_dispensed` int(11) NOT NULL,
  `transaction_date` timestamp DEFAULT current_timestamp(),
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Table structure for table `machine_status`
--

CREATE TABLE `machine_status` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `status_key` varchar(50) NOT NULL, -- e.g., 'is_running', 'last_heartbeat', 'error_message'
  `status_value` text,
  `updated_at` timestamp DEFAULT current_timestamp() ON UPDATE current_timestamp(),
  PRIMARY KEY (`id`),
  UNIQUE KEY `status_key` (`status_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `machine_status` (`status_key`, `status_value`) VALUES
('is_running', 'Running'),
('last_heartbeat', NOW()),
('current_error', 'None'),
('wifi_signal', 'Excellent');

-- --------------------------------------------------------

--
-- Table structure for table `activity_logs`
-- Logs admin actions
--

CREATE TABLE `activity_logs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `admin_id` int(11) NOT NULL,
  `action` varchar(255) NOT NULL,
  `details` text,
  `ip_address` varchar(45),
  `created_at` timestamp DEFAULT current_timestamp(),
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Table structure for table `inventory_logs`
-- Logs stock refills vs modifications
--

CREATE TABLE `inventory_logs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `admin_id` int(11) NOT NULL,
  `brand_id` int(11) NOT NULL,
  `qty_change` int(11) NOT NULL, -- Positive for refill, negative for manual adjustment
  `action_type` enum('refill','adjustment','sale') DEFAULT 'refill',
  `notes` text,
  `created_at` timestamp DEFAULT current_timestamp(),
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

COMMIT;
