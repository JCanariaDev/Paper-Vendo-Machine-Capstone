import React, { useState, useEffect, useRef } from 'react';
import axios from 'axios';
import {
  Wifi, WifiOff, RefreshCw, CreditCard, ShoppingBag,
  Tag, Maximize2, CheckCircle, Banknote, PartyPopper, XCircle,
  Activity, AlertTriangle, Layers, ChevronRight
} from 'lucide-react';

// ─── Machine State Definitions ────────────────────────────────────────────────
const STATES = [
  {
    id: 'offline',
    label: 'Machine Offline',
    sublabel: 'No connection to hardware.',
    icon: WifiOff,
    color: 'text-slate-400',
    bgGlow: 'bg-slate-500/10',
    border: 'border-slate-500/30',
    ringColor: 'bg-slate-400',
    badgeColor: 'bg-slate-500/10 text-slate-400 border-slate-500/20',
    hidden: true,
  },
  {
    id: 'idle',
    label: 'Idle — Waiting for Student',
    sublabel: 'Machine is online and ready. Insert coin to begin.',
    icon: Activity,
    color: 'text-emerald-400',
    bgGlow: 'bg-emerald-500/10',
    border: 'border-emerald-500/30',
    ringColor: 'bg-emerald-400',
    badgeColor: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20',
  },
  {
    id: 'inserting_coins',
    label: 'Inserting Coins',
    sublabel: 'Student is depositing coins into the acceptor slot.',
    icon: CreditCard,
    color: 'text-yellow-400',
    bgGlow: 'bg-yellow-500/10',
    border: 'border-yellow-500/30',
    ringColor: 'bg-yellow-400',
    badgeColor: 'bg-yellow-500/10 text-yellow-400 border-yellow-500/20',
  },
  {
    id: 'choosing_item',
    label: 'Choosing Item Type',
    sublabel: 'Student is selecting either Paper or Ballpen.',
    icon: ShoppingBag,
    color: 'text-blue-400',
    bgGlow: 'bg-blue-500/10',
    border: 'border-blue-500/30',
    ringColor: 'bg-blue-400',
    badgeColor: 'bg-blue-500/10 text-blue-400 border-blue-500/20',
  },
  {
    id: 'choosing_brand',
    label: 'Choosing Brand',
    sublabel: 'Student is selecting a brand (Standard / Budget).',
    icon: Tag,
    color: 'text-violet-400',
    bgGlow: 'bg-violet-500/10',
    border: 'border-violet-500/30',
    ringColor: 'bg-violet-400',
    badgeColor: 'bg-violet-500/10 text-violet-400 border-violet-500/20',
  },
  {
    id: 'choosing_size',
    label: 'Choosing Size / Color',
    sublabel: 'Student is selecting paper size or pen color.',
    icon: Maximize2,
    color: 'text-indigo-400',
    bgGlow: 'bg-indigo-500/10',
    border: 'border-indigo-500/30',
    ringColor: 'bg-indigo-400',
    badgeColor: 'bg-indigo-500/10 text-indigo-400 border-indigo-500/20',
  },
  {
    id: 'confirming',
    label: 'Confirming Purchase',
    sublabel: 'Student is reviewing the cart and confirming the order.',
    icon: CheckCircle,
    color: 'text-cyan-400',
    bgGlow: 'bg-cyan-500/10',
    border: 'border-cyan-500/30',
    ringColor: 'bg-cyan-400',
    badgeColor: 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20',
  },
  {
    id: 'dispensing_items',
    label: 'Dispensing Items',
    sublabel: 'Stepper motors are feeding paper / pen from the bay.',
    icon: Layers,
    color: 'text-orange-400',
    bgGlow: 'bg-orange-500/10',
    border: 'border-orange-500/30',
    ringColor: 'bg-orange-400',
    badgeColor: 'bg-orange-500/10 text-orange-400 border-orange-500/20',
  },
  {
    id: 'dispensing_change',
    label: 'Dispensing Change',
    sublabel: 'Coin hopper is releasing change back to the student.',
    icon: Banknote,
    color: 'text-lime-400',
    bgGlow: 'bg-lime-500/10',
    border: 'border-lime-500/30',
    ringColor: 'bg-lime-400',
    badgeColor: 'bg-lime-500/10 text-lime-400 border-lime-500/20',
  },
  {
    id: 'done',
    label: 'Transaction Complete',
    sublabel: 'Receipt shown. Machine returning to idle shortly.',
    icon: PartyPopper,
    color: 'text-emerald-400',
    bgGlow: 'bg-emerald-500/10',
    border: 'border-emerald-500/30',
    ringColor: 'bg-emerald-400',
    badgeColor: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20',
  },
  {
    id: 'failed',
    label: 'Transaction Failed / Cancelled',
    sublabel: 'An error occurred or the student cancelled the transaction.',
    icon: XCircle,
    color: 'text-red-400',
    bgGlow: 'bg-red-500/10',
    border: 'border-red-500/30',
    ringColor: 'bg-red-400',
    badgeColor: 'bg-red-500/10 text-red-400 border-red-500/20',
  },
];

