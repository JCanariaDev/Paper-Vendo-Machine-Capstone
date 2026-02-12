<?php
require_once 'auth.php'; // Includes session, db, and user details

// Fetch Totals
$total_sales_q = $conn->query("SELECT SUM(amount_paid) as total FROM sales_transactions");
$total_sales = $total_sales_q->fetch_assoc()['total'] ?? 0;

$total_papers_q = $conn->query("SELECT SUM(sheets_dispensed) as total FROM sales_transactions");
$total_papers = $total_papers_q->fetch_assoc()['total'] ?? 0;

// Machine Status
$status_q = $conn->query("SELECT status_value FROM machine_status WHERE status_key='is_running'");
$is_running = $status_q->fetch_assoc()['status_value'] ?? 'Offline';

$heartbeat_q = $conn->query("SELECT status_value FROM machine_status WHERE status_key='last_heartbeat'");
$last_heart = $heartbeat_q->fetch_assoc()['status_value'] ?? 'Never';

// Online detection (2 min threshold)
$online = (strtotime($last_heart) > strtotime('-2 minutes'));
$status_display = $online ? "Online ($is_running)" : "Offline";
$status_color = $online ? "success" : "danger";
$status_icon = $online ? "fa-check-circle" : "fa-times-circle";

// Today's Date
$today = date('Y-m-d');
$today_sales_q = $conn->query("SELECT SUM(amount_paid) as total FROM sales_transactions WHERE DATE(transaction_date) = '$today'");
$today_sales = $today_sales_q->fetch_assoc()['total'] ?? 0;
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Dashboard - Paper Vendo</title>
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
                <!-- Action Buttons -->
                 <div style="display: flex; justify-content: flex-end; margin-bottom: 1.5rem;">
                     <a href="inventory.php" class="btn btn-primary"><i class="fas fa-plus-circle"></i> Manage Stock</a>
                 </div>

                <!-- Stats Rows -->
                <div class="row">
                    <!-- Total Sales -->
                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card border-left-success h-100 py-2">
                            <div class="card-body">
                                <div class="row no-gutters align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs text-success text-uppercase mb-1">Total Earnings</div>
                                        <div class="h5 mb-0 font-weight-bold">₱<?php echo number_format($total_sales, 2); ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas fa-wallet fa-2x" style="color: #d1d5db;"></i>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Today Sales -->
                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card border-left-warning h-100 py-2">
                            <div class="card-body">
                                <div class="row no-gutters align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs text-warning text-uppercase mb-1">Today's Earnings</div>
                                        <div class="h5 mb-0 font-weight-bold">₱<?php echo number_format($today_sales, 2); ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas fa-calendar-check fa-2x" style="color: #d1d5db;"></i>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Papers Dispensed -->
                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card border-left-info h-100 py-2">
                            <div class="card-body">
                                <div class="row no-gutters align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs text-info text-uppercase mb-1">Papers Sold</div>
                                        <div class="h5 mb-0 font-weight-bold"><?php echo number_format($total_papers); ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas fa-file-alt fa-2x" style="color: #d1d5db;"></i>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Machine Status -->
                    <div class="col-xl-3 col-md-6 mb-4">
                        <div class="card border-left-danger h-100 py-2">
                            <div class="card-body">
                                <div class="row no-gutters align-items-center">
                                    <div class="col mr-2">
                                        <div class="text-xs text-<?php echo $status_color; ?> text-uppercase mb-1">System Status</div>
                                        <div class="h5 mb-0 font-weight-bold"><?php echo $status_display; ?></div>
                                    </div>
                                    <div class="col-auto">
                                        <i class="fas <?php echo $status_icon; ?> fa-2x" style="color: #d1d5db;"></i>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Content Row -->
                <div class="row">
                    <!-- Chart -->
                    <div class="col-xl-8">
                        <div class="card mb-4">
                            <div class="card-header">
                                <h6>Sales Overview (Last 7 Days)</h6>
                            </div>
                            <div class="card-body">
                                <div class="chart-area" style="height: 300px;">
                                    <canvas id="myAreaChart"></canvas>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Stock Alert -->
                    <div class="col-xl-4">
                        <div class="card mb-4">
                            <div class="card-header">
                                <h6>Live Stock Levels</h6>
                            </div>
                            <div class="card-body">
                                <?php
                                $stock_q = $conn->query("SELECT brand_name, current_stock, max_capacity FROM paper_settings");
                                while($row = $stock_q->fetch_assoc()) {
                                    $percent = ($row['current_stock'] / $row['max_capacity']) * 100;
                                    $color = ($percent < 20) ? 'bg-danger' : (($percent < 50) ? 'bg-warning' : 'bg-success');
                                    // Map bootstrap bg classes to our custom vars or keep style logic
                                    $styleColor = "var(--success)";
                                    if ($percent < 20) $styleColor = "var(--danger)";
                                    else if ($percent < 50) $styleColor = "var(--warning)";
                                    
                                    echo "<div class='mb-3'>
                                            <div class='d-flex justify-content-between mb-1'>
                                                <span class='font-weight-bold'>{$row['brand_name']}</span>
                                                <span class='text-muted small'>{$row['current_stock']} / {$row['max_capacity']}</span>
                                            </div>
                                            <div class='progress' style='height:8px; background:#f3f4f6; border-radius:10px; overflow:hidden;'>
                                                <div style='width: {$percent}%; height:100%; background-color: $styleColor; border-radius:10px; transition: width 0.5s;'></div>
                                            </div>
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

    <!-- Chart Script -->
    <script>
    // Prepare Data
    var ctx = document.getElementById("myAreaChart");
    
    <?php
    $labels = [];
    $data = [];
    for ($i = 6; $i >= 0; $i--) {
        $d = date('Y-m-d', strtotime("-$i days"));
        $labels[] = date('M d', strtotime("-$i days"));
        $q = $conn->query("SELECT SUM(amount_paid) as t FROM sales_transactions WHERE DATE(transaction_date) = '$d'");
        $data[] = $q->fetch_assoc()['t'] ?? 0;
    }
    ?>

    // Modern Chart Config
    var myLineChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: <?php echo json_encode($labels); ?>,
        datasets: [{
          label: "Earnings",
          tension: 0.4, // Smoother curves
          backgroundColor: "rgba(102, 126, 234, 0.05)",
          borderColor: "#667eea",
          pointRadius: 4,
          pointBackgroundColor: "#fff",
          pointBorderColor: "#667eea",
          pointHoverRadius: 6,
          pointHoverBackgroundColor: "#667eea",
          pointHoverBorderColor: "#fff",
          borderWidth: 3,
          data: <?php echo json_encode($data); ?>,
          fill: true
        }],
      },
      options: {
        maintainAspectRatio: false,
        plugins: {
            legend: { display: false }
        },
        scales: {
          x: { 
              grid: { display: false },
              ticks: { color: '#9ca3af' }
          },
          y: { 
              grid: { color: '#f3f4f6', borderDash: [5, 5] },
              ticks: { color: '#9ca3af', callback: function(value) { return '₱' + value; } } 
          },
        },
        interaction: {
            intersect: false,
            mode: 'index',
        },
      }
    });
    </script>
</body>
</html>
