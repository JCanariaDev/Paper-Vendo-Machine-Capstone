<?php
// navbar.php
$base_url = basename($_SERVER['PHP_SELF']);

// Fetch machine heartbeat for footer status
$status_q = $conn->query("SELECT status_value FROM machine_status WHERE status_key='last_heartbeat'");
$last_heart = $status_q->fetch_assoc()['status_value'] ?? 'Never';
$is_online = (strtotime($last_heart) > strtotime('-2 minutes'));
?>
<div id="sidebar">
    <div class="sidebar-header">
        <i class="fas fa-robot"></i>
        <span>PAPER VENDO</span>
    </div>
    
    <div class="sidebar-menu-wrapper">
        <div class="menu-label">Main Navigation</div>
        <ul class="sidebar-menu">
            <li>
                <a href="dashboard.php" class="<?php echo ($base_url == 'dashboard.php') ? 'active' : ''; ?>">
                    <i class="fas fa-chart-line"></i> Dashboard
                </a>
            </li>
            <li>
                <a href="inventory.php" class="<?php echo ($base_url == 'inventory.php') ? 'active' : ''; ?>">
                    <i class="fas fa-box-open"></i> Inventory Control
                </a>
            </li>
            <li>
                <a href="transactions.php" class="<?php echo ($base_url == 'transactions.php') ? 'active' : ''; ?>">
                    <i class="fas fa-receipt"></i> Sales History
                </a>
            </li>
        </ul>

        <div class="menu-label">Administration</div>
        <ul class="sidebar-menu">
            <li>
                <a href="users.php" class="<?php echo ($base_url == 'users.php') ? 'active' : ''; ?>">
                    <i class="fas fa-users-cog"></i> User Accounts
                </a>
            </li>
            <li>
                <a href="logs.php" class="<?php echo ($base_url == 'logs.php') ? 'active' : ''; ?>">
                    <i class="fas fa-clipboard-list"></i> System Logs
                </a>
            </li>
            <li>
                <a href="profile.php" class="<?php echo ($base_url == 'profile.php') ? 'active' : ''; ?>">
                    <i class="fas fa-user-shield"></i> My Profile
                </a>
            </li>
        </ul>
    </div>

    <div class="sidebar-footer">
        <div class="machine-status <?php echo $is_online ? 'online' : 'offline'; ?>">
            <div class="status-dot"></div>
            <span>Machine: <?php echo $is_online ? 'CONNECTED' : 'OFFLINE'; ?></span>
        </div>
        <a href="logout.php" class="logout-link">
            <i class="fas fa-sign-out-alt"></i> Logout
        </a>
    </div>
</div>
