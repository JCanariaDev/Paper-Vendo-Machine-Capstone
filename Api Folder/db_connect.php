<?php
// db_connect.php for API
$servername = "localhost";
$username = "root";
$password = "";
$dbname = "paper_vendo_db";

$conn = new mysqli($servername, $username, $password, $dbname);

if ($conn->connect_error) {
    http_response_code(500);
    die(json_encode(["error" => "Database connection failed: " . $conn->connect_error]));
}
?>

