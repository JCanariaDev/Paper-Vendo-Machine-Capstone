<?php
require_once 'auth.php';
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Activity Logs - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <h1 class="h3 mb-4 text-gray-800">System Activity Logs</h1>

                <div class="card shadow mb-4">
                    <div class="card-header py-3">
                        <h6 class="m-0 font-weight-bold text-primary">Recent Activities</h6>
                    </div>
                    <div class="card-body">
                        <div class="table-responsive">
                            <table class="table table-bordered" width="100%" cellspacing="0">
                                <thead>
                                    <tr>
                                        <th>ID</th>
                                        <th>Admin (User)</th>
                                        <th>Action</th>
                                        <th>Details</th>
                                        <th>Date & Time</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $sql = "SELECT l.id, a.username, l.action, l.details, l.created_at 
                                            FROM activity_logs l 
                                            JOIN admins a ON l.admin_id = a.id 
                                            ORDER BY l.created_at DESC LIMIT 100";
                                    $result = $conn->query($sql);

                                    if ($result && $result->num_rows > 0) {
                                        while($row = $result->fetch_assoc()) {
                                            echo "<tr>
                                                <td>{$row['id']}</td>
                                                <td><span class='badge badge-primary'>{$row['username']}</span></td>
                                                <td>{$row['action']}</td>
                                                <td>{$row['details']}</td>
                                                <td>{$row['created_at']}</td>
                                            </tr>";
                                        }
                                    } else {
                                        echo "<tr><td colspan='5'>No logs found.</td></tr>";
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
