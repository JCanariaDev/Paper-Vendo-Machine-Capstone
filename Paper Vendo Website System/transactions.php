<?php
require_once 'auth.php';

// Filter Logic
$filter_period = $_GET['period'] ?? 'all';
$filter_type = $_GET['type'] ?? 'all';

$where = [];
if ($filter_period == 'today') $where[] = "DATE(transaction_date) = CURDATE()";
if ($filter_period == 'week') $where[] = "transaction_date >= DATE_SUB(NOW(), INTERVAL 7 DAY)";
if ($filter_type != 'all') $where[] = "item_type = '$filter_type'";

$where_sql = count($where) > 0 ? "WHERE " . implode(" AND ", $where) : "";
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Sales Audit - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
    <style>
        @media print {
            #sidebar, .topbar, .filter-section, .btn-print-hide { display: none !important; }
            #content { margin-left: 0 !important; width: 100% !important; background: #fff !important; color: #000 !important; }
            .card { border: none !important; box-shadow: none !important; color: #000 !important; }
            .table { color: #000 !important; }
            .badge { border: 1px solid #000 !important; color: #000 !important; background: transparent !important; }
        }
    </style>
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            
            <div class="container-fluid">
                <div class="d-flex justify-content-between align-items-center mb-4">
                    <h1 class="h3 text-gray-800">Sales Transactions & Audit</h1>
                    <button onclick="window.print()" class="btn btn-primary btn-print-hide"><i class="fas fa-print"></i> Generate Report</button>
                </div>

                <!-- Filter Board -->
                <div class="card shadow mb-4 filter-section">
                    <div class="card-body py-3">
                        <form method="GET" class="row align-items-center">
                            <div class="col-md-3">
                                <label class="small text-muted">Period</label>
                                <select name="period" class="form-control" onchange="this.form.submit()">
                                    <option value="all" <?php if($filter_period=='all') echo 'selected'; ?>>Lifetime History</option>
                                    <option value="today" <?php if($filter_period=='today') echo 'selected'; ?>>Today</option>
                                    <option value="week" <?php if($filter_period=='week') echo 'selected'; ?>>Last 7 Days</option>
                                </select>
                            </div>
                            <div class="col-md-3">
                                <label class="small text-muted">Item Type</label>
                                <select name="type" class="form-control" onchange="this.form.submit()">
                                    <option value="all" <?php if($filter_type=='all') echo 'selected'; ?>>All Items</option>
                                    <option value="paper" <?php if($filter_type=='paper') echo 'selected'; ?>>Papers Only</option>
                                    <option value="ballpen" <?php if($filter_type=='ballpen') echo 'selected'; ?>>Ballpens Only</option>
                                </select>
                            </div>
                            <div class="col-md-6 text-right">
                                <div class="text-xs text-muted mb-1">Total Result Set Value</div>
                                <?php 
                                $total_q = $conn->query("SELECT SUM(amount_paid) as total FROM sales_transactions $where_sql");
                                $total_val = $total_q->fetch_assoc()['total'] ?? 0;
                                ?>
                                <div class="h4 font-weight-bold text-success mb-0">₱<?php echo number_format($total_val, 2); ?></div>
                            </div>
                        </form>
                    </div>
                </div>

                <div class="card shadow mb-4">
                    <div class="card-header py-3">
                        <h6>Transaction Logs</h6>
                    </div>
                    <div class="card-body">
                        <div class="table-responsive">
                            <table class="table">
                                <thead>
                                    <tr>
                                        <th>Ref ID</th>
                                        <th>Product Details</th>
                                        <th>Price</th>
                                        <th>Qty</th>
                                        <th>Net Amount</th>
                                        <th>Date & Timestamp</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $sql = "SELECT t.*, s.brand_name 
                                            FROM sales_transactions t 
                                            LEFT JOIN paper_settings s ON t.brand_id = s.id 
                                            $where_sql 
                                            ORDER BY t.transaction_date DESC LIMIT 500";
                                    $result = $conn->query($sql);

                                    if ($result->num_rows > 0) {
                                        while($row = $result->fetch_assoc()) {
                                            $type_badge = ($row['item_type'] == 'paper') ? '<span class="badge badge-info">PAPER</span>' : '<span class="badge badge-primary">PEN</span>';
                                            $item_name = ($row['item_type'] == 'paper') ? ($row['brand_name'] . " (" . strtoupper($row['paper_size']) . ")") : 'Standard Ballpen';
                                            ?>
                                            <tr>
                                                <td class="text-muted small">#<?php echo str_pad($row['id'], 6, '0', STR_PAD_LEFT); ?></td>
                                                <td>
                                                    <div class="mb-1"><?php echo $type_badge; ?></div>
                                                    <div class="font-weight-bold text-light"><?php echo $item_name; ?></div>
                                                </td>
                                                <td class="small">₱<?php echo number_format($row['amount_paid'] / $row['qty_dispensed'], 2); ?></td>
                                                <td><?php echo $row['qty_dispensed']; ?></td>
                                                <td class="font-weight-bold text-success">₱<?php echo number_format($row['amount_paid'], 2); ?></td>
                                                <td class="small text-light">
                                                    <div><?php echo date('M d, Y', strtotime($row['transaction_date'])); ?></div>
                                                    <div class="text-muted"><?php echo date('h:i A', strtotime($row['transaction_date'])); ?></div>
                                                </td>
                                            </tr>
                                            <?php
                                        }
                                    } else {
                                        echo "<tr><td colspan='6' class='text-center py-5 text-muted'>No transactions match your criteria.</td></tr>";
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
