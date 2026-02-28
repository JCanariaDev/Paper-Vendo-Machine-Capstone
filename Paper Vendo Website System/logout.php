<?php
require_once 'db_connect.php';
session_start();
session_destroy();
header("Location: " . BASE_URL . "index.php");
exit;
?>
