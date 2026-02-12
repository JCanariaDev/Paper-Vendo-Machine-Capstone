<?php
// sidebar.php
$base_url = basename($_SERVER['PHP_SELF']);
?>
<div id="sidebar">
    <div class="sidebar-header">
        <i class="fas fa-print"></i> Paper Vendo
    </div>
    <ul class="sidebar-menu">
        <li>
            <a href="dashboard.php" class="<?php echo ($base_url == 'dashboard.php') ? 'active' : ''; ?>">
                <i class="fas fa-tachometer-alt"></i> Dashboard
            </a>
        </li>
        <li>
            <a href="inventory.php" class="<?php echo ($base_url == 'inventory.php') ? 'active' : ''; ?>">
                <i class="fas fa-boxes"></i> Inventory
            </a>
        </li>
        <li>
            <a href="transactions.php" class="<?php echo ($base_url == 'transactions.php') ? 'active' : ''; ?>">
                <i class="fas fa-history"></i> Sales History
            </a>
        </li>
        <li>
            <a href="users.php" class="<?php echo ($base_url == 'users.php') ? 'active' : ''; ?>">
                <i class="fas fa-users-cog"></i> Users
            </a>
        </li>
        <li>
            <a href="logs.php" class="<?php echo ($base_url == 'logs.php') ? 'active' : ''; ?>">
                <i class="fas fa-list-alt"></i> Activity Logs
            </a>
        </li>
        <li>
            <a href="logout.php">
                <i class="fas fa-sign-out-alt"></i> Logout
            </a>
        </li>
    </ul>
</div>
