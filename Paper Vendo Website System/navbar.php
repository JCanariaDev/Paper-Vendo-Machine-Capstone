<?php
// navbar.php
$base_url = basename($_SERVER['PHP_SELF']);
?>
<div id="sidebar">
    <div class="sidebar-header">
        <i class="fas fa-robot"></i> PAPER VENDO
    </div>
    <ul class="sidebar-menu">
        <li>
            <a href="<?php echo BASE_URL; ?>dashboard.php" class="<?php echo ($base_url == 'dashboard.php') ? 'active' : ''; ?>">
                <i class="fas fa-chart-line"></i> Dashboard
            </a>
        </li>
        <li>
            <a href="<?php echo BASE_URL; ?>inventory.php" class="<?php echo ($base_url == 'inventory.php') ? 'active' : ''; ?>">
                <i class="fas fa-box-open"></i> Inventory
            </a>
        </li>
        <li>
            <a href="<?php echo BASE_URL; ?>transactions.php" class="<?php echo ($base_url == 'transactions.php') ? 'active' : ''; ?>">
                <i class="fas fa-receipt"></i> Sales History
            </a>
        </li>
        <li>
            <a href="<?php echo BASE_URL; ?>users.php" class="<?php echo ($base_url == 'users.php') ? 'active' : ''; ?>">
                <i class="fas fa-users-cog"></i> Users
            </a>
        </li>
        <li>
            <a href="<?php echo BASE_URL; ?>logs.php" class="<?php echo ($base_url == 'logs.php') ? 'active' : ''; ?>">
                <i class="fas fa-clipboard-list"></i> Activity
            </a>
        </li>
        <li>
            <a href="<?php echo BASE_URL; ?>profile.php" class="<?php echo ($base_url == 'profile.php') ? 'active' : ''; ?>">
                <i class="fas fa-user-shield"></i> Profile
            </a>
        </li>
        <li>
            <a href="<?php echo BASE_URL; ?>logout.php">
                <i class="fas fa-sign-out-alt"></i> Logout
            </a>
        </li>
    </ul>
</div>
