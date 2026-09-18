import React, { useEffect, useMemo, useState } from 'react';
import axios from 'axios';
import { AlertCircle, Calendar, CheckCircle2, Info, RefreshCw, Search, ScrollText, TriangleAlert } from 'lucide-react';

const LEVEL_STYLES = {
  INFO: 'text-sky-500 bg-sky-50 border-sky-200 dark:bg-sky-950/30 dark:border-sky-800/40',
  DEBUG: 'text-slate-500 bg-slate-50 border-slate-200 dark:bg-slate-900/40 dark:border-slate-700',
  WARNING: 'text-amber-500 bg-amber-50 border-amber-200 dark:bg-amber-950/30 dark:border-amber-800/40',
  ERROR: 'text-red-500 bg-red-50 border-red-200 dark:bg-red-950/30 dark:border-red-800/40',
};

function LevelIcon({ level }) {
  if (level === 'ERROR') return <AlertCircle className="w-4 h-4" />;
  if (level === 'WARNING') return <TriangleAlert className="w-4 h-4" />;
  if (level === 'INFO') return <CheckCircle2 className="w-4 h-4" />;
  return <Info className="w-4 h-4" />;
}

export default function Logs() {
  const [logs, setLogs] = useState([]);
  const [level, setLevel] = useState('all');
  const [search, setSearch] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  const fetchLogs = async () => {
    setLoading(true);
    try {
      const response = await axios.get('/api/machine/logs?limit=500');
      setLogs(response.data || []);
      setError('');
    } catch (err) {
      setError(err.response?.data?.message || 'Could not retrieve machine logs.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchLogs(); }, []);

  const filteredLogs = useMemo(() => logs.filter((log) => {
    if (level !== 'all' && log.level !== level) return false;
    const searchable = [log.source, log.event_type, log.message, log.tr_number, log.transaction_id]
      .filter(Boolean).join(' ').toLowerCase();
    return searchable.includes(search.toLowerCase());
  }), [logs, level, search]);

  return (
    <div className="space-y-8 max-w-7xl mx-auto font-sans">
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div className="flex items-center gap-3">
          <div className="flex items-center justify-center w-12 h-12 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.08] p-2 shadow-sm">
            <ScrollText className="w-7 h-7 text-primary-500" />
          </div>
          <div>
            <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">Machine Logs</h1>
            <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">Transaction, controller, and database events.</p>
          </div>
        </div>
        <button onClick={fetchLogs} className="inline-flex items-center justify-center gap-2 h-11 px-4 rounded-xl bg-primary-500 text-white font-bold text-sm hover:bg-primary-600 transition-colors">
          <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} /> Refresh
        </button>
      </div>

      <div className="flex flex-col md:flex-row gap-4">
        <div className="relative flex-1">
          <Search className="absolute left-3.5 top-1/2 -translate-y-1/2 w-5 h-5 text-slate-400" />
          <input value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Search source, event, message, or transaction..." className="w-full h-11 pl-11 pr-4 rounded-xl text-sm bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500" />
        </div>
        <select value={level} onChange={(event) => setLevel(event.target.value)} className="h-11 px-4 rounded-xl text-sm font-semibold bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.08] text-slate-800 dark:text-white outline-none">
          <option value="all">All levels</option>
          <option value="INFO">Info</option>
          <option value="WARNING">Warning</option>
          <option value="ERROR">Error</option>
          <option value="DEBUG">Debug</option>
        </select>
      </div>

      {error && <div className="p-4 rounded-xl border border-red-200 bg-red-50 text-red-600 dark:bg-red-950/30 dark:border-red-800/40 dark:text-red-300 text-sm font-semibold">{error}</div>}

      <div className="rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm overflow-hidden">
        <div className="px-6 py-4 border-b border-slate-100 dark:border-white/[0.05] text-xs font-bold uppercase tracking-widest text-slate-400">{filteredLogs.length} events</div>
        {loading ? (
          <div className="flex h-48 items-center justify-center"><RefreshCw className="w-8 h-8 animate-spin text-primary-500" /></div>
        ) : filteredLogs.length === 0 ? (
          <div className="py-16 text-center text-sm font-semibold text-slate-400">No machine logs found.</div>
        ) : (
          <div className="divide-y divide-slate-100 dark:divide-white/[0.04]">
            {filteredLogs.map((log) => (
              <div key={log.id} className="px-6 py-4 flex flex-col lg:flex-row lg:items-center gap-3 lg:gap-6 hover:bg-slate-50/60 dark:hover:bg-white/[0.01]">
                <span className={`inline-flex items-center gap-1.5 w-fit px-2.5 py-1 rounded-full border text-[11px] font-bold ${LEVEL_STYLES[log.level] || LEVEL_STYLES.INFO}`}><LevelIcon level={log.level} />{log.level}</span>
                <div className="min-w-0 flex-1">
                  <div className="flex flex-wrap items-center gap-2 text-xs font-bold uppercase tracking-wider text-slate-400">
                    <span>{log.source}</span><span>•</span><span>{log.event_type}</span>
                  </div>
                  <p className="mt-1 text-sm font-semibold text-slate-800 dark:text-white">{log.message}</p>
                  {log.tr_number && <p className="mt-1 text-xs font-bold text-slate-400">{log.tr_number}</p>}
                </div>
                <span className="inline-flex items-center gap-1.5 text-xs text-slate-400 whitespace-nowrap"><Calendar className="w-3.5 h-3.5" />{new Date(log.created_at).toLocaleString()}</span>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
