import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { Search, Info, Calendar, Download } from 'lucide-react';

export default function Transactions() {
  const [sales, setSales] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');

  const fetchTransactions = async () => {
    try {
      const res = await axios.get('/api/machine/transactions');
      setSales(res.data);
    } catch (err) {
      console.error('Error fetching transactions:', err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchTransactions();
  }, []);

  if (loading) {
    return (
      <div className="flex h-[70vh] items-center justify-center">
        <div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500"></div>
      </div>
    );
  }

  // Filter logic based on type or specifications
  const filteredSales = sales.filter(item => {
    const searchString = `${item.item_type} ${item.paper_size || ''} ${item.amount_paid}`.toLowerCase();
    return searchString.includes(searchQuery.toLowerCase());
  });

  return (
    <div className="space-y-8 max-w-7xl mx-auto font-sans">
      
      {/* Top Header */}
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div>
          <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
            Sales History Logs
          </h1>
          <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
            Browse database transaction logs pushed from your hardware gateway.
          </p>
        </div>
      </div>

      {/* Control Filters Row */}
      <div className="flex flex-col md:flex-row gap-4 justify-between items-stretch">
        {/* Search Bar */}
        <div className="relative flex-1 max-w-md">
          <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400">
            <Search className="w-5 h-5" />
          </span>
          <input
            type="text"
            placeholder="Search by category, layout size, or price..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="w-full h-11 pl-11 pr-4 rounded-xl text-sm bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500 transition-all"
          />
        </div>

        {/* Counter Summary */}
        <div className="flex items-center gap-2 text-xs font-bold text-slate-400 uppercase tracking-wider bg-slate-50 border border-slate-200 dark:bg-white/[0.01] dark:border-white/[0.05] px-4 py-2.5 rounded-xl self-start md:self-auto">
          <Info className="w-4 h-4 text-primary-500" />
          <span>Showing {filteredSales.length} of {sales.length} logs</span>
        </div>
      </div>

      {/* Tables Log Component */}
      <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
        <div className="overflow-x-auto">
          {filteredSales.length > 0 ? (
            <table className="w-full text-left text-sm border-collapse">
              <thead>
                <tr className="border-b border-slate-100 dark:border-white/[0.04] text-slate-400 font-bold">
                  <th className="py-3 px-4">Transaction ID</th>
                  <th className="py-3 px-4">Dispense Category</th>
                  <th className="py-3 px-4">Layout Specification</th>
                  <th className="py-3 px-4 text-center">Dispensed Qty</th>
                  <th className="py-3 px-4 text-center">Amount Received</th>
                  <th className="py-3 px-4 text-right">Transaction Date</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-100 dark:divide-white/[0.03]">
                {filteredSales.map((log) => {
                  const isPaper = log.item_type === 'paper';
                  const dateObj = new Date(log.transaction_date);
                  return (
                    <tr key={log.id} className="hover:bg-slate-50/50 dark:hover:bg-white/[0.01]">
                      <td className="py-4 px-4 font-semibold text-slate-400 text-xs">
                        #TX-{log.id.toString().padStart(6, '0')}
                      </td>
                      <td className="py-4 px-4">
                        <span className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-bold ${
                          isPaper 
                            ? 'bg-primary-500/10 text-primary-500' 
                            : 'bg-emerald-500/10 text-emerald-500'
                        }`}>
                          {isPaper ? 'Paper Dispatch' : 'Pen Dispatch'}
                        </span>
                      </td>
                      <td className="py-4 px-4 font-semibold">
                        {isPaper ? (
                          <span>Size: {log.paper_size || 'Unknown Size'}</span>
                        ) : (
                          <span>Ballpen Item #{log.brand_id}</span>
                        )}
                      </td>
                      <td className="py-4 px-4 text-center font-bold">
                        {log.qty_dispensed} {log.qty_dispensed > 1 ? 'pcs' : 'pc'}
                      </td>
                      <td className="py-4 px-4 text-center font-extrabold text-slate-800 dark:text-white">
                        ₱{parseFloat(log.amount_paid).toFixed(2)}
                      </td>
                      <td className="py-4 px-4 text-right text-xs text-slate-500 font-semibold">
                        <div className="flex items-center justify-end gap-1.5">
                          <Calendar className="w-3.5 h-3.5 shrink-0 opacity-70" />
                          <span>
                            {dateObj.toLocaleDateString()} at {dateObj.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                          </span>
                        </div>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          ) : (
            <div className="flex flex-col items-center justify-center p-12 text-center text-slate-400 font-semibold">
              <Search className="w-10 h-10 mb-3 opacity-30" />
              <span>No transactions found matching your search.</span>
            </div>
          )}
        </div>
      </div>

    </div>
  );
}
