import React, { useEffect, useMemo, useState } from 'react';
import axios from 'axios';
import { Boxes, CalendarDays, PackagePlus, RefreshCw, Search } from 'lucide-react';

const formatDate = (value) => new Date(value).toLocaleString(undefined, {
  dateStyle: 'medium',
  timeStyle: 'short'
});

export default function RefillHistory() {
  const [history, setHistory] = useState([]);
  const [filter, setFilter] = useState('all');
  const [search, setSearch] = useState('');
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);

  const loadHistory = async (showRefresh = false) => {
    if (showRefresh) setRefreshing(true);
    try {
      const response = await axios.get('/api/machine/inventory/refill-history');
      setHistory(response.data || []);
    } catch (error) {
      console.error('Could not load refill history:', error);
    } finally {
      setLoading(false);
      if (showRefresh) setRefreshing(false);
    }
  };

  useEffect(() => {
    loadHistory();
  }, []);

  const visibleHistory = useMemo(() => {
    const query = search.trim().toLowerCase();
    return history.filter((entry) => {
      const matchesType = filter === 'all' || entry.item_type === filter;
      const searchable = [entry.product_name, entry.compartment_number, entry.operation, entry.performed_by]
        .filter(Boolean)
        .join(' ')
        .toLowerCase();
      return matchesType && (!query || searchable.includes(query));
    });
  }, [filter, history, search]);

  return (
    <div className="mx-auto max-w-6xl space-y-8 font-sans">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
        <div>
          <div className="flex items-center gap-3">
            <div className="flex h-12 w-12 items-center justify-center rounded-2xl border border-slate-200 bg-white p-2 shadow-sm dark:border-white/[0.08] dark:bg-[#161F30]
            "><img src="/logo.png" alt="P&B V Machine Logo" className="h-full w-full object-contain" /></div>
            <h1 className="font-display text-3xl font-extrabold leading-tight text-slate-800 dark:text-white md:text-4xl">Refill History</h1>
          </div>
          <p className="mt-2 text-sm text-slate-500 dark:text-slate-400">Track pads and ballpen pieces loaded into each machine compartment.</p>
        </div>
        <button type="button" onClick={() => loadHistory(true)} disabled={refreshing} className="inline-flex h-10 items-center justify-center gap-2 rounded-xl border border-primary-200 bg-primary-50 px-4 text-xs font-extrabold text-primary-700 transition hover:bg-primary-100 disabled:cursor-wait disabled:opacity-60 dark:border-primary-800/50 dark:bg-primary-950/30 dark:text-primary-300">
          <RefreshCw className={`h-4 w-4 ${refreshing ? 'animate-spin' : ''}`} /> Refresh
        </button>
      </div>

      <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
        <div className="relative flex-1 sm:max-w-xl">
          <Search className="pointer-events-none absolute left-3.5 top-1/2 h-5 w-5 -translate-y-1/2 text-slate-400" />
          <input value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Search product, compartment, or operation..." className="h-11 w-full rounded-xl border border-slate-200 bg-white pl-11 pr-4 text-sm text-slate-800 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-white" />
        </div>
        <div className="flex rounded-xl border border-slate-200 bg-white p-1 dark:border-white/[0.08] dark:bg-[#161F30]">
          {['all', 'paper', 'pen'].map((value) => <button key={value} type="button" onClick={() => setFilter(value)} className={`rounded-lg px-4 py-2 text-xs font-extrabold capitalize transition ${filter === value ? 'bg-primary-500 text-white' : 'text-slate-500 hover:bg-slate-100 dark:text-slate-300 dark:hover:bg-white/[0.05]'}`}>{value === 'pen' ? 'Ballpens' : value === 'all' ? 'All' : 'Paper'}</button>)}
        </div>
      </div>

      <div className="rounded-2xl border border-slate-200 bg-white p-4 shadow-sm dark:border-white/[0.06] dark:bg-[#161F30] sm:p-6">
        {loading ? <div className="flex h-48 items-center justify-center"><div className="h-9 w-9 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500" /></div> : visibleHistory.length === 0 ? <div className="flex h-48 flex-col items-center justify-center text-center text-slate-400"><Boxes className="mb-3 h-10 w-10" /><p className="font-semibold">No refill records found.</p><p className="mt-1 text-xs">New records will appear after a compartment is restocked.</p></div> : <div className="max-h-[720px] space-y-3 overflow-y-auto pr-1">{visibleHistory.map((entry) => { const isPaper = entry.item_type === 'paper'; return <article key={entry.id} className="rounded-2xl border border-slate-200 bg-slate-50/70 p-4 transition hover:border-primary-300 dark:border-white/[0.07] dark:bg-white/[0.025] dark:hover:border-primary-500/50"><div className="flex items-start justify-between gap-4"><div className="flex min-w-0 items-start gap-3"><div className={`flex h-10 w-10 shrink-0 items-center justify-center rounded-xl ${isPaper ? 'bg-emerald-100 text-emerald-600 dark:bg-emerald-950/40 dark:text-emerald-300' : 'bg-blue-100 text-blue-600 dark:bg-blue-950/40 dark:text-blue-300'}`}><PackagePlus className="h-5 w-5" /></div><div className="min-w-0"><h2 className="truncate font-extrabold text-slate-800 dark:text-white">{entry.product_name}</h2><p className="mt-1 text-xs font-semibold text-slate-500 dark:text-slate-400">{isPaper ? 'Paper' : 'Ballpen'} compartment {entry.compartment_number}</p></div></div><span className="shrink-0 rounded-full border border-primary-200 bg-primary-50 px-2.5 py-1 text-[10px] font-extrabold uppercase tracking-wide text-primary-700 dark:border-primary-800/50 dark:bg-primary-950/30 dark:text-primary-300">{entry.operation}</span></div><div className="mt-4 grid gap-3 border-t border-slate-200/80 pt-3 text-xs dark:border-white/[0.07] sm:grid-cols-3"><div><p className="font-bold uppercase tracking-wider text-slate-400">Added</p><p className="mt-1 text-sm font-extrabold text-slate-800 dark:text-white">{entry.quantity_added} {entry.quantity_unit}</p></div><div><p className="font-bold uppercase tracking-wider text-slate-400">Bay stock</p><p className="mt-1 text-sm font-extrabold text-slate-800 dark:text-white">{entry.previous_compartment_stock} → {entry.resulting_compartment_stock} {entry.quantity_unit}</p></div><div><p className="flex items-center gap-1 font-bold uppercase tracking-wider text-slate-400"><CalendarDays className="h-3.5 w-3.5" /> Recorded</p><p className="mt-1 text-sm font-semibold text-slate-600 dark:text-slate-300">{formatDate(entry.created_at)}</p></div></div></article>; })}</div>}
      </div>
    </div>
  );
}
