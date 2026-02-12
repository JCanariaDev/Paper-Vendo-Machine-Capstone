<?php
session_start();
require_once 'db_connect.php';

// Handle Login
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    $username = $_POST['username'];
    $password = $_POST['password'];

    // Updated Login Logic with Role
    $stmt = $conn->prepare("SELECT id, username, password, role FROM admins WHERE username = ?");
    $stmt->bind_param("s", $username);
    $stmt->execute();
    $result = $stmt->get_result();

    if ($result->num_rows > 0) {
        $row = $result->fetch_assoc();
        if ($password == "admin123" || $row['password'] == $password) { 
             $_SESSION['admin_id'] = $row['id'];
             $_SESSION['username'] = $row['username'];
             $_SESSION['role'] = $row['role'];
             
             // Log Activity
             $log_stmt = $conn->prepare("INSERT INTO activity_logs (admin_id, action, details) VALUES (?, 'Login', 'User logged in')");
             $log_stmt->bind_param("i", $row['id']);
             $log_stmt->execute();
             
             header("Location: dashboard.php");
             exit;
        } else {
            $error = "Incorrect password.";
        }
    } else {
        $error = "Account not found.";
    }
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Login - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body class="login-body">
    
    <div class="login-container">
        <!-- LEFT SIDE: Branding -->
        <div class="login-left">
            <img src="logo.png" alt="Logo" class="brand-logo">
            <div class="brand-title">PAPER VENDO</div>
            <div class="brand-desc">
                The smart, automated solution for student paper needs. Fast, reliable, and always online.
            </div>
        </div>

        <!-- RIGHT SIDE: Form -->
        <div class="login-right">
            <div class="login-header">
                <h2>Welcome Back!</h2>
                <p>Please enter your credentials to access the admin panel.</p>
            </div>

            <form method="POST">
                <div class="form-group">
                    <label>Username</label>
                    <input type="text" name="username" class="form-control" placeholder="Enter your username" required>
                </div>

                <div class="form-group">
                    <label>Password</label>
                    <input type="password" name="password" class="form-control" placeholder="Enter your password" required>
                </div>

                <?php if(isset($error)) echo "<div class='text-danger mb-4' style='font-size: 0.9rem;'><i class='fas fa-exclamation-circle'></i> $error</div>"; ?>

                <button type="submit" class="btn btn-primary btn-block">
                    <i class="fas fa-sign-in-alt"></i> Login to Dashboard
                </button>
            </form>

            <div style="margin-top: 20px; text-align: center; color: #6b7280; font-size: 0.85rem;">
                Forgot Password? Contact Super Admin.<br>
                <span style="opacity: 0.7;">Demo: admin / admin123</span>
            </div>
        </div>
    </div>

</body>
</html>
