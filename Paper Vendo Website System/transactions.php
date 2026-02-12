<?php
require_once 'auth.php';
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Transactions - Paper Vendo</title>
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
                    <h1 class="h3 text-gray-800">Sales Transactions</h1>
                    <button onclick="window.print()" class="btn btn-primary btn-sm"><i class="fas fa-print"></i> Print Report</button>
                </div>

                <div class="card shadow mb-4">
                    <div class="card-header py-3">
                        <h6 class="m-0 font-weight-bold text-primary">All Transactions</h6>
                    </div>
                    <div class="card-body">
                        <div class="table-responsive">
                            <table class="table table-bordered" width="100%" cellspacing="0">
                                <thead>
                                    <tr>
                                        <th>Trans ID</th>
                                        <th>Brand</th>
                                        <th>Paper Size</th>
                                        <th>Amount</th>
                                        <th>Sheets</th>
                                        <th>Date & Time</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    $sql = "SELECT t.id, s.brand_name, t.paper_size, t.amount_paid, t.sheets_dispensed, t.transaction_date 
                                            FROM sales_transactions t 
                                            JOIN paper_settings s ON t.brand_id = s.id 
                                            ORDER BY t.transaction_date DESC LIMIT 100";
                                    $result = $conn->query($sql);

                                    if ($result->num_rows > 0) {
                                        while($row = $result->fetch_assoc()) {
                                            $size_label = str_replace(['1/4','crosswise','lengthwise','1_whole'], ['1/4','Crosswise','Lengthwise','1 Whole'], $row['paper_size']);
                                            echo "<tr>
                                                <td>#{$row['id']}</td>
                                                <td>{$row['brand_name']}</td>
                                                <td>{$size_label}</td>
                                                <td class='text-success'>₱{$row['amount_paid']}</td>
                                                <td>{$row['sheets_dispensed']}</td>
                                                <td>{$row['transaction_date']}</td>
                                            </tr>";
                                        }
                                    } else {
                                        echo "<tr><td colspan='6'>No transactions found.</td></tr>";
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
