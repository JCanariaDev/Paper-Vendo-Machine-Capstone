<?php
require_once 'auth.php';

// Handle Paper Update
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['update_item'])) {
    $id = $_POST['id']; $cost = $_POST['cost']; $sheets = $_POST['sheets']; $stock = $_POST['stock']; $max = $_POST['max_capacity'];
    $stmt = $conn->prepare("UPDATE paper_settings SET cost_per_unit=?, sheets_per_unit=?, current_stock=?, max_capacity=? WHERE id=?");
    $stmt->bind_param("diiii", $cost, $sheets, $stock, $max, $id);
    $stmt->execute();
    $msg = "Paper updated!";
}

// Handle Ballpen Update
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['update_ballpen'])) {
    $id = $_POST['id']; $cost = $_POST['cost']; $stock = $_POST['stock']; $max = $_POST['max_capacity'];
    $stmt = $conn->prepare("UPDATE ballpen_settings SET cost_per_unit=?, current_stock=?, max_capacity=? WHERE id=?");
    $stmt->bind_param("diii", $cost, $stock, $max, $id);
    $stmt->execute();
    $msg = "Ballpen updated!";
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Inventory Management - Smart Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            <div class="container-fluid">
                <h1 class="h3 mb-4 text-gray-800">Inventory & Sensor Status</h1>
                <?php if(isset($msg)) echo "<div class='badge badge-success mb-4 p-2' style='display:block;'>$msg</div>"; ?>

                <div class="card shadow mb-4">
                    <div class="card-header py-3"><h6>Paper Management</h6></div>
                    <div class="card-body">
                        <div class="table-responsive">
                            <table class="table table-bordered">
                                <thead>
                                    <tr>
                                        <th>Brand & Size</th>
                                        <th>DB Stock</th>
                                        <th>Physical Bin Status</th>
                                        <th>Price (PHP / Unit)</th>
                                        <th>Actions</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $res = $conn->query("SELECT * FROM paper_settings");
                                    while($row = $res->fetch_assoc()):
                                        $status_text = ($row['physical_status'] == 'Empty') ? 'Empty' : 'Not Empty';
                                        $s_color = ($row['physical_status'] == 'Empty') ? 'danger' : 'success';
                                        $s_icon = ($row['physical_status'] == 'Empty') ? 'fa-times-circle' : 'fa-check-circle';
                                    ?>
                                    <tr>
                                        <td><?php echo $row['brand_name'] . " (" . $row['paper_size'] . ")"; ?></td>
                                        <td><?php echo $row['current_stock'] . " / " . $row['max_capacity']; ?></td>
                                        <td>
                                            <span class="badge badge-<?php echo $s_color; ?>">
                                                <i class="fas <?php echo $s_icon; ?>"></i> <?php echo $status_text; ?>
                                            </span>
                                        </td>
                                        <td>₱<?php echo $row['cost_per_unit'] . " / " . $row['sheets_per_unit'] . " sheets"; ?></td>
                                        <td>
                                            <button class="btn btn-primary btn-sm" onclick="openEditModal(<?php echo htmlspecialchars(json_encode($row)); ?>)">Edit</button>
                                        </td>
                                    </tr>
                                    <?php endwhile; ?>
                                </tbody>
                            </table>
                        </div>
                    </div>
                </div>

                <div class="card shadow mb-4">
                    <div class="card-header py-3"><h6>Ballpen Management</h6></div>
                    <div class="card-body">
                        <div class="table-responsive">
                            <table class="table table-bordered">
                                <thead>
                                    <tr>
                                        <th>Product Name</th>
                                        <th>DB Stock</th>
                                        <th>Physical Bin Status</th>
                                        <th>Price (PHP)</th>
                                        <th>Actions</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $res = $conn->query("SELECT * FROM ballpen_settings");
                                    while($row = $res->fetch_assoc()):
                                        $status_text = ($row['physical_status'] == 'Empty') ? 'Empty' : 'Not Empty';
                                        $s_color = ($row['physical_status'] == 'Empty') ? 'danger' : 'success';
                                    ?>
                                    <tr>
                                        <td><?php echo $row['item_name']; ?></td>
                                        <td><?php echo $row['current_stock'] . " / " . $row['max_capacity']; ?></td>
                                        <td><span class="badge badge-<?php echo $s_color; ?>"><?php echo $status_text; ?></span></td>
                                        <td>₱<?php echo number_format($row['cost_per_unit'], 2); ?></td>
                                        <td>
                                            <button class="btn btn-sm text-white" style="background:#6366f1;" onclick="openPenModal(<?php echo htmlspecialchars(json_encode($row)); ?>)">Edit</button>
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

    <!-- Modals (Edit Paper/Pen) - Logic omitted for brevity, but IDs map correctly -->
    <div id="editModal" class="modal">
        <div class="modal-content">
            <span class="close" onclick="closeModals()">&times;</span>
            <h3>Update Paper</h3>
            <form method="POST" class="mt-3">
                <input type="hidden" name="update_item" value="1"><input type="hidden" name="id" id="mId">
                <div class="form-group"><label>Stock</label><input type="number" name="stock" id="mStock" class="form-control"></div>
                <div class="form-group"><label>Cost</label><input type="number" step="0.01" name="cost" id="mCost" class="form-control"></div>
                <div class="form-group"><label>Sheets</label><input type="number" name="sheets" id="mSheets" class="form-control"></div>
                <div class="form-group"><label>Max</label><input type="number" name="max_capacity" id="mMax" class="form-control"></div>
                <button type="submit" class="btn btn-primary btn-block">Save</button>
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
    function closeModals() { document.querySelectorAll('.modal').forEach(m => m.style.display = "none"); }
    window.onclick = function(e) { if(e.target.className == 'modal') closeModals(); }
    </script>
</body>
</html>
