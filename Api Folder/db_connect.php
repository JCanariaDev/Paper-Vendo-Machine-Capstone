<?php
// db_connect.php for API
// --- LOCAL CONFIGURATION (XAMPP) ---
$servername = "localhost";
$username = "root";
$password = "";
$dbname = "paper_vendo_db";

// --- HOSTING CONFIGURATION (Uncomment to use) ---
/*
$servername = "sqlXXX.epizy.com";     // e.g., sql301.epizy.com
$username   = "epiz_XXXXXXXX";        // e.g., epiz_12345678
$password   = "your_hosting_pass";   
$dbname     = "epiz_XXXXXXXX_db";
*/

$conn = new mysqli($servername, $username, $password, $dbname);

if ($conn->connect_error) {
    http_response_code(500);
    die(json_encode(["error" => "Database connection failed: " . $conn->connect_error]));
}
?>

