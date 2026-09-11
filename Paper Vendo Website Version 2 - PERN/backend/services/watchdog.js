/**
 * Machine Heartbeat Watchdog Service
 * 
 * Periodically monitors the `machine_online_status` table in Supabase.
 * If the ESP32 has not posted a heartbeat within the configured timeout window
 * (default: 10 seconds), the watchdog automatically transitions the machine status
 * in Supabase to 'Offline'.
 */

let watchdogTimer = null;

export function startMachineWatchdog(supabase) {
  if (watchdogTimer) {
    clearInterval(watchdogTimer);
  }

  const timeoutSeconds = Number.parseInt(process.env.HEARTBEAT_TIMEOUT_SECONDS || '10', 10);
  const timeoutMs = (Number.isFinite(timeoutSeconds) && timeoutSeconds > 0 ? timeoutSeconds : 10) * 1000;
  const checkIntervalMs = 3000; // Check every 3 seconds

  console.log(`[Watchdog] Machine online watchdog started (Timeout: ${timeoutMs / 1000}s, Check Interval: ${checkIntervalMs / 1000}s)`);

  const checkHeartbeat = async () => {
    try {
      const { data, error } = await supabase
        .from('machine_online_status')
        .select('id, status, last_heartbeat, updated_at')
        .eq('id', 1)
        .maybeSingle();

      if (error) {
        // Table might not exist yet if migrations haven't run
        if (error.code !== 'PGRST116') {
          console.warn('[Watchdog] Heartbeat check error:', error.message);
        }
        return;
      }

      if (!data) return;

      // Only act if the machine is marked Online
      if (data.status === 'Online') {
        const lastHeartbeatTime = new Date(data.last_heartbeat).getTime();
        const elapsedMs = Date.now() - lastHeartbeatTime;

        if (elapsedMs > timeoutMs) {
          const { error: updateError } = await supabase
            .from('machine_online_status')
            .update({
              status: 'Offline',
              updated_at: new Date().toISOString()
            })
            .eq('id', 1);

          if (updateError) {
            console.error('[Watchdog] Failed to set machine to Offline:', updateError.message);
          } else {
            console.warn(
              `[Watchdog] Heartbeat expired (${(elapsedMs / 1000).toFixed(1)}s elapsed > ${timeoutMs / 1000}s threshold). Marked machine as OFFLINE.`
            );
          }
        }
      }
    } catch (err) {
      console.error('[Watchdog] Unexpected error during heartbeat check:', err.message);
    }
  };

  // Run initial check, then periodic timer
  checkHeartbeat();
  watchdogTimer = setInterval(checkHeartbeat, checkIntervalMs);

  return watchdogTimer;
}

export function stopMachineWatchdog() {
  if (watchdogTimer) {
    clearInterval(watchdogTimer);
    watchdogTimer = null;
    console.log('[Watchdog] Machine online watchdog stopped.');
  }
}