const TIMELINE_STATES = STATES.filter((s) => !s.hidden);

// ─── State Inference ──────────────────────────────────────────────────────────
function inferMachineState(machineStatus, latestTx) {
  const isRunning = machineStatus.find((s) => s.status_key === 'is_running');
  const isOnline =
    isRunning?.status_value === 'Online' ||
    isRunning?.status_value === 'Connected';

  if (!isOnline) return 'offline';
  if (!latestTx) return 'idle';

  const { status, created_at } = latestTx;
  const ageSeconds = (Date.now() - new Date(created_at).getTime()) / 1000;

  if (
    ['COMPLETED', 'COMPLETED_CHANGE_OWED', 'CANCELLED', 'FAILED_DISPENSE', 'FAILED_CHANGE'].includes(status)
  ) {
    if (ageSeconds > 30) return 'idle';
    return status === 'COMPLETED' || status === 'COMPLETED_CHANGE_OWED' ? 'done' : 'failed';
  }
  if (status === 'CHANGE_PAID') return 'dispensing_change';
  if (status === 'RESERVED') {
    if (ageSeconds < 10) return 'inserting_coins';
    if (ageSeconds < 25) return 'choosing_item';
    if (ageSeconds < 40) return 'confirming';
    return 'dispensing_items';
  }
  return 'idle';
}

