<?php
require_once 'auth.php'; // Includes session, db, and user details

$msg = "";
$err = "";

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    // Change Password / Details
    $username = $_POST['username'];
    $new_pass = $_POST['password'];
    $confirm = $_POST['confirm_password'];
    
    if (!empty($new_pass)) {
        if ($new_pass === $confirm) {
            // Update with password
            // In prod: password_hash($new_pass, PASSWORD_DEFAULT)
            $stmt = $conn->prepare("UPDATE admins SET username=?, password=? WHERE id=?");
            $stmt->bind_param("ssi", $username, $new_pass, $current_user_id);
            if ($stmt->execute()) {
                $msg = "Profile updated successfully!";
                // Log
                $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ($current_user_id, 'Update Profile', 'Updated username/password')");
                // Refresh user data
                $user['username'] = $username; 
            } else {
                $err = "Error updating profile.";
            }
        } else {
            $err = "Passwords do not match.";
        }
    } else {
        // Update username only
        $stmt = $conn->prepare("UPDATE admins SET username=? WHERE id=?");
        $stmt->bind_param("si", $username, $current_user_id);
        if ($stmt->execute()) {
            $msg = "Username updated!";
            $user['username'] = $username;
        } else {
            $err = "Error updating username.";
        }
    }
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Profile - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <h1 class="h3 mb-4 text-gray-800">My Profile</h1>
                
                <?php if($msg) echo "<div class='badge badge-success mb-4' style='display:block; padding:10px;'>$msg</div>"; ?>
                <?php if($err) echo "<div class='badge badge-danger mb-4' style='display:block; padding:10px;'>$err</div>"; ?>

                <div class="card shadow mb-4" style="max-width:600px">
                    <div class="card-header py-3">
                        <h6 class="m-0 font-weight-bold text-primary">Edit Profile</h6>
                    </div>
                    <div class="card-body">
                        <form method="POST">
                            <div class="form-group">
                                <label>Username</label>
                                <input type="text" name="username" class="form-control" value="<?php echo htmlspecialchars($user['username']); ?>" required>
                            </div>
                            <hr>
                            <label class="text-xs text-primary">Change Password (Leave blank to keep current)</label>
                            <div class="form-group">
                                <label>New Password</label>
                                <input type="password" name="password" class="form-control">
                            </div>
                            <div class="form-group">
                                <label>Confirm Password</label>
                                <input type="password" name="confirm_password" class="form-control">
                            </div>
                            <button type="submit" class="btn btn-primary btn-block">Update Profile</button>
                        </form>
                    </div>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
