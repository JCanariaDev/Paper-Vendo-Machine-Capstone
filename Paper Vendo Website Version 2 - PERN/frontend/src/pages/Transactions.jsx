import React, { useEffect, useState } from 'react';
import axios from 'axios';
import { Calendar, Info, Search } from 'lucide-react';

const currency = (value) => `PHP ${Number(value || 0).toFixed(2)}`;

export default function Transactions() {
  const [sales, setSales] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');

  useEffect(() => {
    axios.get('/api/machine/transactions')
      .then((response) => setSales(response.data || []))
      .catch((error) => console.error('Error fetching transaction lines:', error))
      .finally(() => setLoading(false));
  }, []);

  const filteredSales = sales.filter((item) => {
    const searchable = [item.transaction_id, item.item_type, item.product_name, item.paper_size, item.status, item.amount_paid]
      .filter(Boolean).join(' ').toLowerCase();
    return searchable.includes(searchQuery.toLowerCase());
  });

  if (loading) {
    return <div className="flex h-[70vh] items-center justify-center"><div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500" /></div>;
  }

  return (
    <div className="space-y-8 max-w-7xl mx-auto font-sans">
      <div>
        <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">Sales History Logs</h1>
        <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">Verified transaction lines with immutable unit, price, and physical-output records.</p>
      </div>

      <div className="flex flex-col md:flex-row gap-4 justify-between items-stretch">
        <div className="relative flex-1 max-w-md">
          <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400"><Search className="w-5 h-5" /></span>
          <input type="text" placeholder="Search transaction, product, status..." value={searchQuery} onChange={(event) => setSearchQuery(event.target.value)} className="w-full h-11 pl-11 pr-4 rounded-xl text-sm bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500 transition-all" />
        </div>
        <div className="flex items-center gap-2 text-xs font-bold text-slate-400 uppercase tracking-wider bg-slate-50 border border-slate-200 dark:bg-white/[0.01] dark:border-white/[0.05] px-4 py-2.5 rounded-xl self-start md:self-auto">
          <Info className="w-4 h-4 text-primary-500" /><span>Showing {filteredSales.length} of {sales.length} lines</span>
        </div>
      </div>

      <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm overflow-x-auto">
        <table className="w-full text-left text-sm border-collapse">
          <thead><tr className="border-b border-slate-100 dark:border-white/[0.04] text-slate-400 font-bold">
            <th className="py-3 px-4">Transaction</th><th className="py-3 px-4">Product</th><th className="py-3 px-4 text-center">Units</th><th className="py-3 px-4 text-center">Physical output</th><th className="py-3 px-4 text-center">Line subtotal</th><th className="py-3 px-4 text-center">Status</th><th className="py-3 px-4 text-right">Date</th>
          </tr></thead>
          <tbody className="divide-y divide-slate-100 dark:divide-white/[0.03]">
            {filteredSales.map((line) => {
              const isPaper = line.item_type === 'paper';
              const outputLabel = isPaper ? 'sheets' : 'pieces';
              const statusClass = line.status === 'COMPLETED' ? 'text-emerald-500' : line.status?.startsWith('FAILED') ? 'text-red-500' : 'text-amber-500';
              return <tr key={line.id} className="hover:bg-slate-50/50 dark:hover:bg-white/[0.01]">
                <td className="py-4 px-4 font-mono font-semibold text-slate-400 text-xs">#TX-{String(line.transaction_id || line.id).slice(0, 8).toUpperCase()}</td>
                <td className="py-4 px-4 font-semibold text-slate-800 dark:text-white"><span className="block">{line.product_name}</span>{isPaper && <span className="text-xs text-slate-400">{line.paper_size}</span>}</td>
                <td className="py-4 px-4 text-center font-bold">{line.units_requested}</td>
                <td className="py-4 px-4 text-center text-slate-500">{line.qty_dispensed}/{line.qty_requested} {outputLabel}</td>
                <td className="py-4 px-4 text-center font-extrabold text-primary-500">{currency(line.amount_paid)}</td>
                <td className={`py-4 px-4 text-center text-xs font-bold ${statusClass}`}>{line.status}</td>
                <td className="py-4 px-4 text-right text-xs text-slate-500 font-semibold"><span className="inline-flex items-center gap-1.5"><Calendar className="w-3.5 h-3.5" />{new Date(line.transaction_date).toLocaleString()}</span></td>
              </tr>;
            })}
            {!filteredSales.length && <tr><td colSpan="7" className="py-10 text-center font-semibold text-slate-400">No transaction lines found.</td></tr>}
          </tbody>
        </table>
      </div>
    </div>
  );
}
