import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { 
  BarChart, 
  Bar, 
  XAxis, 
  YAxis, 
  CartesianGrid, 
  Tooltip, 
  ResponsiveContainer, 
  PieChart, 
  Pie, 
  Cell, 
  Legend, 
  AreaChart, 
  Area 
} from 'recharts';
import { 
  Clock, 
  Calendar, 
  TrendingUp, 
  DollarSign, 
  ShoppingBag, 
  Percent, 
  AlertTriangle,
  Info
} from 'lucide-react';

export default function Analytics() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  const fetchAnalytics = async () => {
    try {
      setLoading(true);
      const res = await axios.get('/api/machine/analytics');
      setData(res.data);
      setError('');
    } catch (err) {
      console.error('Error fetching analytics data:', err);
      setError('Failed to aggregate advanced analytics. Make sure the database has transactions.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchAnalytics();
  }, []);

  if (loading) {
    return (
      <div className="flex h-[70vh] items-center justify-center">
        <div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500"></div>
      </div>
    );
  }

  if (error || !data) {
    return (
      <div className="max-w-md mx-auto mt-20 p-6 rounded-2xl bg-red-500/10 border border-red-500/20 text-center space-y-4">
        <AlertTriangle className="w-12 h-12 text-red-500 mx-auto" />
        <h3 className="text-lg font-bold text-slate-800 dark:text-white">Analytics Unavailable</h3>
        <p className="text-sm text-slate-500 dark:text-slate-400">{error || 'No analytics data found.'}</p>
        <button 
          onClick={fetchAnalytics}
          className="px-4 py-2 bg-primary-500 hover:bg-primary-600 text-white rounded-xl text-sm font-semibold transition-colors"
        >
          Try Again
        </button>
      </div>
    );
  }

  const { kpis, hourlySales, dayOfWeekSales, productBreakdown } = data;

  // Group the flat breakdown items into 4 main categories for the Donut Chart & List
  const groups = [
    { name: 'Budget Paper', count: 0, units: 0, revenue: 0, items: [] },
    { name: 'Standard Paper', count: 0, units: 0, revenue: 0, items: [] },
    { name: 'Budget Ballpen', count: 0, units: 0, revenue: 0, items: [] },
    { name: 'Standard Ballpen', count: 0, units: 0, revenue: 0, items: [] }
  ];

  productBreakdown?.forEach(item => {
    if (item.type === 'paper') {
      const isBudget = item.brand_name.toLowerCase().includes('budget');
      const g = isBudget ? groups[0] : groups[1];
      g.items.push(item);
      g.count += item.count;
      g.units += item.units;
      g.revenue += item.revenue;
    } else {
      const isBudget = item.item_name.toLowerCase().includes('budget');
      const g = isBudget ? groups[2] : groups[3];
      g.items.push(item);
      g.count += item.count;
      g.units += item.units;
      g.revenue += item.revenue;
    }
  });

  // Custom tooltips and gradients styling
  const COLORS = ['#0EA5E9', '#10B981', '#F59E0B', '#6366F1', '#EC4899'];

  return (
    <div className="space-y-10 max-w-7xl mx-auto font-sans">
      
      {/* Top Header */}
      <div>
        <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
          Advance Analytics
        </h1>
        <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
          Review machine peak performance hours, weekly transactions activity, and item volume distributions.
        </p>
      </div>

      {/* KPI Cards Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6">
        
        {/* KPI 1: Peak Hours */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex items-center gap-4">
          <div className="p-3.5 rounded-xl bg-primary-500/10 text-primary-500 shrink-0">
            <Clock className="w-6 h-6" />
          </div>
          <div>
            <span className="block text-[11px] font-bold text-slate-400 uppercase tracking-wider">Peak Sales Hours</span>
            <span className="block text-lg font-extrabold text-slate-850 dark:text-white mt-0.5 truncate">{kpis.peakHourStr}</span>
          </div>
        </div>

        {/* KPI 2: Peak Day */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex items-center gap-4">
          <div className="p-3.5 rounded-xl bg-emerald-500/10 text-emerald-500 shrink-0">
            <Calendar className="w-6 h-6" />
          </div>
          <div>
            <span className="block text-[11px] font-bold text-slate-400 uppercase tracking-wider">Most Active Day</span>
            <span className="block text-lg font-extrabold text-slate-850 dark:text-white mt-0.5">{kpis.peakDayStr}</span>
          </div>
        </div>

        {/* KPI 3: Avg Transaction Value */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex items-center gap-4">
          <div className="p-3.5 rounded-xl bg-amber-500/10 text-amber-500 shrink-0">
            <DollarSign className="w-6 h-6" />
          </div>
          <div>
            <span className="block text-[11px] font-bold text-slate-400 uppercase tracking-wider">Avg. Ticket Size</span>
            <span className="block text-lg font-extrabold text-slate-850 dark:text-white mt-0.5">₱{parseFloat(kpis.avgTransactionValue).toFixed(2)}</span>
          </div>
        </div>

        {/* KPI 4: Sales Velocity (Items/Transaction) */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex items-center gap-4">
          <div className="p-3.5 rounded-xl bg-indigo-500/10 text-indigo-500 shrink-0">
            <ShoppingBag className="w-6 h-6" />
          </div>
          <div>
            <span className="block text-[11px] font-bold text-slate-400 uppercase tracking-wider">Total Sales count</span>
            <span className="block text-lg font-extrabold text-slate-850 dark:text-white mt-0.5">{kpis.totalSales} items</span>
          </div>
        </div>

      </div>

      {/* Main Aggregated Graphs */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        
        {/* Peak Hours Area Chart */}
        <div className="lg:col-span-2 p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex flex-col justify-between">
          <div className="mb-4">
            <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">Peak Hours Activity Distribution</h3>
            <p className="text-xs text-slate-400 mt-0.5">Hourly transaction volumes and revenue aggregation trends.</p>
          </div>
          <div className="h-72 w-full">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={hourlySales} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                <defs>
                  <linearGradient id="colorHour" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#0EA5E9" stopOpacity={0.2}/>
                    <stop offset="95%" stopColor="#0EA5E9" stopOpacity={0}/>
                  </linearGradient>
                </defs>
                <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="rgba(148, 163, 184, 0.08)" />
                <XAxis dataKey="hour" stroke="#94A3B8" fontSize={10} tickLine={false} />
                <YAxis stroke="#94A3B8" fontSize={10} tickLine={false} />
                <Tooltip 
                  contentStyle={{ 
                    backgroundColor: 'rgba(30, 41, 59, 0.95)', 
                    borderColor: 'rgba(255,255,255,0.06)', 
                    color: '#fff', 
                    borderRadius: '12px',
                    fontSize: '12px'
                  }} 
                />
                <Area type="monotone" dataKey="transactions" stroke="#0EA5E9" strokeWidth={2} fillOpacity={1} fill="url(#colorHour)" name="Transactions" />
              </AreaChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Product Breakdown Donut Chart */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex flex-col justify-between">
          <div className="mb-4">
            <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">Product Item Distribution</h3>
            <p className="text-xs text-slate-400 mt-0.5">Sales breakdown by layout specifications and brand items.</p>
          </div>
          <div className="h-60 w-full relative flex items-center justify-center">
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie
                  data={groups}
                  cx="50%"
                  cy="50%"
                  innerRadius={60}
                  outerRadius={80}
                  paddingAngle={5}
                  dataKey="count"
                >
                  {groups.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={COLORS[index % COLORS.length]} />
                  ))}
                </Pie>
                <Tooltip
                  contentStyle={{ 
                    backgroundColor: 'rgba(30, 41, 59, 0.95)', 
                    borderColor: 'rgba(255,255,255,0.06)', 
                    color: '#fff', 
                    borderRadius: '12px',
                    fontSize: '12px'
                  }} 
                />
              </PieChart>
            </ResponsiveContainer>
          </div>
          
          {/* Custom Labels List with Dynamic Size Breakdown */}
          <div className="mt-2 space-y-3 border-b border-slate-100 dark:border-white/[0.04] pb-4 max-h-[300px] overflow-y-auto pr-1">
            {groups.map((group, idx) => {
              const isPen = group.name.toLowerCase().includes('ballpen');
              const labelUnit = isPen ? (group.count === 1 ? 'piece' : 'pieces') : 'sheets';
              const labelUnits = group.units === 1 ? 'unit' : 'units';
              return (
                <div key={group.name} className="space-y-1.5 p-3 rounded-2xl bg-slate-50/50 dark:bg-slate-900/10 border border-slate-100 dark:border-white/[0.02] hover:border-slate-200 dark:hover:border-white/[0.06] transition-colors">
                  {/* Category Header */}
                  <div className="flex items-center justify-between text-xs font-bold text-slate-800 dark:text-white">
                    <div className="flex items-center gap-2">
                      <span className="w-2.5 h-2.5 rounded-full shrink-0" style={{ backgroundColor: COLORS[idx % COLORS.length] }} />
                      <span>{group.name}</span>
                    </div>
                    <span>{group.units} {labelUnits} (₱{parseFloat(group.revenue).toFixed(2)})</span>
                  </div>

                  {/* Inner Sizes Breakdown */}
                  <div className="pl-4.5 space-y-1">
                    {group.items.map(item => {
                      if (item.count === 0) return null;
                      const itemSize = item.paper_size || 'Standard';
                      const itemLabelUnit = isPen ? (item.count === 1 ? 'piece' : 'pieces') : 'sheets';
                      const itemLabelUnits = item.units === 1 ? 'unit' : 'units';
                      return (
                        <div key={item.id || item.brand_name || item.item_name} className="flex justify-between text-[10px] text-slate-400 dark:text-slate-500 font-medium">
                          <span>• {isPen ? item.item_name : `Size: ${itemSize}`}</span>
                          <span>
                            {item.count} {itemLabelUnit} ({item.units} {itemLabelUnits})
                          </span>
                        </div>
                      );
                    })}
                    {group.count === 0 && (
                      <div className="text-[10px] text-slate-400 dark:text-slate-500 italic pl-2">
                        No sales recorded yet
                      </div>
                    )}
                  </div>
                </div>
              );
            })}
          </div>

          {/* Explanation Alert box */}
          <div className="mt-4 p-3 rounded-xl bg-slate-50 dark:bg-slate-900/50 border border-slate-100 dark:border-white/[0.03] flex gap-2 items-start text-[11px] text-slate-500 dark:text-slate-400 leading-normal">
            <Info className="w-4 h-4 text-primary-500 shrink-0 mt-0.5" />
            <div>
              <span className="font-bold text-slate-700 dark:text-slate-350 block mb-0.5">Units vs. Pieces/Sheets</span>
              For Paper, <b>1 Unit</b> purchased (₱1) yields multiple physical <b>Sheets</b> (e.g. 2–4). For Pen, <b>1 Unit</b> = <b>1 Piece</b>. The count displays physical items dispensed.
            </div>
          </div>

        </div>

      </div>

      {/* Week Day Sales Volume (Bar Chart) */}
      <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
        <div className="mb-6">
          <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">Transactions Volume by Day of Week</h3>
          <p className="text-xs text-slate-400 mt-0.5">Determine the busiest operating days based on weekly transactions count.</p>
        </div>
        <div className="h-72 w-full">
          <ResponsiveContainer width="100%" height="100%">
            <BarChart data={dayOfWeekSales} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
              <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="rgba(148, 163, 184, 0.08)" />
              <XAxis dataKey="day" stroke="#94A3B8" fontSize={11} tickLine={false} />
              <YAxis stroke="#94A3B8" fontSize={11} tickLine={false} />
              <Tooltip 
                contentStyle={{ 
                  backgroundColor: 'rgba(30, 41, 59, 0.95)', 
                  borderColor: 'rgba(255,255,255,0.06)', 
                  color: '#fff', 
                  borderRadius: '12px',
                  fontSize: '12px'
                }} 
              />
              <Bar dataKey="transactions" fill="#10B981" radius={[8, 8, 0, 0]} maxBarSize={45} name="Transactions" />
            </BarChart>
          </ResponsiveContainer>
        </div>
      </div>

    </div>
  );
}
