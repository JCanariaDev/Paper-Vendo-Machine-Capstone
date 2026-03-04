<?php
require_once 'auth.php';

// Filter Logic
$filter_admin = $_GET['admin'] ?? 'all';
$where = [];
if ($filter_admin != 'all') $where[] = "l.admin_id = " . intval($filter_admin);

$where_sql = count($where) > 0 ? "WHERE " . implode(" AND ", $where) : "";
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Audit Logs - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <h1 class="h3 mb-4 text-gray-800">System Audit Trail</h1>

                <div class="card shadow mb-4">
                    <div class="card-header py-3 d-flex justify-content-between align-items-center">
                        <h6>Action History</h6>
                        <form method="GET" class="d-flex align-items-center">
                            <label class="small text-muted mr-2 mb-0">Filter Admin:</label>
                            <select name="admin" class="form-control form-control-sm" onchange="this.form.submit()" style="width:150px">
                                <option value="all">All Administrators</option>
                                <?php 
                                $admins = $conn->query("SELECT id, username FROM admins");
                                while($a = $admins->fetch_assoc()) {
                                    $sel = ($filter_admin == $a['id']) ? 'selected' : '';
                                    echo "<option value='{$a['id']}' $sel>{$a['username']}</option>";
                                }
                                ?>
                            </select>
                        </form>
                    </div>
                    <div class="card-body p-0">
                        <div class="table-responsive">
                            <table class="table mb-0">
                                <thead class="bg-dark">
                                    <tr>
                                        <th class="pl-4">Administrative User</th>
                                        <th>Action Type</th>
                                        <th>Operational Details</th>
                                        <th class="pr-4">Timestamp</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $sql = "SELECT l.*, a.username, a.role 
                                            FROM activity_logs l 
                                            JOIN admins a ON l.admin_id = a.id 
                                            $where_sql 
                                            ORDER BY l.created_at DESC LIMIT 200";
                                    $result = $conn->query($sql);

                                    if ($result && $result->num_rows > 0) {
                                        while($row = $result->fetch_assoc()) {
                                            $action_color = 'var(--primary)';
                                            if(strpos($row['action'], 'Delete') !== false) $action_color = 'var(--danger)';
                                            if(strpos($row['action'], 'Update') !== false || strpos($row['action'], 'Edit') !== false) $action_color = 'var(--warning)';
                                            if(strpos($row['action'], 'Login') !== false) $action_color = 'var(--success)';
                                            ?>
                                            <tr>
                                                <td class="pl-4">
                                                    <div class="font-weight-bold"><?php echo htmlspecialchars($row['username']); ?></div>
                                                    <div class="text-xs text-muted"><?php echo strtoupper($row['role']); ?></div>
                                                </td>
                                                <td>
                                                    <span class="badge" style="background: <?php echo $action_color; ?>22; color: <?php echo $action_color; ?>; border: 1px solid <?php echo $action_color; ?>44">
                                                        <?php echo strtoupper($row['action']); ?>
                                                    </span>
                                                </td>
                                                <td class="small text-light"><?php echo htmlspecialchars($row['details']); ?></td>
                                                <td class="pr-4 small text-muted">
                                                    <?php echo date('M d, Y', strtotime($row['created_at'])); ?><br>
                                                    <?php echo date('h:i A', strtotime($row['created_at'])); ?>
                                                </td>
                                            </tr>
                                            <?php
                                        }
                                    } else {
                                        echo "<tr><td colspan='4' class='text-center py-5'>No activity recorded yet.</td></tr>";
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
</body>
</html>
