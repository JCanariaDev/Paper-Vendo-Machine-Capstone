<?php
/**
 * Paper Vendo Machine API
 * Single ESP32 Communication Endpoint
 * Handles: is_running status, paper computation based on brand and size
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

// Handle preflight
if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS') {
    http_response_code(200);
    exit;
}

require_once __DIR__ . '/../Paper Vendo Website System/db_connect.php';

$method = $_SERVER['REQUEST_METHOD'];
$action = $_GET['action'] ?? '';

// ============================================
// GET: Machine Status (is_running)
// ============================================
if ($method == 'GET' && $action == 'status') {
    $status_q = $conn->query("SELECT status_key, status_value FROM machine_status WHERE status_key IN ('is_running', 'last_heartbeat', 'current_error')");
    $status = [];
    while ($row = $status_q->fetch_assoc()) {
        $status[$row['status_key']] = $row['status_value'];
    }
    
    // Check if online (2 min threshold)
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
// POST: Update Machine Status (is_running)
// ============================================
if ($method == 'POST' && $action == 'update_status') {
    $data = json_decode(file_get_contents('php://input'), true);
    $is_running = $data['is_running'] ?? 'Running';
    
    // Update is_running
    $stmt = $conn->prepare("INSERT INTO machine_status (status_key, status_value) VALUES ('is_running', ?) ON DUPLICATE KEY UPDATE status_value = ?, updated_at = NOW()");
    $stmt->bind_param("ss", $is_running, $is_running);
    $stmt->execute();
    
    // Update heartbeat
    $stmt2 = $conn->prepare("INSERT INTO machine_status (status_key, status_value) VALUES ('last_heartbeat', NOW()) ON DUPLICATE KEY UPDATE status_value = NOW(), updated_at = NOW()");
    $stmt2->execute();
    
    echo json_encode(['success' => true, 'message' => 'Status updated']);
    exit;
}

// ============================================
// GET: Get Paper Computation (Brand + Size)
// ============================================
if ($method == 'GET' && $action == 'get_paper_computation') {
    $brand_id = $_GET['brand_id'] ?? null;
    $paper_size = $_GET['paper_size'] ?? null;
    
    if (!$brand_id || !$paper_size) {
        echo json_encode(['success' => false, 'error' => 'Missing brand_id or paper_size']);
        exit;
    }
    
    // Validate paper_size
    $valid_sizes = ['1/4', 'crosswise', 'lengthwise', '1_whole'];
    if (!in_array($paper_size, $valid_sizes)) {
        echo json_encode(['success' => false, 'error' => 'Invalid paper_size']);
        exit;
    }
    
    // Get settings for this brand and size
    $stmt = $conn->prepare("SELECT id, brand_name, paper_size, cost_per_unit, sheets_per_unit, current_stock FROM paper_settings WHERE id = ? AND paper_size = ?");
    $stmt->bind_param("is", $brand_id, $paper_size);
    $stmt->execute();
    $result = $stmt->get_result();
    
    if ($result->num_rows == 0) {
        echo json_encode(['success' => false, 'error' => 'Brand/Size combination not found']);
        exit;
    }
    
    $row = $result->fetch_assoc();
    
    // Check stock
    if ($row['current_stock'] < $row['sheets_per_unit']) {
        echo json_encode([
            'success' => false,
            'error' => 'Insufficient stock',
            'available' => $row['current_stock'],
            'required' => $row['sheets_per_unit']
        ]);
        exit;
    }
    
    echo json_encode([
        'success' => true,
        'brand_id' => $row['id'],
        'brand_name' => $row['brand_name'],
        'paper_size' => $row['paper_size'],
        'cost_per_unit' => floatval($row['cost_per_unit']),
        'sheets_per_unit' => intval($row['sheets_per_unit']),
        'current_stock' => intval($row['current_stock'])
    ]);
    exit;
}

// ============================================
// POST: Register Transaction
// ============================================
if ($method == 'POST' && $action == 'register_transaction') {
    $data = json_decode(file_get_contents('php://input'), true);
    
    $brand_id = $data['brand_id'] ?? null;
    $paper_size = $data['paper_size'] ?? null;
    $amount_paid = $data['amount_paid'] ?? null;
    $sheets_dispensed = $data['sheets_dispensed'] ?? null;
    
    if (!$brand_id || !$paper_size || !$amount_paid || !$sheets_dispensed) {
        echo json_encode(['success' => false, 'error' => 'Missing required fields']);
        exit;
    }
    
    // Validate paper_size
    $valid_sizes = ['1/4', 'crosswise', 'lengthwise', '1_whole'];
    if (!in_array($paper_size, $valid_sizes)) {
        echo json_encode(['success' => false, 'error' => 'Invalid paper_size']);
        exit;
    }
    
    // Insert transaction
    $stmt = $conn->prepare("INSERT INTO sales_transactions (brand_id, paper_size, amount_paid, sheets_dispensed) VALUES (?, ?, ?, ?)");
    $stmt->bind_param("isdi", $brand_id, $paper_size, $amount_paid, $sheets_dispensed);
    
    if ($stmt->execute()) {
        // Update stock
        $update_stmt = $conn->prepare("UPDATE paper_settings SET current_stock = current_stock - ? WHERE id = ? AND paper_size = ?");
        $update_stmt->bind_param("iis", $sheets_dispensed, $brand_id, $paper_size);
        $update_stmt->execute();
        
        echo json_encode(['success' => true, 'message' => 'Transaction registered']);
    } else {
        echo json_encode(['success' => false, 'error' => 'Failed to register transaction']);
    }
    exit;
}

// ============================================
// GET: Get All Settings (for ESP32 cache)
// ============================================
if ($method == 'GET' && $action == 'get_all_settings') {
    $result = $conn->query("SELECT id, brand_name, paper_size, cost_per_unit, sheets_per_unit, current_stock FROM paper_settings ORDER BY brand_name, paper_size");
    $settings = [];
    
    while ($row = $result->fetch_assoc()) {
        $settings[] = [
            'id' => intval($row['id']),
            'brand_name' => $row['brand_name'],
            'paper_size' => $row['paper_size'],
            'cost_per_unit' => floatval($row['cost_per_unit']),
            'sheets_per_unit' => intval($row['sheets_per_unit']),
            'current_stock' => intval($row['current_stock'])
        ];
    }
    
    echo json_encode(['success' => true, 'data' => $settings]);
    exit;
}

// ============================================
// Default: Invalid Request
// ============================================
http_response_code(400);
echo json_encode(['success' => false, 'error' => 'Invalid request']);
?>

