<?php
session_start();
require_once 'db_connect.php'; // Ensure BASE_URL is defined

if (!isset($_SESSION['admin_id'])) {
    $redirect = defined('BASE_URL') ? BASE_URL . "index.php" : "index.php";
    header("Location: $redirect");
    exit;
}

// Fetch current user details if needed for navbar
$current_user_id = $_SESSION['admin_id'];
$user_q = $conn->query("SELECT * FROM admins WHERE id = $current_user_id");
$user = $user_q->fetch_assoc();
?>
