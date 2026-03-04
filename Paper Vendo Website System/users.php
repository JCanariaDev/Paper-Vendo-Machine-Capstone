<?php
require_once 'auth.php';

// Handle Add/Edit User
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['action'])) {
    $action = $_POST['action'];
    $username = $_POST['username'];
    $role = $_POST['role'];
    $password = $_POST['password'];

    if ($action == 'add') {
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
                $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ({$_SESSION['admin_id']}, 'Add User', 'Created user: $username')");
            } else { $err = "Error adding user."; }
        }
    } elseif ($action == 'edit') {
        $user_id = $_POST['user_id'];
        if (!empty($password)) {
            $stmt = $conn->prepare("UPDATE admins SET username=?, password=?, role=? WHERE id=?");
            $stmt->bind_param("sssi", $username, $password, $role, $user_id);
        } else {
            $stmt = $conn->prepare("UPDATE admins SET username=?, role=? WHERE id=?");
            $stmt->bind_param("ssi", $username, $role, $user_id);
        }
        
        if ($stmt->execute()) {
            $msg = "User updated successfully!";
            $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ({$_SESSION['admin_id']}, 'Edit User', 'Updated user: $username')");
        } else { $err = "Error updating user."; }
    }
}

// Handle Delete
if (isset($_GET['del'])) {
    $del_id = $_GET['del'];
    if ($del_id != $_SESSION['admin_id']) {
        $stmt = $conn->prepare("DELETE FROM admins WHERE id = ?");
        $stmt->bind_param("i", $del_id);
        if ($stmt->execute()) {
            $msg = "User removed!";
            $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ({$_SESSION['admin_id']}, 'Delete User', 'Deleted ID: $del_id')");
        }
    } else { $err = "Self-deletion is restricted."; }
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Users Console - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <div class="d-flex justify-content-between align-items-center mb-4">
                    <h1 class="h3 text-gray-800">User Management</h1>
                    <button class="btn btn-primary" onclick="openAddModal()"><i class="fas fa-plus"></i> Add New Account</button>
                </div>
                
                <?php if(isset($msg)) echo "<div class='badge badge-success mb-4 p-2' style='display:block;'>$msg</div>"; ?>
                <?php if(isset($err)) echo "<div class='badge badge-danger mb-4 p-2' style='display:block;'>$err</div>"; ?>

                <div class="card shadow mb-4">
                    <div class="card-header py-3">
                        <h6>System Administrators & Staff</h6>
                    </div>
                    <div class="card-body">
                        <div class="table-responsive">
                            <table class="table">
                                <thead>
                                    <tr>
                                        <th>Username</th>
                                        <th>Role</th>
                                        <th>Added On</th>
                                        <th class="text-right">Actions</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $res = $conn->query("SELECT * FROM admins ORDER BY id DESC");
                                    while($row = $res->fetch_assoc()) {
                                        $role_class = ($row['role'] == 'superadmin') ? 'badge-primary' : 'badge-info';
                                        $is_me = ($row['id'] == $_SESSION['admin_id']);
                                        ?>
                                        <tr>
                                            <td>
                                                <div class="d-flex align-items-center">
                                                    <div class="user-avatar mr-3" style="width:30px; height:30px; font-size:0.75rem;">
                                                        <?php echo strtoupper(substr($row['username'], 0, 1)); ?>
                                                    </div>
                                                    <div>
                                                        <span class="font-weight-bold"><?php echo htmlspecialchars($row['username']); ?></span>
                                                        <?php if($is_me) echo " <span class='text-muted small'>(You)</span>"; ?>
                                                    </div>
                                                </div>
                                            </td>
                                            <td><span class="badge <?php echo $role_class; ?>"><?php echo strtoupper($row['role']); ?></span></td>
                                            <td class="small text-muted"><?php echo date('M d, Y', strtotime($row['created_at'])); ?></td>
                                            <td class="text-right">
                                                <button class="btn btn-sm btn-primary" onclick='openEditModal(<?php echo json_encode($row); ?>)'><i class="fas fa-user-edit"></i></button>
                                                <?php if(!$is_me): ?>
                                                <a href="users.php?del=<?php echo $row['id']; ?>" class="btn btn-sm btn-danger" onclick="return confirm('Archive this user?')"><i class="fas fa-user-minus"></i></a>
                                                <?php endif; ?>
                                            </td>
                                        </tr>
                                    <?php } ?>
                                </tbody>
                            </table>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <!-- User Modal -->
    <div id="userModal" class="modal">
        <div class="modal-content">
            <span class="close" onclick="closeModals()">&times;</span>
            <div class="d-flex align-items-center mb-3">
                <div class="icon-shape bg-primary-soft text-primary mr-3"><i class="fas fa-user-shield"></i></div>
                <h3 class="m-0" id="modalTitle">Manage User</h3>
            </div>
            <form method="POST">
                <input type="hidden" name="action" id="modalAction" value="add">
                <input type="hidden" name="user_id" id="mUserId">
                
                <div class="form-group">
                    <label>Username</label>
                    <input type="text" name="username" id="mUsername" class="form-control" required>
                </div>
                
                <div class="form-group">
                    <label>System Role</label>
                    <select name="role" id="mRole" class="form-control">
                        <option value="staff">Staff</option>
                        <option value="superadmin">Super Admin</option>
                    </select>
                </div>

                <div class="form-group">
                    <label>Password <span id="passHint" class="small text-muted"></span></label>
                    <input type="password" name="password" id="mPassword" class="form-control">
                </div>

                <button type="submit" class="btn btn-primary btn-block mt-4" id="modalSubmit">Create User</button>
            </form>
        </div>
    </div>

    <script>
    function openAddModal() {
        document.getElementById('userModal').style.display = "block";
        document.getElementById('modalTitle').innerText = "Add New User";
        document.getElementById('modalAction').value = "add";
        document.getElementById('modalSubmit').innerText = "Create User";
        document.getElementById('passHint').innerText = "";
        document.getElementById('mUsername').value = "";
        document.getElementById('mPassword').required = true;
    }
    function openEditModal(data) {
        document.getElementById('userModal').style.display = "block";
        document.getElementById('modalTitle').innerText = "Edit User Settings";
        document.getElementById('modalAction').value = "edit";
        document.getElementById('modalSubmit').innerText = "Update Account";
        document.getElementById('passHint').innerText = "(Leave blank to keep current)";
        document.getElementById('mUserId').value = data.id;
        document.getElementById('mUsername').value = data.username;
        document.getElementById('mRole').value = data.role;
        document.getElementById('mPassword').required = false;
    }
    function closeModals() { document.querySelectorAll('.modal').forEach(m => m.style.display = "none"); }
    window.onclick = function(e) { if(e.target.className == 'modal') closeModals(); }
    </script>
</body>
</html>
