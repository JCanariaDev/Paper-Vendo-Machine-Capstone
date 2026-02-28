<?php
// db_connect.php
// --- LOCAL CONFIGURATION (XAMPP) ---
$servername = "localhost";
$username = "root";
$password = "";
$dbname = "paper_vendo_db";
define('BASE_URL', '/Paper%20Vendo%20Machine%20Capstone/Paper%20Vendo%20Website%20System/');

// --- HOSTING CONFIGURATION (Uncomment to use) ---
/*
$servername = "sqlXXX.epizy.com";     // e.g., sql301.epizy.com
$username   = "epiz_XXXXXXXX";        // e.g., epiz_12345678
$password   = "your_hosting_pass";   
$dbname     = "epiz_XXXXXXXX_db";
define('BASE_URL', '/');             // Adjust if in a subfolder like /paper-vendo/
*/

$conn = new mysqli($servername, $username, $password, $dbname);

if ($conn->connect_error) {
    die(json_encode(["error" => "Connection failed: " . $conn->connect_error]));
}
?>
