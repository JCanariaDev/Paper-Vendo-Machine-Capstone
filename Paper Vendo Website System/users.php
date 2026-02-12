<?php
require_once 'auth.php';

// Handle Add User
if (isset($_POST['submit_btn'])) {
    $username = $_POST['username'];
    $password = $_POST['password']; // In production, use password_hash
    $role = $_POST['role'];

    // Check if username exists
    $check = $conn->prepare("SELECT id FROM admins WHERE username = ?");
    $check->bind_param("s", $username);
    $check->execute();
    if ($check->get_result()->num_rows > 0) {
        $err = "Username already exists!";
    } else {
        $stmt = $conn->prepare("INSERT INTO admins (username, password, role) VALUES (?, ?, ?)");
        $stmt->bind_param("sss", $username, $password, $role);
        if ($stmt->execute()) {
            $msg = "User added successfully!";
            // Log
            $log_stmt = $conn->prepare("INSERT INTO activity_logs (admin_id, action, details) VALUES (?, 'Add User', ?)");
            $details = "Added user: $username ($role)";
            $log_stmt->bind_param("is", $_SESSION['admin_id'], $details);
            $log_stmt->execute();
        } else {
            $err = "Error adding user: " . $conn->error;
        }
    }
}

// Handle Delete User
if (isset($_GET['del'])) {
    $del_id = $_GET['del'];
    if ($del_id != $_SESSION['admin_id']) { // Prevent self-delete
        $stmt = $conn->prepare("DELETE FROM admins WHERE id = ?");
        $stmt->bind_param("i", $del_id);
        if ($stmt->execute()) {
            $msg = "User deleted successfully!";
             // Log
             $log_stmt = $conn->prepare("INSERT INTO activity_logs (admin_id, action, details) VALUES (?, 'Delete User', ?)");
             $details = "Deleted user ID: $del_id";
             $log_stmt->bind_param("is", $_SESSION['admin_id'], $details);
             $log_stmt->execute();
        } else {
            $err = "Error deleting user.";
        }
    } else {
        $err = "You cannot delete your own account.";
    }
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Users - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <h1 class="h3 mb-4 text-gray-800">User Management</h1>
                
                <?php if(isset($msg)) echo "<div class='badge badge-success mb-4' style='display:block; padding:10px;'>$msg</div>"; ?>
                <?php if(isset($err)) echo "<div class='badge badge-danger mb-4' style='display:block; padding:10px;'>$err</div>"; ?>

                <div class="row">
                    <!-- Add User Form -->
                    <div class="col-md-4">
                        <div class="card shadow mb-4">
                            <div class="card-header py-3">
                                <h6 class="m-0 font-weight-bold text-primary">Add New User</h6>
                            </div>
                            <div class="card-body">
                                <form method="POST">
                                    <input type="hidden" name="action" value="add">
                                    <div class="form-group">
                                        <label>Username</label>
                                        <input type="text" name="username" class="form-control" required>
                                    </div>
                                    <div class="form-group">
                                        <label>Password</label>
                                        <input type="password" name="password" class="form-control" required>
                                    </div>
                                    <div class="form-group">
                                        <label>Role</label>
                                        <select name="role" class="form-control">
                                            <option value="staff">Staff</option>
                                            <option value="superadmin">Super Admin</option>
                                        </select>
                                    </div>
                                    <button type="submit" name="submit_btn" class="btn btn-primary btn-block">Create User</button>
                                </form>
                            </div>
                        </div>
                    </div>

                    <!-- User List -->
                    <div class="col-md-8">
                        <div class="card shadow mb-4">
                            <div class="card-header py-3">
                                <h6 class="m-0 font-weight-bold text-primary">Existing Users</h6>
                            </div>
                            <div class="card-body">
                                <table class="table table-bordered">
                                    <thead>
                                        <tr>
                                            <th>ID</th>
                                            <th>Username</th>
                                            <th>Role</th>
                                            <th>Created At</th>
                                            <th>Action</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        <?php
                                        $res = $conn->query("SELECT * FROM admins");
                                        while($row = $res->fetch_assoc()) {
                                            echo "<tr>
                                                <td>{$row['id']}</td>
                                                <td>{$row['username']}</td>
                                                <td>{$row['role']}</td>
                                                <td>{$row['created_at']}</td>
                                                <td>";
                                            if ($row['id'] != $_SESSION['admin_id']) {
                                                echo "<a href='users.php?del={$row['id']}' class='btn btn-danger btn-sm' onclick='return confirm(\"Are you sure?\")'><i class='fas fa-trash'></i></a>";
                                            } else {
                                                echo "<button class='btn btn-secondary btn-sm' disabled>Current</button>";
                                            }
                                            echo "</td></tr>";
                                        }
                                        ?>
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
