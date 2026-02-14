<?php
/**
 * Paper Vendo Machine API
 * Updated to handle Physical Sensor Status
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS') {
    http_response_code(200);
    exit;
}

require_once __DIR__ . '/../Paper Vendo Website System/db_connect.php';

$method = $_SERVER['REQUEST_METHOD'];
$action = $_GET['action'] ?? '';

// ============================================
// POST: Update Sensor Status (Physical Detection)
// ============================================
if ($method == 'POST' && $action == 'update_sensor_status') {
    $data = json_decode(file_get_contents('php://input'), true);
    $item_type = $data['item_type'] ?? 'paper';
    $item_id = $data['item_id'] ?? null;
    $status = $data['status'] ?? 'Good'; // Good or Empty

    if ($item_id) {
        if ($item_type == 'paper') {
            $stmt = $conn->prepare("UPDATE paper_settings SET physical_status = ? WHERE id = ?");
        } else {
            $stmt = $conn->prepare("UPDATE ballpen_settings SET physical_status = ? WHERE id = ?");
        }
        $stmt->bind_param("si", $status, $item_id);
        $stmt->execute();
        echo json_encode(['success' => true, 'message' => 'Sensor status updated']);
    } else {
        echo json_encode(['success' => false, 'error' => 'Missing ID']);
    }
    exit;
}

// ============================================
// GET: Status & Online Check
// ============================================
if ($method == 'GET' && $action == 'status') {
    $status_q = $conn->query("SELECT status_key, status_value FROM machine_status WHERE status_key IN ('is_running', 'last_heartbeat', 'current_error')");
    $status = [];
    while ($row = $status_q->fetch_assoc()) {
        $status[$row['status_key']] = $row['status_value'];
    }
    $last_heartbeat = $status['last_heartbeat'] ?? 'Never';
    $online = (strtotime($last_heartbeat) > strtotime('-2 minutes'));
    echo json_encode([
        'success' => true,
        'is_running' => $status['is_running'] ?? 'Offline',
        'is_online' => $online,
        'last_heartbeat' => $last_heartbeat,
        'error' => $status['current_error'] ?? 'None'
    ]);
    exit;
}

// ============================================
// POST: Update Machine Generic Status
// ============================================
if ($method == 'POST' && $action == 'update_status') {
    $data = json_decode(file_get_contents('php://input'), true);
    $is_running = $data['is_running'] ?? 'Running';
    $stmt = $conn->prepare("INSERT INTO machine_status (status_key, status_value) VALUES ('is_running', ?) ON DUPLICATE KEY UPDATE status_value = ?, updated_at = NOW()");
    $stmt->bind_param("ss", $is_running, $is_running);
    $stmt->execute();
    $conn->query("INSERT INTO machine_status (status_key, status_value) VALUES ('last_heartbeat', NOW()) ON DUPLICATE KEY UPDATE status_value = NOW(), updated_at = NOW()");
    echo json_encode(['success' => true]);
    exit;
}

// ============================================
// POST: Register Transaction
// ============================================
if ($method == 'POST' && $action == 'register_transaction') {
    $data = json_decode(file_get_contents('php://input'), true);
    $item_type = $data['item_type'] ?? 'paper';
    $item_id = $data['item_id'] ?? null;
    $amount_paid = $data['amount_paid'] ?? null;
    $qty_dispensed = $data['qty_dispensed'] ?? null;
    $paper_size = $data['paper_size'] ?? null;

    if (!$item_id || !$amount_paid || !$qty_dispensed) {
        echo json_encode(['success' => false, 'error' => 'Missing fields']);
        exit;
    }

    if ($item_type == 'paper') {
        $stmt = $conn->prepare("INSERT INTO sales_transactions (item_type, brand_id, paper_size, amount_paid, qty_dispensed) VALUES ('paper', ?, ?, ?, ?)");
        $stmt->bind_param("isdi", $item_id, $paper_size, $amount_paid, $qty_dispensed);
        $stmt->execute();
        $conn->query("UPDATE paper_settings SET current_stock = current_stock - $qty_dispensed WHERE id = $item_id");
    } else {
        $stmt = $conn->prepare("INSERT INTO sales_transactions (item_type, amount_paid, qty_dispensed) VALUES ('ballpen', ?, ?)");
        $stmt->bind_param("di", $amount_paid, $qty_dispensed);
        $stmt->execute();
        $conn->query("UPDATE ballpen_settings SET current_stock = current_stock - $qty_dispensed WHERE id = $item_id");
    }
    echo json_encode(['success' => true]);
    exit;
}

// ============================================
// GET: Fetch Settings (Cached by ESP32)
// ============================================
if ($method == 'GET' && $action == 'get_all_settings') {
    $paper_res = $conn->query("SELECT id, brand_name, paper_size, cost_per_unit, sheets_per_unit, current_stock, physical_status FROM paper_settings");
    $paper = [];
    while ($row = $paper_res->fetch_assoc()) $paper[] = $row;
    
    $pen_res = $conn->query("SELECT id, item_name, cost_per_unit, current_stock, physical_status FROM ballpen_settings LIMIT 1");
    $pen = $pen_res->fetch_assoc();
    
    echo json_encode(['success' => true, 'paper' => $paper, 'ballpen' => $pen]);
    exit;
}

http_response_code(400);
echo json_encode(['success' => false]);
?>
