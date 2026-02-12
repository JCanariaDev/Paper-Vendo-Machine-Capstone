<?php
require_once 'auth.php';

// Handle Add New Item
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['add_item'])) {
    $brand = $_POST['brand'];
    $size = $_POST['size'];
    $cost = $_POST['cost'];
    $sheets = $_POST['sheets'];
    $stock = $_POST['stock'];
    $max = $_POST['max_capacity'];
    
    // Validate inputs slightly
    if (!empty($brand) && !empty($size)) {
        $stmt = $conn->prepare("INSERT INTO paper_settings (brand_name, paper_size, cost_per_unit, sheets_per_unit, current_stock, max_capacity) VALUES (?, ?, ?, ?, ?, ?)");
        $stmt->bind_param("ssdiii", $brand, $size, $cost, $sheets, $stock, $max);
        
        if($stmt->execute()) {
            $msg = "New item added successfully!";
            // Log
            $details = "Added new item: $brand ($size)";
            $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ({$_SESSION['admin_id']}, 'Add Inventory', '$details')");
        } else {
            $error = "Error adding item: " . $conn->error;
        }
    } else {
        $error = "Brand and Size are required.";
    }
}

// Handle Update Item
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['update_item'])) {
    $id = $_POST['id'];
    $cost = $_POST['cost'];
    $sheets = $_POST['sheets'];
    $stock = $_POST['stock'];
    $max = $_POST['max_capacity'];
    
    $stmt = $conn->prepare("UPDATE paper_settings SET cost_per_unit=?, sheets_per_unit=?, current_stock=?, max_capacity=? WHERE id=?");
    $stmt->bind_param("diiii", $cost, $sheets, $stock, $max, $id);
    
    if($stmt->execute()) {
        $msg = "Settings updated successfully!";
        // Log
        $details = "Updated Item ID $id: Stock=$stock, Cost=$cost";
        $conn->query("INSERT INTO activity_logs (admin_id, action, details) VALUES ({$_SESSION['admin_id']}, 'Update Inventory', '$details')");
    }
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Inventory - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <h1 class="h3 mb-4 text-gray-800">Inventory Management</h1>
                
                <?php 
                if(isset($msg)) echo "<div class='badge badge-success mb-4' style='display:block; padding:10px;'>$msg</div>"; 
                if(isset($error)) echo "<div class='badge badge-danger mb-4' style='display:block; padding:10px;'>$error</div>"; 
                ?>

                <div class="card shadow mb-4">
                    <div class="card-header py-3 d-flex flex-row align-items-center justify-content-between">
                        <h6 class="m-0 font-weight-bold text-primary">Paper Stock & Pricing</h6>
                        <button class="btn btn-success btn-sm" onclick="openAddModal()">
                            <i class="fas fa-plus"></i> Add New Item
                        </button>
                    </div>
                    <div class="card-body">
                        <div class="table-responsive">
                            <table class="table table-bordered" width="100%" cellspacing="0">
                                <thead>
                                    <tr>
                                        <th>Brand</th>
                                        <th>Paper Size</th>
                                        <th>Cost (PHP)</th>
                                        <th>Sheets per Cost</th>
                                        <th>Stock Level</th>
                                        <th>Capacity</th>
                                        <th>Status</th>
                                        <th>Actions</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $res = $conn->query("SELECT * FROM paper_settings ORDER BY brand_name, paper_size");
                                    while($row = $res->fetch_assoc()) {
                                        $percent = ($row['max_capacity'] > 0) ? ($row['current_stock'] / $row['max_capacity']) * 100 : 0;
                                        $status = ($percent < 20) ? '<span class="badge badge-danger">Low Stock</span>' : '<span class="badge badge-success">Good</span>';
                                        
                                        // Map DB enum values to Display labels
                                        $size_map = [
                                            '1/4' => '1/4',
                                            'crosswise' => 'Crosswise',
                                            'lengthwise' => 'Lengthwise',
                                            '1_whole' => '1 Whole'
                                        ];
                                        $size_label = isset($size_map[$row['paper_size']]) ? $size_map[$row['paper_size']] : $row['paper_size'];
                                        
                                        $display_name = addslashes($row['brand_name'] . ' - ' . $size_label);
                                        
                                        echo "<tr>
                                            <td>{$row['brand_name']}</td>
                                            <td>{$size_label}</td>
                                            <td>₱{$row['cost_per_unit']}</td>
                                            <td>{$row['sheets_per_unit']}</td>
                                            <td>{$row['current_stock']}</td>
                                            <td>{$row['max_capacity']}</td>
                                            <td>$status</td>
                                            <td>
                                                <button class='btn btn-primary btn-sm' onclick=\"openEditModal({$row['id']}, '{$display_name}', {$row['cost_per_unit']}, {$row['sheets_per_unit']}, {$row['current_stock']}, {$row['max_capacity']})\">
                                                    <i class='fas fa-edit'></i> Edit
                                                </button>
                                            </td>
                                        </tr>";
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

    <!-- Edit Modal -->
    <div id="editModal" class="modal">
        <div class="modal-content" style="max-width:500px">
            <span class="close" onclick="closeEditModal()">&times;</span>
            <h3 id="modalTitle">Edit Item</h3>
            <form method="POST" class="mt-4">
                <input type="hidden" name="update_item" value="1">
                <input type="hidden" name="id" id="modalId">
                <div class="form-group">
                    <label>Cost per Unit (PHP)</label>
                    <input type="number" step="0.01" name="cost" id="modalCost" class="form-control" required>
                </div>
                <div class="form-group">
                    <label>Sheets per Unit</label>
                    <input type="number" name="sheets" id="modalSheets" class="form-control" required>
                </div>
                <div class="form-group">
                    <label>Current Stock</label>
                    <input type="number" name="stock" id="modalStock" class="form-control" required>
                </div>
                <div class="form-group">
                    <label>Max Capacity</label>
                    <input type="number" name="max_capacity" id="modalMax" class="form-control" required>
                </div>
                <button type="submit" class="btn btn-primary btn-block">Save Changes</button>
            </form>
        </div>
    </div>

    <!-- Add Item Modal -->
    <div id="addModal" class="modal">
        <div class="modal-content" style="max-width:500px">
            <span class="close" onclick="closeAddModal()">&times;</span>
            <h3>Add New Paper Item</h3>
            <form method="POST" class="mt-4">
                <input type="hidden" name="add_item" value="1">
                
                <div class="form-group">
                    <label>Brand Name</label>
                    <input type="text" name="brand" class="form-control" placeholder="e.g. Budget Brand" required>
                </div>

                <div class="form-group">
                    <label>Paper Size</label>
                    <select name="size" class="form-control" required>
                        <option value="1/4">1/4</option>
                        <option value="crosswise">Crosswise</option>
                        <option value="lengthwise">Lengthwise</option>
                        <option value="1_whole">1 Whole</option>
                    </select>
                </div>

                <div class="form-group">
                    <label>Cost per Unit (PHP)</label>
                    <input type="number" step="0.01" name="cost" class="form-control" placeholder="1.00" required>
                </div>

                <div class="form-group">
                    <label>Sheets per Unit</label>
                    <input type="number" name="sheets" class="form-control" placeholder="1" required>
                </div>

                <div class="form-group">
                    <label>Current Stock</label>
                    <input type="number" name="stock" class="form-control" placeholder="0" required>
                </div>

                <div class="form-group">
                    <label>Max Capacity</label>
                    <input type="number" name="max_capacity" class="form-control" value="500" required>
                </div>

                <button type="submit" class="btn btn-success btn-block">Add Item</button>
            </form>
        </div>
    </div>

    <script>
    // Edit Modal Functions
    function openEditModal(id, name, cost, sheets, stock, max) {
        document.getElementById('editModal').style.display = "block";
        document.getElementById('modalTitle').innerText = "Edit: " + name;
        document.getElementById('modalId').value = id;
        document.getElementById('modalCost').value = cost;
        document.getElementById('modalSheets').value = sheets;
        document.getElementById('modalStock').value = stock;
        document.getElementById('modalMax').value = max;
    }

    function closeEditModal() {
        document.getElementById('editModal').style.display = "none";
    }

    // Add Modal Functions
    function openAddModal() {
        document.getElementById('addModal').style.display = "block";
    }

    function closeAddModal() {
        document.getElementById('addModal').style.display = "none";
    }

    // Close if clicked outside
    window.onclick = function(event) {
        var editModal = document.getElementById('editModal');
        var addModal = document.getElementById('addModal');
        if (event.target == editModal) {
            closeEditModal();
        }
        if (event.target == addModal) {
            closeAddModal();
        }
    }
    </script>
</body>
</html>
