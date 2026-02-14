<?php
require_once 'auth.php';

// Fetch Totals
$total_sales_q = $conn->query("SELECT SUM(amount_paid) as total FROM sales_transactions");
$total_sales = $total_sales_q->fetch_assoc()['total'] ?? 0;

$total_papers_q = $conn->query("SELECT SUM(qty_dispensed) as total FROM sales_transactions WHERE item_type = 'paper'");
$total_papers = $total_papers_q->fetch_assoc()['total'] ?? 0;

$total_pens_q = $conn->query("SELECT SUM(qty_dispensed) as total FROM sales_transactions WHERE item_type = 'ballpen'");
$total_pens = $total_pens_q->fetch_assoc()['total'] ?? 0;

// Machine Status
$status_q = $conn->query("SELECT status_value FROM machine_status WHERE status_key='is_running'");
$is_running = $status_q->fetch_assoc()['status_value'] ?? 'Offline';
$last_heart = $conn->query("SELECT status_value FROM machine_status WHERE status_key='last_heartbeat'")->fetch_assoc()['status_value'] ?? 'Never';
$online = (strtotime($last_heart) > strtotime('-2 minutes'));

// Today's Sales
$today = date('Y-m-d');
$today_sales = $conn->query("SELECT SUM(amount_paid) as total FROM sales_transactions WHERE DATE(transaction_date) = '$today'")->fetch_assoc()['total'] ?? 0;

// Sensor Alerts
$paper_alerts = $conn->query("SELECT brand_name, paper_size, current_stock FROM paper_settings WHERE physical_status = 'Empty' AND current_stock > 0");
$pen_alerts = $conn->query("SELECT item_name, current_stock FROM ballpen_settings WHERE physical_status = 'Empty' AND current_stock > 0");
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Smart Dashboard - Paper Vendo</title>
    <link rel="stylesheet" href="style.css">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <div class="dashboard-wrapper">
        <?php include 'navbar.php'; ?>
        <div id="content">
            <?php include 'header.php'; ?>
            <div class="container-fluid">
                
                <!-- SENSOR ALERTS -->
                <?php if ($paper_alerts->num_rows > 0 || $pen_alerts->num_rows > 0): ?>
                <div class="card bg-danger text-white border-0 shadow-lg mb-4">
                    <div class="card-body py-3 d-flex align-items-center">
                        <i class="fas fa-exclamation-triangle fa-2x mr-3"></i>
                        <div>
                            <h6 class="m-0 font-weight-bold">Hardware Alert: Panel Empty Detection</h6>
                            <p class="m-0 small">The following sensors report 0 items reached, but the database shows stock. Please refill physical panels.</p>
                            <ul class="m-0 mt-2 small pl-3">
                                <?php while($a = $paper_alerts->fetch_assoc()) echo "<li>{$a['brand_name']} ({$a['paper_size']}): DB shows {$a['current_stock']} sheets left.</li>"; ?>
                                <?php while($a = $pen_alerts->fetch_assoc()) echo "<li>{$a['item_name']}: DB shows {$a['current_stock']} pens left.</li>"; ?>
                            </ul>
                        </div>
                    </div>
                </div>
                <?php endif; ?>

                <div class="row">
                    <div class="col-xl-2 col-md-6 mb-4">
                        <div class="card border-left-success h-100 py-2">
                            <div class="card-body">
                                <div class="text-xs text-success text-uppercase mb-1">Total Sales</div>
                                <div class="h5 mb-0 font-weight-bold">₱<?php echo number_format($total_sales, 2); ?></div>
                            </div>
                        </div>
                    </div>
                    <!-- Other stat cards omitted for brevity/compactness -->
                    <div class="col-xl-2 col-md-6 mb-4">
                        <div class="card border-left-info h-100 py-2">
                            <div class="card-body">
                                <div class="text-xs text-info text-uppercase mb-1">Papers Dispensed</div>
                                <div class="h5 mb-0 font-weight-bold"><?php echo number_format($total_papers); ?></div>
                            </div>
                        </div>
                    </div>
                    <div class="col-xl-4 col-md-12 mb-4">
                        <div class="card border-left-<?php echo $online?'success':'danger'; ?> h-100 py-2">
                            <div class="card-body">
                                <div class="text-xs text-uppercase mb-1">Machine Connection</div>
                                <div class="h5 mb-0 font-weight-bold"><?php echo $online ? "Online (Active)" : "Offline (Disconnected)"; ?></div>
                            </div>
                        </div>
                    </div>
                </div>

                <div class="row">
                    <div class="col-xl-8">
                        <div class="card mb-4">
                            <div class="card-header d-flex justify-content-between">
                                <h6>Weekly Sales Trend</h6>
                            </div>
                            <div class="card-body"><canvas id="myAreaChart"></canvas></div>
                        </div>
                    </div>
                    <div class="col-xl-4">
                        <div class="card mb-4">
                            <div class="card-header"><h6>Live Stock & Sensor Status</h6></div>
                            <div class="card-body">
                                <?php
                                $stock_q = $conn->query("SELECT brand_name, current_stock, max_capacity, physical_status FROM paper_settings");
                                while($row = $stock_q->fetch_assoc()) {
                                    $p = ($row['current_stock']/$row['max_capacity']) * 100;
                                    $c = $row['physical_status'] == 'Empty' ? 'var(--danger)' : 'var(--success)';
                                    echo "<div class='mb-3'>
                                            <div class='d-flex justify-content-between mb-1'>
                                                <small class='font-weight-bold'>{$row['brand_name']}</small>
                                                <span class='badge' style='background:$c; color:white;'>{$row['physical_status']}</span>
                                            </div>
                                            <div class='progress' style='height:6px;'><div style='width:{$p}%; background:var(--primary); height:100%; border-radius:5px;'></div></div>
                                          </div>";
                                }
                                ?>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <script>
    var ctx = document.getElementById("myAreaChart");
    <?php
    $labels = []; $data = [];
    for ($i = 6; $i >= 0; $i--) {
        $d = date('Y-m-d', strtotime("-$i days"));
        $labels[] = date('M d', strtotime("-$i days"));
        $data[] = $conn->query("SELECT SUM(amount_paid) as t FROM sales_transactions WHERE DATE(transaction_date) = '$d'")->fetch_assoc()['t'] ?? 0;
    }
    ?>
    new Chart(ctx, { type: 'line', data: { labels: <?php echo json_encode($labels); ?>, datasets: [{ label: "Earnings", tension:0.4, borderColor: "#4f46e5", data: <?php echo json_encode($data); ?>, fill:true, backgroundColor: "rgba(79, 110, 229, 0.05)" }] }, options: { maintainAspectRatio: false, plugins: { legend: { display: false } } } });
    </script>
</body>
</html>
