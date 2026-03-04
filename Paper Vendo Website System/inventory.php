<?php
require_once 'auth.php';

// Handle Paper Update
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['update_item'])) {
    $id = $_POST['id']; $cost = $_POST['cost']; $sheets = $_POST['sheets']; $stock = $_POST['stock']; $max = $_POST['max_capacity'];
    $stmt = $conn->prepare("UPDATE paper_settings SET cost_per_unit=?, sheets_per_unit=?, current_stock=?, max_capacity=? WHERE id=?");
    $stmt->bind_param("diiii", $cost, $sheets, $stock, $max, $id);
    if($stmt->execute()) {
        $msg = "Paper configuration updated successfully!";
        // Activity Log
        $log_stmt = $conn->prepare("INSERT INTO activity_logs (admin_id, action, details) VALUES (?, 'Update Inventory', ?)");
        $details = "Updated paper settings for ID: $id";
        $log_stmt->bind_param("is", $_SESSION['admin_id'], $details);
        $log_stmt->execute();
    }
}

// Handle Ballpen Update
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['update_ballpen'])) {
    $id = $_POST['id']; $cost = $_POST['cost']; $stock = $_POST['stock']; $max = $_POST['max_capacity'];
    $stmt = $conn->prepare("UPDATE ballpen_settings SET cost_per_unit=?, current_stock=?, max_capacity=? WHERE id=?");
    $stmt->bind_param("diii", $cost, $stock, $max, $id);
    if($stmt->execute()) {
        $msg = "Ballpen configuration updated successfully!";
        // Activity Log
        $log_stmt = $conn->prepare("INSERT INTO activity_logs (admin_id, action, details) VALUES (?, 'Update Inventory', ?)");
        $details = "Updated ballpen settings for ID: $id";
        $log_stmt->bind_param("is", $_SESSION['admin_id'], $details);
        $log_stmt->execute();
    }
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Inventory Control - Smart Vendo</title>
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
                    <h1 class="h3 text-gray-800">Inventory & Sensor Control</h1>
                    <div class="small text-muted"><i class="fas fa-info-circle"></i> Sensor data is updated every dispensing cycle.</div>
                </div>

                <?php if(isset($msg)) echo "<div class='alert-premium mb-4' style='border-color:var(--success)'><div class='alert-icon bg-success'><i class='fas fa-check'></i></div><div class='alert-content'><strong>Success</strong><p>$msg</p></div></div>"; ?>

                <div class="row">
                    <!-- Paper Section -->
                    <div class="col-xl-12">
                        <div class="card shadow mb-4">
                            <div class="card-header py-3">
                                <h6>Paper Stock Management</h6>
                            </div>
                            <div class="card-body">
                                <div class="table-responsive">
                                    <table class="table">
                                        <thead>
                                            <tr>
                                                <th>Brand & Size</th>
                                                <th>Stock Level (DB)</th>
                                                <th width="150">Physical Status</th>
                                                <th>Price Configuration</th>
                                                <th>Action</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            <?php
                                            $res = $conn->query("SELECT * FROM paper_settings");
                                            while($row = $res->fetch_assoc()):
                                                $perc = ($row['max_capacity'] > 0) ? ($row['current_stock'] / $row['max_capacity']) * 100 : 0;
                                                $bar_color = ($perc < 20) ? 'var(--danger)' : (($perc < 50) ? 'var(--warning)' : 'var(--primary)');
                                                $s_color = ($row['physical_status'] == 'Empty') ? 'danger' : 'success';
                                                $s_icon = ($row['physical_status'] == 'Empty') ? 'fa-times-circle' : 'fa-check-circle';
                                            ?>
                                            <tr>
                                                <td>
                                                    <div class="font-weight-bold text-light"><?php echo htmlspecialchars($row['brand_name']); ?></div>
                                                    <div class="text-xs text-muted"><?php echo str_replace('_', ' ', strtoupper($row['paper_size'])); ?></div>
                                                </td>
                                                <td>
                                                    <div class="d-flex align-items-center">
                                                        <div class="progress flex-grow-1 mr-3" style="height: 6px; background: rgba(0,0,0,0.2);">
                                                            <div class="progress-bar" style="width: <?php echo $perc; ?>%; background: <?php echo $bar_color; ?>;"></div>
                                                        </div>
                                                        <span class="small font-weight-bold"><?php echo $row['current_stock']; ?> / <?php echo $row['max_capacity']; ?></span>
                                                    </div>
                                                </td>
                                                <td>
                                                    <span class="badge badge-<?php echo $s_color; ?>">
                                                        <i class="fas <?php echo $s_icon; ?>"></i> <?php echo strtoupper($row['physical_status']); ?>
                                                    </span>
                                                </td>
                                                <td>
                                                    <span class="text-success font-weight-bold">₱<?php echo number_format($row['cost_per_unit'], 2); ?></span>
                                                    <span class="text-muted small">/ <?php echo $row['sheets_per_unit']; ?> sheets</span>
                                                </td>
                                                <td>
                                                    <button class="btn btn-primary btn-sm" onclick='openEditModal(<?php echo json_encode($row); ?>)'>
                                                        <i class="fas fa-edit"></i> Configure
                                                    </button>
                                                </td>
                                            </tr>
                                            <?php endwhile; ?>
                                        </tbody>
                                    </table>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Ballpen Section -->
                    <div class="col-xl-12">
                        <div class="card shadow mb-4">
                            <div class="card-header py-3">
                                <h6>Ballpen Stock Management</h6>
                            </div>
                            <div class="card-body">
                                <div class="table-responsive">
                                    <table class="table">
                                        <thead>
                                            <tr>
                                                <th>Item Name</th>
                                                <th>Stock Level (DB)</th>
                                                <th width="150">Physical Status</th>
                                                <th>Unit Price</th>
                                                <th>Action</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            <?php
                                            $res = $conn->query("SELECT * FROM ballpen_settings");
                                            while($row = $res->fetch_assoc()):
                                                $perc = ($row['max_capacity'] > 0) ? ($row['current_stock'] / $row['max_capacity']) * 100 : 0;
                                                $bar_color = ($perc < 20) ? 'var(--danger)' : 'var(--primary)';
                                                $s_color = ($row['physical_status'] == 'Empty') ? 'danger' : 'success';
                                            ?>
                                            <tr>
                                                <td class="font-weight-bold text-light"><?php echo htmlspecialchars($row['item_name']); ?></td>
                                                <td>
                                                    <div class="d-flex align-items-center">
                                                        <div class="progress flex-grow-1 mr-3" style="height: 6px; background: rgba(0,0,0,0.2);">
                                                            <div class="progress-bar" style="width: <?php echo $perc; ?>%; background: <?php echo $bar_color; ?>;"></div>
                                                        </div>
                                                        <span class="small font-weight-bold"><?php echo $row['current_stock']; ?> / <?php echo $row['max_capacity']; ?></span>
                                                    </div>
                                                </td>
                                                <td><span class="badge badge-<?php echo $s_color; ?>"><?php echo strtoupper($row['physical_status']); ?></span></td>
                                                <td class="text-success font-weight-bold">₱<?php echo number_format($row['cost_per_unit'], 2); ?></td>
                                                <td>
                                                    <button class="btn btn-primary btn-sm" onclick='openPenModal(<?php echo json_encode($row); ?>)'>
                                                        <i class="fas fa-cog"></i> Setup
                                                    </button>
                                                </td>
                                            </tr>
                                            <?php endwhile; ?>
                                        </tbody>
                                    </table>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <!-- Edit Paper Modal -->
    <div id="editModal" class="modal">
        <div class="modal-content">
            <span class="close" onclick="closeModals()">&times;</span>
            <div class="d-flex align-items-center mb-3">
                <div class="icon-shape bg-primary-soft text-primary mr-3"><i class="fas fa-print"></i></div>
                <h3 class="m-0">Configure Paper</h3>
            </div>
            <form method="POST">
                <input type="hidden" name="update_item" value="1"><input type="hidden" name="id" id="mId">
                <div class="row">
                    <div class="col-md-6 form-group"><label>Current Stock</label><input type="number" name="stock" id="mStock" class="form-control"></div>
                    <div class="col-md-6 form-group"><label>Max Capacity</label><input type="number" name="max_capacity" id="mMax" class="form-control"></div>
                </div>
                <div class="row">
                    <div class="col-md-6 form-group"><label>Price (₱)</label><input type="number" step="0.01" name="cost" id="mCost" class="form-control"></div>
                    <div class="col-md-6 form-group"><label>Sheets per Unit</label><input type="number" name="sheets" id="mSheets" class="form-control"></div>
                </div>
                <button type="submit" class="btn btn-primary btn-block mt-3">Apply Changes</button>
            </form>
        </div>
    </div>

    <!-- Edit Ballpen Modal -->
    <div id="penModal" class="modal">
        <div class="modal-content">
            <span class="close" onclick="closeModals()">&times;</span>
            <div class="d-flex align-items-center mb-3">
                <div class="icon-shape bg-primary-soft text-primary mr-3"><i class="fas fa-pen"></i></div>
                <h3 class="m-0">Configure Ballpen</h3>
            </div>
            <form method="POST">
                <input type="hidden" name="update_ballpen" value="1"><input type="hidden" name="id" id="pId">
                <div class="row">
                    <div class="col-md-6 form-group"><label>Current Stock</label><input type="number" name="stock" id="pStock" class="form-control"></div>
                    <div class="col-md-6 form-group"><label>Max Capacity</label><input type="number" name="max_capacity" id="pMax" class="form-control"></div>
                </div>
                <div class="form-group"><label>Unit Price (₱)</label><input type="number" step="0.01" name="cost" id="pCost" class="form-control"></div>
                <button type="submit" class="btn btn-primary btn-block mt-3">Apply Changes</button>
            </form>
        </div>
    </div>

    <script>
    function openEditModal(data) {
        document.getElementById('editModal').style.display = "block";
        document.getElementById('mId').value = data.id;
        document.getElementById('mStock').value = data.current_stock;
        document.getElementById('mCost').value = data.cost_per_unit;
        document.getElementById('mSheets').value = data.sheets_per_unit;
        document.getElementById('mMax').value = data.max_capacity;
    }
    function openPenModal(data) {
        document.getElementById('penModal').style.display = "block";
        document.getElementById('pId').value = data.id;
        document.getElementById('pStock').value = data.current_stock;
        document.getElementById('pCost').value = data.cost_per_unit;
        document.getElementById('pMax').value = data.max_capacity;
    }
    function closeModals() { document.querySelectorAll('.modal').forEach(m => m.style.display = "none"); }
    window.onclick = function(e) { if(e.target.className == 'modal') closeModals(); }
    </script>
</body>
</html>
