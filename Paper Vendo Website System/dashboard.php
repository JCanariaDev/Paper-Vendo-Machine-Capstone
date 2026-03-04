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
                
                <!-- HARDWARE STATUS BAR -->
                <div class="row mb-4">
                    <div class="col-xl-3 col-md-6 mb-2">
                        <div class="card status-mini-card">
                            <div class="d-flex align-items-center">
                                <div class="icon-shape bg-primary-soft text-primary mr-3">
                                    <i class="fas fa-wifi"></i>
                                </div>
                                <div>
                                    <div class="small text-muted">WiFi Strength</div>
                                    <div class="font-weight-bold"><?php echo $conn->query("SELECT status_value FROM machine_status WHERE status_key='wifi_signal'")->fetch_assoc()['status_value'] ?? 'N/A'; ?></div>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="col-xl-3 col-md-6 mb-2">
                        <div class="card status-mini-card">
                            <div class="d-flex align-items-center">
                                <div class="icon-shape bg-success-soft text-success mr-3">
                                    <i class="fas fa-microchip"></i>
                                </div>
                                <div>
                                    <div class="small text-muted">System Load</div>
                                    <div class="font-weight-bold">Normal</div>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="col-xl-3 col-md-6 mb-2">
                        <div class="card status-mini-card">
                            <div class="d-flex align-items-center">
                                <div class="icon-shape bg-warning-soft text-warning mr-3">
                                    <i class="fas fa-clock"></i>
                                </div>
                                <div>
                                    <div class="small text-muted">Last Active</div>
                                    <div class="font-weight-bold small"><?php echo date('h:i A', strtotime($last_heart)); ?></div>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="col-xl-3 col-md-6 mb-2">
                        <div class="card status-mini-card">
                            <div class="d-flex align-items-center">
                                <div class="icon-shape bg-danger-soft text-danger mr-3">
                                    <i class="fas fa-bug"></i>
                                </div>
                                <div>
                                    <div class="small text-muted">Errors</div>
                                    <div class="font-weight-bold"><?php echo $conn->query("SELECT status_value FROM machine_status WHERE status_key='current_error'")->fetch_assoc()['status_value'] ?? 'None'; ?></div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- SENSOR ALERTS -->
                <?php if ($paper_alerts->num_rows > 0 || $pen_alerts->num_rows > 0): ?>
                <div class="alert-premium mb-4">
                    <div class="alert-icon bg-danger"><i class="fas fa-exclamation-triangle"></i></div>
                    <div class="alert-content">
                        <strong>Hardware Refill Required</strong>
                        <p>Sensors detect empty slots. Stock mismatch found in: 
                            <?php 
                            $al = [];
                            while($a = $paper_alerts->fetch_assoc()) $al[] = $a['brand_name'] . " (" . $a['paper_size'] . ")";
                            while($a = $pen_alerts->fetch_assoc()) $al[] = $a['item_name'];
                            echo implode(", ", $al);
                            ?>
                        </p>
                    </div>
                </div>
                <?php endif; ?>

                <!-- STATS CARDS -->
                <div class="row">
                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card stat-card-premium border-left-success">
                            <div class="card-body">
                                <div class="row align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs font-weight-bold text-success text-uppercase mb-1">Lifetime Revenue</div>
                                        <div class="h3 mb-0 font-weight-bold">₱<?php echo number_format($total_sales, 2); ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas fa-coins fa-2x text-gray-300"></i>
                                    </div>
                                </div>
                                <div class="mt-2 small text-muted">
                                    Total earnings from all transactions
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card stat-card-premium border-left-primary">
                            <div class="card-body">
                                <div class="row align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs font-weight-bold text-primary text-uppercase mb-1">Today's Sales</div>
                                        <div class="h3 mb-0 font-weight-bold">₱<?php echo number_format($today_sales, 2); ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas fa-wallet fa-2x text-gray-300"></i>
                                    </div>
                                </div>
                                <div class="mt-2 small text-muted">
                                    Collected since 12:00 AM
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card stat-card-premium border-left-info">
                            <div class="card-body">
                                <div class="row align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs font-weight-bold text-info text-uppercase mb-1">Total Dispensed</div>
                                        <div class="h3 mb-0 font-weight-bold"><?php echo number_format($total_papers + $total_pens); ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas fa-print fa-2x text-gray-300"></i>
                                    </div>
                                </div>
                                <div class="mt-2 small text-muted">
                                    Items successfully vended
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card stat-card-premium <?php echo $online?'border-left-success':'border-left-danger'; ?>">
                            <div class="card-body">
                                <div class="row align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs font-weight-bold <?php echo $online?'text-success':'text-danger'; ?> text-uppercase mb-1">Connectivity</div>
                                        <div class="h3 mb-0 font-weight-bold"><?php echo $online ? "Online" : "Offline"; ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas <?php echo $online?'fa-link':'fa-unlink'; ?> fa-2x text-gray-300"></i>
                                    </div>
                                </div>
                                <div class="mt-2 small text-muted">
                                    <?php echo $online ? 'Machine is heartbeating' : 'Last seen ' . $last_heart; ?>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- CHARTS & LIVE STOCK -->
                <div class="row">
                    <div class="col-xl-8 col-lg-7">
                        <div class="card shadow mb-4">
                            <div class="card-header d-flex flex-row align-items-center justify-content-between">
                                <h6 class="m-0 font-weight-bold text-primary">Sales Performance Overview</h6>
                                <div class="badge badge-primary">Last 7 Days</div>
                            </div>
                            <div class="card-body">
                                <div class="chart-area" style="height: 320px;">
                                    <canvas id="myAreaChart"></canvas>
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class="col-xl-4 col-lg-5">
                        <div class="card shadow mb-4">
                            <div class="card-header py-3 d-flex flex-row align-items-center justify-content-between">
                                <h6 class="m-0 font-weight-bold text-primary">Live Inventory Status</h6>
                            </div>
                            <div class="card-body">
                                <?php
                                $stock_q = $conn->query("SELECT brand_name, paper_size, current_stock, max_capacity, physical_status FROM paper_settings");
                                while($row = $stock_q->fetch_assoc()) {
                                    $p = ($row['current_stock']/$row['max_capacity']) * 100;
                                    $c = ($p < 20 || $row['physical_status'] == 'Empty') ? 'var(--danger)' : 'var(--primary)';
                                    $p_status = $row['physical_status'] == 'Empty' ? '<span class="text-danger"><i class="fas fa-times-circle"></i> Empty</span>' : '<span class="text-success"><i class="fas fa-check-circle"></i> Good</span>';
                                    
                                    echo "<div class='mb-4'>
                                            <div class='d-flex justify-content-between mb-1'>
                                                <span class='font-weight-bold small'>{$row['brand_name']} ({$row['paper_size']})</span>
                                                <span class='small font-weight-bold'>{$row['current_stock']} / {$row['max_capacity']}</span>
                                            </div>
                                            <div class='progress' style='height: 8px; border-radius: 10px; background: rgba(0,0,0,0.1);'>
                                                <div class='progress-bar' style='width: {$p}%; background: $c; border-radius: 10px; transition: width 0.5s;'></div>
                                            </div>
                                            <div class='text-right mt-1' style='font-size: 0.7rem;'>
                                                $p_status
                                            </div>
                                          </div>";
                                }
                                ?>
                                <hr>
                                <a href="inventory.php" class="btn btn-primary btn-sm btn-block">Quick Refill</a>
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
    new Chart(ctx, { 
        type: 'line', 
        data: { 
            labels: <?php echo json_encode($labels); ?>, 
            datasets: [{ 
                label: "Earnings", 
                tension: 0.3, 
                borderColor: "#4f46e5", 
                borderWidth: 3,
                pointRadius: 4,
                pointBackgroundColor: "#4f46e5",
                pointBorderColor: "#fff",
                data: <?php echo json_encode($data); ?>, 
                fill: true, 
                backgroundColor: (context) => {
                    const chart = context.chart;
                    const {ctx, chartArea} = chart;
                    if (!chartArea) return null;
                    const gradient = ctx.createLinearGradient(0, chartArea.bottom, 0, chartArea.top);
                    gradient.addColorStop(0, 'rgba(79, 70, 229, 0)');
                    gradient.addColorStop(1, 'rgba(79, 70, 229, 0.1)');
                    return gradient;
                }
            }] 
        }, 
        options: { 
            maintainAspectRatio: false, 
            plugins: { legend: { display: false } },
            scales: {
                y: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#94a3b8' } },
                x: { grid: { display: false }, ticks: { color: '#94a3b8' } }
            }
        } 
    });
    </script>
</body>
</html>
