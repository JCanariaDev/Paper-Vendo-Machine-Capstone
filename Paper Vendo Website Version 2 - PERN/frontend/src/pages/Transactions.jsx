import React, { useCallback, useEffect, useMemo, useState } from 'react';
import axios from 'axios';
import { AlertCircle, Calendar, CheckCircle2, ChevronLeft, ChevronRight, Clock3, Filter, HandCoins, Info, RefreshCw, Search } from 'lucide-react';
import { useNavigate } from 'react-router-dom';

const PAGE_SIZE = 10;
const AUTO_REFRESH_MS = 15000;
const currency = (value) => `PHP ${Number(value || 0).toFixed(2)}`;
const dateKey = (date) => `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;

const startOfWeek = (date) => {
  const result = new Date(date);
  const day = result.getDay();
  result.setHours(0, 0, 0, 0);
  result.setDate(result.getDate() - (day === 0 ? 6 : day - 1));
  return result;
};

const getDateLabel = (date) => {
  const today = new Date();
  const yesterday = new Date(today);
  yesterday.setDate(today.getDate() - 1);
  if (dateKey(date) === dateKey(today)) return 'Today';
  if (dateKey(date) === dateKey(yesterday)) return 'Yesterday';
  return date.toLocaleDateString(undefined, { weekday: 'long', month: 'long', day: 'numeric', year: 'numeric' });
};

const statusDetails = (status) => {
  if (status?.startsWith('FAILED')) return { label: status.replaceAll('_', ' '), tone: 'red', icon: AlertCircle };
  if (status === 'PARTIAL_SUCCESS') return { label: 'PARTIAL SUCCESS', tone: 'amber', icon: AlertCircle };
  if (status === 'REFUNDED') return { label: 'REFUNDED', tone: 'emerald', icon: HandCoins };
  if (status === 'COMPLETED_CHANGE_OWED') return { label: 'CHANGE OWED', tone: 'amber', icon: Clock3 };
  if (status === 'COMPLETED') return { label: 'COMPLETED', tone: 'emerald', icon: CheckCircle2 };
  return { label: status || 'PENDING', tone: 'slate', icon: Clock3 };
};

const statusTone = {
  red: 'border-red-200 bg-red-50 text-red-600 dark:border-red-900/40 dark:bg-red-950/30 dark:text-red-300',
  amber: 'border-amber-200 bg-amber-50 text-amber-600 dark:border-amber-900/40 dark:bg-amber-950/30 dark:text-amber-300',
  emerald: 'border-emerald-200 bg-emerald-50 text-emerald-600 dark:border-emerald-900/40 dark:bg-emerald-950/30 dark:text-emerald-300',
  slate: 'border-slate-200 bg-slate-50 text-slate-500 dark:border-white/[0.08] dark:bg-white/[0.04] dark:text-slate-300',
};

function groupTransactions(lines) {
  const groups = new Map();
  lines.forEach((line) => {
    const key = line.transaction_id || line.tr_number || line.id;
    if (!groups.has(key)) groups.set(key, { ...line, lines: [] });
    groups.get(key).lines.push(line);
  });
  return Array.from(groups.values()).map((transaction) => ({
    ...transaction,
    totalAmount: transaction.lines.reduce((sum, line) => sum + Number(line.amount_paid || 0), 0),
    totalUnits: transaction.lines.reduce((sum, line) => sum + Number(line.units_requested || 0), 0),
    totalDispensed: transaction.lines.reduce((sum, line) => sum + Number(line.qty_dispensed || 0), 0),
    totalRequested: transaction.lines.reduce((sum, line) => sum + Number(line.qty_requested || 0), 0),
    searchText: transaction.lines.map((line) => [line.product_name, line.paper_size, line.item_type, line.status, line.failure_reason, line.amount_paid].filter(Boolean).join(' ')).join(' '),
  }));
}

export default function Transactions() {
  const navigate = useNavigate();
  const [sales, setSales] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [dateFilter, setDateFilter] = useState('all');
  const [specificDate, setSpecificDate] = useState('');
  const [page, setPage] = useState(1);
  const [releasingId, setReleasingId] = useState('');
  const [refundingId, setRefundingId] = useState('');
  const [refreshing, setRefreshing] = useState(false);
  const [lastUpdated, setLastUpdated] = useState(null);

  const loadSales = useCallback(async ({ showRefreshing = false } = {}) => {
    if (showRefreshing) setRefreshing(true);
    try {
      const response = await axios.get('/api/machine/transactions');
      setSales(response.data || []);
      setLastUpdated(new Date());
    } finally {
      if (showRefreshing) setRefreshing(false);
    }
  }, []);

  useEffect(() => {
    loadSales().catch((error) => console.error('Error fetching transaction lines:', error)).finally(() => setLoading(false));
    const refreshTimer = window.setInterval(() => {
      loadSales().catch((error) => console.error('Automatic transaction refresh failed:', error));
    }, AUTO_REFRESH_MS);
    return () => window.clearInterval(refreshTimer);
  }, [loadSales]);

  const transactions = useMemo(() => {
    const now = new Date();
    const today = dateKey(now);
    const weekStart = startOfWeek(now);
    const monthStart = new Date(now.getFullYear(), now.getMonth(), 1);
    const query = searchQuery.trim().toLowerCase();

    return groupTransactions(sales).filter((transaction) => {
      const transactionDate = new Date(transaction.transaction_date);
      const day = dateKey(transactionDate);
      const searchable = [transaction.tr_number, transaction.transaction_id, transaction.status, transaction.failure_reason, transaction.searchText].filter(Boolean).join(' ').toLowerCase();
      const matchesSearch = !query || searchable.includes(query);
      const matchesDate = dateFilter === 'all'
        || (dateFilter === 'today' && day === today)
        || (dateFilter === 'week' && transactionDate >= weekStart)
        || (dateFilter === 'month' && transactionDate >= monthStart)
        || (dateFilter === 'day' && specificDate && day === specificDate);
      return matchesSearch && matchesDate;
    });
  }, [dateFilter, sales, searchQuery, specificDate]);

  useEffect(() => setPage(1), [dateFilter, searchQuery, specificDate]);

  const totalPages = Math.max(1, Math.ceil(transactions.length / PAGE_SIZE));
  const visibleTransactions = transactions.slice((page - 1) * PAGE_SIZE, page * PAGE_SIZE);
  const groupedTransactions = visibleTransactions.reduce((groups, transaction) => {
    const key = dateKey(new Date(transaction.transaction_date));
    if (!groups[key]) groups[key] = { label: getDateLabel(new Date(transaction.transaction_date)), items: [] };
    groups[key].items.push(transaction);
    return groups;
  }, {});

  const releaseChange = async (transactionId) => {
    setReleasingId(transactionId);
    try {
      await axios.post(`/api/machine/transactions/${encodeURIComponent(transactionId)}/release-change`);
      await loadSales();
    } catch (error) {
      console.error('Error releasing transaction change:', error);
      window.alert(error.response?.data?.message || 'Could not release the change record.');
    } finally {
      setReleasingId('');
    }
  };

  const recordFailedDispenseRefund = async (transaction) => {
    const refundAmount = Math.max(0, Number(transaction.credit_received || 0) - Number(transaction.change_paid || 0) - Number(transaction.refund_paid_cents || 0) / 100);
    if (!window.confirm(`Confirm that you handed ${currency(refundAmount)} to the customer for ${transaction.tr_number}. This records the manual refund and cannot run the hopper.`)) return;
    setRefundingId(transaction.transaction_id);
    try {
      await axios.post(`/api/machine/transactions/${encodeURIComponent(transaction.transaction_id)}/record-failed-dispense-refund`);
      await loadSales();
    } catch (error) {
      console.error('Error recording failed-dispense refund:', error);
      window.alert(error.response?.data?.message || 'Could not record the refund.');
    } finally {
      setRefundingId('');
    }
  };

  const updateDateFilter = (value) => {
    setDateFilter(value);
    if (value !== 'day') setSpecificDate('');
  };

  if (loading) return <div className="flex h-[70vh] items-center justify-center"><div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500" /></div>;

  return (
    <div className="mx-auto max-w-7xl space-y-8 font-sans">
      <div className="flex flex-col gap-3 sm:flex-row sm:items-start sm:justify-between">
        <div>
        <div className="flex items-center gap-3"><div className="flex h-12 w-12 items-center justify-center rounded-2xl border border-slate-200 bg-white p-2 shadow-sm dark:border-white/[0.08] dark:bg-[#161F30]"><img src="/logo.png" alt="P&B V Machine Logo" className="h-full w-full object-contain" /></div><h1 className="font-display text-3xl font-extrabold leading-tight text-slate-800 dark:text-white md:text-4xl">Sales History Logs</h1></div>
        <p className="mt-1 text-sm text-slate-500 dark:text-slate-400">Verified transaction receipts with physical output, change tracking, and release confirmation.</p>
        </div>
        <div className="flex items-center gap-3"><span className="text-xs font-semibold text-slate-400">{lastUpdated ? `Auto-refresh: ${lastUpdated.toLocaleTimeString()}` : 'Auto-refreshing...'}</span><button type="button" onClick={() => loadSales({ showRefreshing: true }).catch((error) => { console.error('Manual transaction refresh failed:', error); window.alert('Could not refresh sales history.'); })} disabled={refreshing} className="inline-flex h-10 items-center gap-2 rounded-xl border border-primary-200 bg-primary-50 px-3.5 text-xs font-extrabold text-primary-700 transition hover:bg-primary-100 disabled:cursor-wait disabled:opacity-60 dark:border-primary-800/50 dark:bg-primary-950/30 dark:text-primary-300"><RefreshCw className={`h-4 w-4 ${refreshing ? 'animate-spin' : ''}`} />{refreshing ? 'Refreshing...' : 'Refresh'}</button></div>
      </div>

      <div className="flex flex-col gap-3 lg:flex-row lg:items-center lg:justify-between">
        <div className="flex flex-1 flex-col gap-3 sm:flex-row"><div className="relative flex-1 lg:max-w-xl"><span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400"><Search className="h-5 w-5" /></span><input type="text" placeholder="Search TR No., product, status, or claims..." value={searchQuery} onChange={(event) => setSearchQuery(event.target.value)} className="h-11 w-full rounded-xl border border-slate-200 bg-white pl-11 pr-4 text-sm text-slate-800 outline-none transition-all focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-white" /></div><div className="flex items-center gap-2"><div className="relative flex-1 sm:flex-none"><Filter className="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-primary-500" /><select value={dateFilter} onChange={(event) => updateDateFilter(event.target.value)} className="h-11 w-full appearance-none rounded-xl border border-slate-200 bg-white pl-9 pr-9 text-sm font-semibold text-slate-700 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-white sm:w-40"><option value="all">All dates</option><option value="today">Today</option><option value="week">This week</option><option value="month">This month</option><option value="day">Specific day</option></select></div>{dateFilter === 'day' && <input type="date" value={specificDate} onChange={(event) => setSpecificDate(event.target.value)} className="h-11 rounded-xl border border-slate-200 bg-white px-3 text-sm font-semibold text-slate-700 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-white" aria-label="Choose a specific day" />}</div></div>
        <div className="flex shrink-0 items-center gap-2 rounded-xl border border-slate-200 bg-slate-50 px-4 py-2.5 text-xs font-bold uppercase tracking-wider text-slate-400 dark:border-white/[0.05] dark:bg-white/[0.01]"><Info className="h-4 w-4 text-primary-500" /><span>Showing {transactions.length} of {groupTransactions(sales).length} transactions</span></div>
      </div>

      <div className="rounded-2xl border border-slate-200 bg-white p-4 shadow-sm dark:border-white/[0.06] dark:bg-[#161F30] sm:p-6">
        <div className="max-h-[760px] space-y-6 overflow-y-auto pr-1">
          {Object.keys(groupedTransactions).length ? Object.entries(groupedTransactions).map(([key, group]) => (
            <section key={key}><div className="mb-3 flex items-center gap-3 px-1"><span className="whitespace-nowrap text-xs font-extrabold uppercase tracking-[0.18em] text-slate-400">{group.label}</span><div className="h-px flex-1 bg-slate-100 dark:bg-white/[0.06]" /></div><div className="space-y-3">
              {group.items.map((transaction) => {
                const details = statusDetails(transaction.status);
                const StatusIcon = details.icon;
                const isFailed = transaction.status?.startsWith('FAILED');
                const changeDue = Number(transaction.change_due || 0);
                const changePaid = Number(transaction.change_paid || 0);
                const changeOwed = Math.max(0, changeDue - changePaid);
                const refundPaid = Number(transaction.refund_paid_cents || 0) / 100;
                const refundableCredit = Math.max(0, Number(transaction.credit_received || 0) - changePaid - refundPaid);
                const canRecordRefund = transaction.status === 'FAILED_DISPENSE' && transaction.totalDispensed === 0 && refundableCredit > 0;
                const trNumber = transaction.tr_number || `TR-${String(transaction.transaction_id || transaction.id).slice(0, 5).toUpperCase()}`;
                const transactionId = transaction.transaction_id;

                return <article key={transactionId} className="rounded-2xl border border-slate-200 bg-slate-50/60 p-4 transition hover:border-primary-300 hover:bg-primary-50/30 dark:border-white/[0.07] dark:bg-white/[0.025] dark:hover:border-primary-500/50 dark:hover:bg-primary-500/[0.06] sm:p-5">
                  <div className="flex items-start justify-between gap-4"><span className="inline-flex items-center rounded-lg border border-primary-100 bg-primary-50 px-2.5 py-1 font-mono text-xs font-extrabold text-primary-600 dark:border-primary-800/40 dark:bg-primary-950/40 dark:text-primary-300">{trNumber}</span><span className="inline-flex items-center gap-1.5 text-right text-xs font-semibold text-slate-400"><Calendar className="h-3.5 w-3.5" />{new Date(transaction.transaction_date).toLocaleString()}</span></div>
                  <div className="mt-4 space-y-2">{transaction.lines.map((line) => { const requested = Number(line.qty_requested || 0); const dispensed = Number(line.qty_dispensed || 0); const outcome = dispensed >= requested ? 'Dispensed successfully' : dispensed > 0 ? 'Partially dispensed' : 'Not dispensed'; return <div key={line.id} className="flex items-start justify-between gap-3"><div className="min-w-0"><p className="truncate text-base font-extrabold text-slate-800 dark:text-white">{line.product_name}{line.item_type === 'paper' && line.paper_size ? ` · ${line.paper_size}` : ''}</p><p className="mt-1 text-sm font-semibold text-slate-500 dark:text-slate-400">{line.units_requested} {line.units_requested === 1 ? 'unit' : 'units'} · {dispensed}/{requested} {line.item_type === 'paper' ? 'sheets' : 'pieces'} dispensed</p><p className={`mt-1 text-xs font-extrabold ${outcome === 'Dispensed successfully' ? 'text-emerald-600 dark:text-emerald-300' : outcome === 'Partially dispensed' ? 'text-amber-600 dark:text-amber-300' : 'text-red-600 dark:text-red-300'}`}>{outcome}</p></div><span className="shrink-0 text-xs font-bold text-primary-600 dark:text-primary-300">{currency(line.amount_paid)}</span></div>; })}</div>
                  <div className="mt-4 flex flex-col gap-3 border-t border-slate-200/80 pt-3 dark:border-white/[0.07] sm:flex-row sm:items-center sm:justify-between"><div className="text-xs font-semibold text-slate-500 dark:text-slate-400"><span className="mr-2 uppercase tracking-wider text-slate-400">Change audit</span>{changeOwed > 0 ? <span className="text-amber-600 dark:text-amber-300">Owed {currency(changeOwed)}</span> : changeDue > 0 ? <span className="text-emerald-600 dark:text-emerald-300">Paid {currency(changePaid)}</span> : <span>Exact pay</span>}{refundPaid > 0 && <span className="ml-2 text-emerald-600 dark:text-emerald-300">Credit refund recorded {currency(refundPaid)}</span>}</div><div className="flex flex-wrap items-center gap-2">{changeOwed > 0 && <button type="button" disabled={releasingId === transactionId} onClick={() => releaseChange(transactionId)} className="inline-flex items-center gap-1.5 rounded-full border border-amber-300 bg-amber-100 px-3 py-1.5 text-xs font-extrabold text-amber-700 transition hover:bg-amber-200 disabled:cursor-wait disabled:opacity-50 dark:border-amber-800/50 dark:bg-amber-950/40 dark:text-amber-300"><HandCoins className="h-3.5 w-3.5" />{releasingId === transactionId ? 'Releasing...' : 'Release Change'}</button>}{canRecordRefund && <button type="button" disabled={refundingId === transactionId} onClick={() => recordFailedDispenseRefund(transaction)} className="inline-flex items-center gap-1.5 rounded-full border border-red-300 bg-red-100 px-3 py-1.5 text-xs font-extrabold text-red-700 transition hover:bg-red-200 disabled:cursor-wait disabled:opacity-50 dark:border-red-800/50 dark:bg-red-950/40 dark:text-red-300"><HandCoins className="h-3.5 w-3.5" />{refundingId === transactionId ? 'Recording...' : `Record Refund ${currency(refundableCredit)}`}</button>}{isFailed ? <button type="button" onClick={() => navigate(`/logs?transaction=${encodeURIComponent(trNumber)}`)} className={`inline-flex items-center gap-1.5 rounded-full border px-3 py-1.5 text-xs font-extrabold uppercase tracking-wide ${statusTone[details.tone]} underline decoration-dotted underline-offset-4`} title="Open related machine logs"><StatusIcon className="h-3.5 w-3.5" />{details.label}</button> : <span className={`inline-flex items-center gap-1.5 rounded-full border px-3 py-1.5 text-xs font-extrabold uppercase tracking-wide ${statusTone[details.tone]}`}><StatusIcon className="h-3.5 w-3.5" />{details.label}</span>}</div></div>
                </article>;
              })}
            </div></section>
          )) : <div className="py-16 text-center font-semibold text-slate-400">No transaction records found for the selected filters.</div>}
        </div>
        <div className="mt-5 flex flex-col gap-3 border-t border-slate-100 pt-4 dark:border-white/[0.06] sm:flex-row sm:items-center sm:justify-between"><p className="text-xs font-semibold text-slate-400">Page {Math.min(page, totalPages)} of {totalPages}</p><div className="flex items-center gap-1.5"><button type="button" disabled={page === 1} onClick={() => setPage((current) => Math.max(1, current - 1))} className="inline-flex h-9 items-center gap-1 rounded-lg border border-slate-200 px-3 text-xs font-bold text-slate-500 transition hover:border-primary-300 hover:text-primary-600 disabled:cursor-not-allowed disabled:opacity-40 dark:border-white/[0.08] dark:text-slate-300"><ChevronLeft className="h-4 w-4" />Previous</button>{Array.from({ length: totalPages }, (_, index) => index + 1).map((pageNumber) => <button key={pageNumber} type="button" onClick={() => setPage(pageNumber)} className={`h-9 min-w-9 rounded-lg px-2 text-xs font-extrabold transition ${pageNumber === page ? 'bg-primary-500 text-white shadow-sm' : 'border border-slate-200 text-slate-500 hover:border-primary-300 hover:text-primary-600 dark:border-white/[0.08] dark:text-slate-300'}`}>{pageNumber}</button>)}<button type="button" disabled={page >= totalPages} onClick={() => setPage((current) => Math.min(totalPages, current + 1))} className="inline-flex h-9 items-center gap-1 rounded-lg border border-slate-200 px-3 text-xs font-bold text-slate-500 transition hover:border-primary-300 hover:text-primary-600 disabled:cursor-not-allowed disabled:opacity-40 dark:border-white/[0.08] dark:text-slate-300">Next<ChevronRight className="h-4 w-4" /></button></div></div>
      </div>
    </div>
  );
}
