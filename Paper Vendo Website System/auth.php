<?php
session_start();
if (!isset($_SESSION['admin_id'])) {
    header("Location: index.php");
    exit;
}
require_once 'db_connect.php';

// Fetch current user details if needed for navbar
$current_user_id = $_SESSION['admin_id'];
$user_q = $conn->query("SELECT * FROM admins WHERE id = $current_user_id");
$user = $user_q->fetch_assoc();
?>
