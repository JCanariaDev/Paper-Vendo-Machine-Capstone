<?php
// header.php
$current_page = basename($_SERVER['PHP_SELF'], ".php");
$page_title = ucfirst($current_page);
if($page_title == "Index") $page_title = "Login";
?>
<div class="topbar">
    <h2 style="text-transform: capitalize;"><?php echo $page_title; ?> Overview</h2>
    
    <div class="user-profile">
        <div class="user-info">
            <span class="user-name">
                <?php echo isset($_SESSION['username']) ? htmlspecialchars($_SESSION['username']) : 'Admin'; ?>
            </span>
            <span class="user-role">
                <?php echo isset($_SESSION['role']) ? ucfirst(htmlspecialchars($_SESSION['role'])) : 'Super Admin'; ?>
            </span>
        </div>
        <div class="user-avatar">
            <?php echo isset($_SESSION['username']) ? strtoupper(substr($_SESSION['username'], 0, 1)) : 'A'; ?>
        </div>
    </div>
</div>