// ─── TX Status Map ────────────────────────────────────────────────────────────
const TX_MAP = {
  COMPLETED:             { label: 'Completed',         color: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20' },
  COMPLETED_CHANGE_OWED: { label: 'Done (Change Owed)',color: 'bg-lime-500/10 text-lime-400 border-lime-500/20' },
  RESERVED:              { label: 'In Progress',       color: 'bg-yellow-500/10 text-yellow-400 border-yellow-500/20' },
  CHANGE_PAID:           { label: 'Change Released',   color: 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20' },
  CANCELLED:             { label: 'Cancelled',         color: 'bg-slate-500/10 text-slate-400 border-slate-500/20' },
  FAILED_DISPENSE:       { label: 'Failed — Dispense', color: 'bg-red-500/10 text-red-400 border-red-500/20' },
  FAILED_CHANGE:         { label: 'Failed — Change',   color: 'bg-orange-500/10 text-orange-400 border-orange-500/20' },
};

// ─── Sub-components ───────────────────────────────────────────────────────────
function StatusBadge({ label, color }) {
  return (
    <span className={`inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-bold border ${color}`}>
      <span className="w-1.5 h-1.5 rounded-full bg-current animate-pulse" />
      {label}
    </span>
  );
}

function ActivityRow({ tx }) {
  const meta = TX_MAP[tx.status] || { label: tx.status, color: 'bg-slate-500/10 text-slate-400 border-slate-500/20' };
  const time = new Date(tx.created_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  const total = tx.subtotal !== undefined
    ? `₱${Number(tx.subtotal || 0).toFixed(2)}`
    : tx.subtotal_cents ? `₱${(tx.subtotal_cents / 100).toFixed(2)}` : '—';
  return (
    <div className="flex items-center justify-between p-3.5 rounded-xl border border-slate-100 dark:border-white/[0.04] bg-slate-50/50 dark:bg-white/[0.01] text-xs gap-3">
      <div className="flex items-center gap-3 min-w-0">
        <ChevronRight className="w-3.5 h-3.5 text-slate-400 shrink-0" />
        <div className="min-w-0">
          <p className="font-bold text-slate-800 dark:text-white truncate">{tx.tr_number || '—'}</p>
          <p className="text-slate-400 mt-0.5">{time}</p>
        </div>
      </div>
      <div className="flex items-center gap-2 shrink-0">
        <span className="font-bold text-slate-600 dark:text-slate-300">{total}</span>
        <span className={`px-2 py-0.5 rounded-full border font-bold ${meta.color}`}>{meta.label}</span>
      </div>
    </div>
  );
}

// ─── Main Page ────────────────────────────────────────────────────────────────
export default function MachineMonitor() {
  const [machineStatus, setMachineStatus] = useState([]);
  const [transactions, setTransactions]   = useState([]);
  const [stateId, setStateId]             = useState('idle');
  const [lastFetched, setLastFetched]     = useState(null);
  const [countdown, setCountdown]         = useState(5);
  const [loading, setLoading]             = useState(true);
  const [error, setError]                 = useState(null);
  const POLL_INTERVAL = 5;
  const countdownRef = useRef(null);

  const fetchData = async () => {
    try {
      const [statusRes, txRes] = await Promise.all([
        axios.get('/api/machine/status'),
        axios.get('/api/machine/transactions?limit=5'),
      ]);
      const statuses = statusRes.data || [];
      const txList   = txRes.data    || [];
      setMachineStatus(statuses);
      setTransactions(txList);
      setStateId(inferMachineState(statuses, txList[0] || null));
      setLastFetched(new Date());
      setError(null);
    } catch (err) {
      console.error('Monitor fetch error:', err);
      setError('Could not reach the backend. Check your connection.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, POLL_INTERVAL * 1000);
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    setCountdown(POLL_INTERVAL);
    if (countdownRef.current) clearInterval(countdownRef.current);
    countdownRef.current = setInterval(() => {
      setCountdown((prev) => {
        if (prev <= 1) { clearInterval(countdownRef.current); return POLL_INTERVAL; }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(countdownRef.current);
  }, [lastFetched]);

  const activeState       = STATES.find((s) => s.id === stateId) || STATES.find((s) => s.id === 'idle');
  const activeTimelineIdx = TIMELINE_STATES.findIndex((s) => s.id === stateId);
  const isOnline          = stateId !== 'offline';
  const latestTx          = transactions[0];
  const creditInserted    = latestTx?.credit_received !== undefined
    ? Number(latestTx.credit_received || 0).toFixed(2)
    : ((latestTx?.credit_received_cents || 0) / 100).toFixed(2);
  const orderTotal        = latestTx?.subtotal !== undefined
    ? Number(latestTx.subtotal || 0).toFixed(2)
    : ((latestTx?.subtotal_cents || 0) / 100).toFixed(2);
  const changeDue         = latestTx?.change_due !== undefined
    ? Number(latestTx.change_due || 0).toFixed(2)
    : ((latestTx?.change_due_cents || 0) / 100).toFixed(2);
  const transactionOngoing = latestTx && ['RESERVED', 'CHANGE_PAID'].includes(latestTx.status);

  if (loading) {
    return (
      <div className="flex h-[70vh] items-center justify-center">
        <div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500" />
      </div>
    );
  }

  return (
    <div className="space-y-8 max-w-7xl mx-auto font-sans">

      {/* ── Page Header ── */}
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div>
          <div className="flex items-center gap-3">
            <div className="flex items-center justify-center w-12 h-12 rounded-2xl bg-white border border-slate-200 dark:border-white/[0.08] p-2 shadow-sm">
              <img src="/logo.png" alt="P&B V Machine Logo" className="w-full h-full object-contain" />
            </div>
            <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
              Realtime Machine Monitor
            </h1>
          </div>
          <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
            Live view of what the vending machine is currently doing.
          </p>
        </div>
        <div className="flex items-center gap-3">
          <div className="flex items-center gap-1.5 px-3 py-2 rounded-xl bg-slate-100 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.06] text-xs text-slate-500 font-semibold">
            <RefreshCw className="w-3.5 h-3.5" />
            <span>Refreshing in {countdown}s</span>
          </div>
          <button
            onClick={fetchData}
            className="flex items-center gap-2 px-4 py-2.5 rounded-xl border border-slate-200 dark:border-white/[0.06] bg-white dark:bg-white/[0.02] text-slate-700 dark:text-slate-300 font-semibold text-sm hover:bg-slate-50 dark:hover:bg-white/[0.04] transition-colors"
          >
            <RefreshCw className="w-4 h-4" />
            <span>Refresh Now</span>
          </button>
        </div>
      </div>

      {/* ── Error Banner ── */}
      {error && (
        <div className="flex items-center gap-3 p-4 rounded-2xl border border-red-500/30 bg-red-500/5 text-red-400 text-sm font-semibold">
          <AlertTriangle className="w-5 h-5 shrink-0" />
          <span>{error}</span>
        </div>
      )}

      {/* ── Connection Banner ── */}
      <div className={`p-5 rounded-2xl border transition-all duration-500 ${
        isOnline ? 'bg-emerald-500/5 border-emerald-500/20' : 'bg-red-500/5 border-red-500/20'
      }`}>
        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <span className="relative flex h-3.5 w-3.5">
              {isOnline && <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-60" />}
              <span className={`relative inline-flex rounded-full h-3.5 w-3.5 ${isOnline ? 'bg-emerald-500' : 'bg-red-500'}`} />
            </span>
            {isOnline
              ? <Wifi className="w-5 h-5 text-emerald-400" />
              : <WifiOff className="w-5 h-5 text-red-400" />}
            <div>
              <h3 className={`font-bold text-sm ${isOnline ? 'text-emerald-700 dark:text-emerald-400' : 'text-red-700 dark:text-red-400'}`}>
                Machine {isOnline ? 'ONLINE' : 'OFFLINE'}
              </h3>
              <p className="text-xs opacity-70 mt-0.5 text-slate-600 dark:text-slate-400">
                {isOnline
                  ? 'ESP32 gateway is communicating with Supabase.'
                  : 'Hardware disconnected. Check ESP32 power and WiFi.'}
              </p>
            </div>
          </div>
          <StatusBadge label={activeState.label} color={activeState.badgeColor} />
        </div>
      </div>

      {/* ── Horizontal Pipeline ── */}
      <div className="p-5 rounded-2xl border border-slate-200 dark:border-white/[0.08] bg-white dark:bg-[#161F30] shadow-sm">
        <div className="flex items-center justify-between gap-3 mb-5">
          <p className="text-xs font-bold uppercase tracking-widest text-slate-400">Transaction Pipeline</p>
          <span className="text-[10px] text-slate-400 font-semibold">Compact live flow</span>
        </div>

        <div className="overflow-x-auto pb-2">
          <div className="flex min-w-[980px] items-start">
            {TIMELINE_STATES.map((state, idx) => {
              const isPast   = activeTimelineIdx > idx && stateId !== 'failed' && stateId !== 'offline';
              const isActive = stateId === state.id;
              const isLast   = idx === TIMELINE_STATES.length - 1;
              return (
                <div key={state.id} className="flex flex-1 items-start">
                  <div className="flex min-w-[92px] flex-col items-center text-center">
                    <div className={`relative flex items-center justify-center w-10 h-10 rounded-full border-2 shrink-0 transition-all duration-500 ${
                      isActive
                        ? `${state.border} ${state.bgGlow} shadow-lg`
                        : isPast
                        ? 'border-emerald-500/50 bg-emerald-500/10'
                        : 'border-slate-200 dark:border-white/[0.08] bg-slate-50 dark:bg-white/[0.02]'
                    }`}>
                      {isPast
                        ? <CheckCircle className="w-5 h-5 text-emerald-400" />
                        : <state.icon className={`w-5 h-5 transition-colors duration-300 ${isActive ? state.color : 'text-slate-400 dark:text-slate-600'}`} />
                      }
                      {isActive && <span className={`absolute inset-0 rounded-full animate-ping opacity-30 ${state.bgGlow}`} />}
                    </div>
                    <span className={`mt-2 text-[11px] font-bold leading-tight transition-colors duration-300 ${
                      isActive ? state.color : isPast ? 'text-emerald-600 dark:text-emerald-400' : 'text-slate-400 dark:text-slate-600'
                    }`}>
                      {state.label}
                    </span>
                    {isActive && (
                      <span className={`mt-1 text-[9px] font-bold px-2 py-0.5 rounded-full border ${state.badgeColor}`}>
                        ACTIVE
                      </span>
                    )}
                  </div>
                  {!isLast && (
                    <div className={`mt-5 h-0.5 flex-1 min-w-[26px] rounded-full transition-colors duration-500 ${
                      isPast ? 'bg-emerald-500/50' : 'bg-slate-200 dark:bg-white/[0.06]'
                    }`} />
                  )}
                </div>
              );
            })}
          </div>
        </div>
      </div>

      {/* ── Current Activity + Recent Log ── */}
      <div className="grid grid-cols-1 xl:grid-cols-2 gap-6">
        <div className={`relative p-6 rounded-2xl border overflow-hidden transition-all duration-500 ${activeState.border} ${activeState.bgGlow}`}>
          <div className={`absolute -top-10 -right-10 w-40 h-40 rounded-full blur-2xl opacity-25 ${activeState.ringColor}`} />
          <div className="relative">
            <div className="flex items-start justify-between gap-4">
              <div>
                <p className="text-xs font-bold uppercase tracking-widest text-slate-400 mb-3">Current Activity</p>
                <h2 className={`font-display font-extrabold text-2xl ${activeState.color} leading-tight`}>
                  {activeState.label}
                </h2>
                <p className="text-xs text-slate-500 dark:text-slate-400 mt-2 leading-relaxed">
                  {activeState.sublabel}
                </p>
              </div>
              <div className={`inline-flex items-center justify-center w-14 h-14 rounded-2xl ${activeState.bgGlow} border ${activeState.border}`}>
                <activeState.icon className={`w-7 h-7 ${activeState.color}`} />
              </div>
            </div>

            <div className="mt-6 rounded-2xl border border-white/10 bg-slate-950 text-white shadow-inner overflow-hidden">
              <div className="flex items-center justify-between px-4 py-3 border-b border-white/10">
                <span className="text-[11px] font-bold uppercase tracking-widest text-slate-400">TFT-Style Credit Display</span>
                <span className={`text-[10px] font-bold px-2 py-1 rounded-full ${
                  transactionOngoing ? 'bg-emerald-400/10 text-emerald-300' : 'bg-slate-700 text-slate-300'
                }`}>
                  {transactionOngoing ? 'TRANSACTION LIVE' : 'NO ACTIVE ORDER'}
                </span>
              </div>
              <div className="p-5">
                <p className="text-xs font-semibold text-slate-400">Credits Inserted</p>
                <p className="mt-1 font-display text-5xl font-black tracking-tight">
                  ₱{transactionOngoing ? creditInserted : '0.00'}
                </p>
                <div className="mt-5 grid grid-cols-2 gap-3">
                  <div className="rounded-xl bg-white/5 border border-white/10 p-3">
                    <p className="text-[10px] uppercase tracking-wider text-slate-500 font-bold">Order Total</p>
                    <p className="mt-1 text-lg font-extrabold">₱{latestTx ? orderTotal : '0.00'}</p>
                  </div>
                  <div className="rounded-xl bg-white/5 border border-white/10 p-3">
                    <p className="text-[10px] uppercase tracking-wider text-slate-500 font-bold">Change Due</p>
                    <p className="mt-1 text-lg font-extrabold">₱{latestTx ? changeDue : '0.00'}</p>
                  </div>
                </div>
              </div>
            </div>

            {latestTx && (
              <div className="mt-5 grid grid-cols-1 sm:grid-cols-3 gap-3 text-xs">
                <div className="p-3 rounded-xl bg-white/60 dark:bg-white/[0.03] border border-slate-200 dark:border-white/[0.06]">
                  <span className="block text-slate-500 mb-1">TR Number</span>
                  <span className="font-mono font-bold text-slate-800 dark:text-white">{latestTx.tr_number || '—'}</span>
                </div>
                <div className="p-3 rounded-xl bg-white/60 dark:bg-white/[0.03] border border-slate-200 dark:border-white/[0.06]">
                  <span className="block text-slate-500 mb-1">Status</span>
                  <span className={`inline-flex px-2 py-0.5 rounded-full border font-bold text-[10px] ${(TX_MAP[latestTx.status] || {}).color || ''}`}>
                    {(TX_MAP[latestTx.status] || { label: latestTx.status }).label}
                  </span>
                </div>
                <div className="p-3 rounded-xl bg-white/60 dark:bg-white/[0.03] border border-slate-200 dark:border-white/[0.06]">
                  <span className="block text-slate-500 mb-1">Started</span>
                  <span className="font-bold text-slate-800 dark:text-white">
                    {new Date(latestTx.created_at).toLocaleTimeString()}
                  </span>
                </div>
              </div>
            )}
          </div>
        </div>

        <div className="p-6 rounded-2xl border border-slate-200 dark:border-white/[0.08] bg-white dark:bg-[#161F30] shadow-sm">
          <div className="flex items-center justify-between mb-5">
            <p className="text-xs font-bold uppercase tracking-widest text-slate-400">Recent Activity Log</p>
            <span className="text-[10px] text-slate-400 font-semibold">Last 5 transactions</span>
          </div>
          <div className="space-y-2">
            {transactions.length === 0 ? (
              <div className="flex flex-col items-center justify-center py-16 text-slate-400">
                <Activity className="w-8 h-8 mb-2 opacity-40" />
                <span className="text-sm font-semibold">No recent transactions</span>
                <span className="text-xs mt-1 text-slate-500">Machine is idle</span>
              </div>
            ) : (
              transactions.map((tx) => <ActivityRow key={tx.id} tx={tx} />)
            )}
          </div>
        </div>
      </div>

      {/* ── Footer ── */}
      {lastFetched && (
        <p className="text-center text-xs text-slate-400 font-semibold">
          Last synced: {lastFetched.toLocaleTimeString()} · Auto-refreshing every {POLL_INTERVAL}s
        </p>
      )}
    </div>
  );
}
