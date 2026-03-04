<?php
require_once 'auth.php';

// Handle Profile Updates
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    $new_user = $_POST['username'];
    $new_pass = $_POST['password'];
    $confirm_pass = $_POST['confirm_password'];
    $admin_id = $_SESSION['admin_id'];

    if (!empty($new_pass)) {
        if ($new_pass === $confirm_pass) {
            $stmt = $conn->prepare("UPDATE admins SET username=?, password=? WHERE id=?");
            $stmt->bind_param("ssi", $new_user, $new_pass, $admin_id);
            if ($stmt->execute()) {
                $_SESSION['username'] = $new_user;
                $msg = "Profile updated successfully!";
                $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ($admin_id, 'Profile Update', 'Changed username/password')");
            } else { $err = "Error updating security credentials."; }
        } else { $err = "New passwords do not match!"; }
    } else {
        $stmt = $conn->prepare("UPDATE admins SET username=? WHERE id=?");
        $stmt->bind_param("si", $new_user, $admin_id);
        if ($stmt->execute()) {
            $_SESSION['username'] = $new_user;
            $msg = "Username updated!";
            $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ($admin_id, 'Profile Update', 'Changed username')");
        } else { $err = "Error updating username."; }
    }
}

// Fetch current info
$q = $conn->prepare("SELECT * FROM admins WHERE id = ?");
$q->bind_param("i", $_SESSION['admin_id']);
$q->execute();
$current_user = $q->get_result()->fetch_assoc();
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>My Profile - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <div class="row">
                    <!-- Left: Profile Info -->
                    <div class="col-xl-4 col-lg-5">
                        <div class="card shadow mb-4">
                            <div class="card-body text-center py-5">
                                <div class="user-avatar mx-auto mb-4" style="width: 100px; height: 100px; font-size: 2.5rem; background: linear-gradient(135deg, var(--primary), var(--info));">
                                    <?php echo strtoupper(substr($current_user['username'], 0, 1)); ?>
                                </div>
                                <h4 class="mb-0 font-weight-bold"><?php echo htmlspecialchars($current_user['username']); ?></h4>
                                <span class="badge badge-primary-soft text-primary mt-2"><?php echo strtoupper($current_user['role']); ?></span>
                                <hr class="my-4">
                                <div class="text-left small text-muted">
                                    <div class="mb-2"><i class="fas fa-calendar-alt mr-2 text-primary"></i> Joined: <?php echo date('M d, Y', strtotime($current_user['created_at'])); ?></div>
                                    <div class="mb-2"><i class="fas fa-id-badge mr-2 text-primary"></i> UID: #<?php echo str_pad($current_user['id'], 3, '0', STR_PAD_LEFT); ?></div>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Right: Advanced Settings -->
                    <div class="col-xl-8 col-lg-7">
                        <div class="card shadow mb-4">
                            <div class="card-header py-3">
                                <h6>Security Credentials & Settings</h6>
                            </div>
                            <div class="card-body">
                                <?php if(isset($msg)) echo "<div class='alert-premium mb-4' style='border-color:var(--success)'><div class='alert-icon bg-success'><i class='fas fa-check'></i></div><div class='alert-content'><strong>Success</strong><p>$msg</p></div></div>"; ?>
                                <?php if(isset($err)) echo "<div class='alert-premium mb-4' style='border-color:var(--danger)'><div class='alert-icon bg-danger'><i class='fas fa-exclamation-triangle'></i></div><div class='alert-content'><strong>Update Error</strong><p>$err</p></div></div>"; ?>

                                <form method="POST">
                                    <div class="row mb-4">
                                        <div class="col-md-6">
                                            <div class="form-group mb-0">
                                                <label class="text-xs text-muted font-weight-bold uppercase mb-1">Administrative Username</label>
                                                <input type="text" name="username" class="form-control bg-dark border-0" value="<?php echo htmlspecialchars($current_user['username']); ?>" required>
                                            </div>
                                        </div>
                                        <div class="col-md-6">
                                            <div class="form-group mb-0">
                                                <label class="text-xs text-muted font-weight-bold uppercase mb-1">Administrator Role</label>
                                                <input type="text" class="form-control bg-dark border-0" value="<?php echo strtoupper($current_user['role']); ?>" disabled>
                                            </div>
                                        </div>
                                    </div>

                                    <div class="row">
                                        <div class="col-md-6">
                                            <div class="form-group">
                                                <label class="text-xs text-muted font-weight-bold uppercase mb-1">New Secure Password</label>
                                                <input type="password" name="password" class="form-control bg-dark border-0" placeholder="••••••••">
                                                <small class="text-muted">Leave empty to keep current password.</small>
                                            </div>
                                        </div>
                                        <div class="col-md-6">
                                            <div class="form-group">
                                                <label class="text-xs text-muted font-weight-bold uppercase mb-1">Confirm New Password</label>
                                                <input type="password" name="confirm_password" class="form-control bg-dark border-0" placeholder="••••••••">
                                            </div>
                                        </div>
                                    </div>

                                    <div class="alert bg-warning-soft text-warning border-0 small mt-3">
                                        <i class="fas fa-shield-alt mr-2"></i> Warning: Changing your username or password will require a new login session.
                                    </div>

                                    <hr class="my-4">
                                    <button type="submit" class="btn btn-primary px-4 py-2">Save Secure Profile Changes</button>
                                </form>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
