<?php
// db_connect.php
$servername = "localhost"; // Change this if needed
$username = "root";        // Default XAMPP user
$password = "";            // Default XAMPP password
$dbname = "paper_vendo_db"; // Make sure to create this DB in phpMyAdmin

$conn = new mysqli($servername, $username, $password, $dbname);

if ($conn->connect_error) {
    die(json_encode(["error" => "Connection failed: " . $conn->connect_error]));
}
?>
