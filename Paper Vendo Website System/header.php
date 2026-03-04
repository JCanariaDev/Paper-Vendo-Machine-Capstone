<?php
// header.php
$current_page = basename($_SERVER['PHP_SELF'], ".php");
$page_title = ucfirst($current_page);
if($page_title == "Index") $page_title = "Login Dashboard";

// Fetch machine heartbeat for topbar status
$status_q = $conn->query("SELECT status_value FROM machine_status WHERE status_key='last_heartbeat'");
$last_heart = $status_q->fetch_assoc()['status_value'] ?? 'Never';
$is_online = (strtotime($last_heart) > strtotime('-2 minutes'));
?>
<div class="topbar">
    <div class="breadcrumb-area">
        <h2 style="text-transform: capitalize;"><?php echo $page_title; ?></h2>
        <span class="text-muted small">System / <span class="text-primary"><?php echo $page_title; ?></span></span>
    </div>
    
    <div class="d-flex align-items-center gap-4">
        <!-- Machine Pulse -->
        <div class="machine-pulse-header d-none d-md-flex align-items-center mr-4">
            <div class="machine-status <?php echo $is_online ? 'online' : 'offline'; ?> border-0 bg-transparent p-0 m-0">
                <div class="status-dot"></div>
            </div>
            <span class="small font-weight-bold ml-2 <?php echo $is_online ? 'text-success' : 'text-danger'; ?>">
                <?php echo $is_online ? 'SYNCED' : 'DISCONNECTED'; ?>
            </span>
        </div>

        <div class="user-profile">
            <div class="user-info">
                <span class="user-name">
                    <?php echo isset($_SESSION['username']) ? htmlspecialchars($_SESSION['username']) : 'Admin User'; ?>
                </span>
                <span class="user-role badge badge-primary text-white" style="font-size: 0.6rem; padding: 2px 8px;">
                    <?php echo isset($_SESSION['role']) ? strtoupper(htmlspecialchars($_SESSION['role'])) : 'SUPER ADMIN'; ?>
                </span>
            </div>
            <div class="user-avatar" style="background: linear-gradient(135deg, var(--primary), var(--info));">
                <?php echo isset($_SESSION['username']) ? strtoupper(substr($_SESSION['username'], 0, 1)) : 'A'; ?>
            </div>
        </div>
    </div>
</div>
