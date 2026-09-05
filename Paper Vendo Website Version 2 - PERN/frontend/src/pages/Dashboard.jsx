import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { 
  AreaChart, 
  Area, 
  XAxis, 
  YAxis, 
  CartesianGrid, 
  Tooltip, 
  ResponsiveContainer 
} from 'recharts';
import { 
  TrendingUp, 
  Coins, 
  Boxes, 
  AlertTriangle, 
  Wifi, 
  Radio, 
  RefreshCw 
} from 'lucide-react';

export default function Dashboard() {
  const [kpis, setKpis] = useState({
    totalRevenue: 0,
    totalSales: 0,
    paperSalesCount: 0,
    penSalesCount: 0,
    paperRevenue: 0,
    penRevenue: 0,
    lowStockCount: 0,
    lowStockItems: []
  });
  const [chartData, setChartData] = useState([]);
  const [status, setStatus] = useState([]);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = async () => {
    try {
      const [analyticsRes, statusRes] = await Promise.all([
        axios.get('/api/machine/analytics'),
        axios.get('/api/machine/status')
      ]);
      setKpis(analyticsRes.data.kpis);
      setChartData(analyticsRes.data.chartData);
      setStatus(statusRes.data);
    } catch (err) {
      console.error('Error fetching dashboard data:', err);
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  };

  useEffect(() => {
    fetchData();
    // Auto-poll status every 15 seconds
    const interval = setInterval(fetchData, 15000);
    return () => clearInterval(interval);
  }, []);

  const handleRefresh = () => {
    setRefreshing(true);
    fetchData();
  };

  const getStatusValue = (key) => {
    const item = status.find(s => s.status_key === key);
    return item ? item.status_value : 'Unknown';
  };

  const isOnline = getStatusValue('is_running') === 'Online' || getStatusValue('is_running') === 'Connected';

  if (loading) {
    return (
      <div className="flex h-[70vh] items-center justify-center">
        <div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500"></div>
      </div>
    );
  }

  return (
    <div className="space-y-8 max-w-7xl mx-auto font-sans">
      
      {/* Top Header */}
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div>
          <div className="flex items-center gap-3">
            <div className="flex items-center justify-center w-12 h-12 rounded-2xl bg-white border border-slate-200 dark:border-white/[0.08] p-2 shadow-sm">
              <img src="/logo.png" alt="P&B V Machine Logo" className="w-full h-full object-contain" />
            </div>
            <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
              Control Dashboard
            </h1>
          </div>
          <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
            Real-time analytics and hardware interface overview.
          </p>
        </div>
        <button
          onClick={handleRefresh}
          disabled={refreshing}
          className="self-start flex items-center gap-2 px-4 py-2.5 rounded-xl border border-slate-200 dark:border-white/[0.06] bg-white dark:bg-white/[0.02] text-slate-700 dark:text-slate-300 font-semibold text-sm hover:bg-slate-50 dark:hover:bg-white/[0.04] transition-colors active:scale-[0.98] disabled:opacity-50"
        >
          <RefreshCw className={`w-4 h-4 ${refreshing ? 'animate-spin' : ''}`} />
          <span>Refresh Cloud</span>
        </button>
      </div>

      {/* Hardware Interface Connection */}
      <div className={`p-5 rounded-2xl border transition-all duration-300 ${
        isOnline 
          ? 'bg-emerald-500/5 border-emerald-500/20 text-emerald-700 dark:text-emerald-400' 
          : 'bg-red-500/5 border-red-500/20 text-red-700 dark:text-red-400'
      }`}>
        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <span className={`relative flex h-3 w-3 ${isOnline ? 'text-emerald-500' : 'text-red-500'}`}>
              <span className={`animate-ping absolute inline-flex h-full w-full rounded-full opacity-75 ${isOnline ? 'bg-emerald-400' : 'bg-red-400'}`}></span>
              <span className={`relative inline-flex rounded-full h-3 w-3 ${isOnline ? 'bg-emerald-500' : 'bg-red-500'}`}></span>
            </span>
            <div>
              <h3 className="font-bold text-sm">
                Vendo Machine Link Status: {isOnline ? 'ONLINE' : 'OFFLINE'}
              </h3>
              <p className="text-xs opacity-80 mt-0.5">
                {isOnline 
                  ? 'Your hardware is communicating normally with the database gateway.' 
                  : 'Hardware disconnected. Check power, ESP32 connections, and Wi-Fi networks.'}
              </p>
            </div>
          </div>
          
          <div className="flex items-center gap-6">
            <div className="flex items-center gap-2">
              <Wifi className="w-4 h-4 opacity-70" />
              <span className="text-xs font-semibold">Signal: {getStatusValue('wifi_signal')}</span>
            </div>
            <div className="flex items-center gap-2">
              <Radio className="w-4 h-4 opacity-70" />
              <span className="text-xs font-semibold">
                Updated: {new Date().toLocaleTimeString()}
              </span>
            </div>
          </div>
        </div>
      </div>

      {/* KPIs Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6">
        {/* Total Revenue */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
          <div className="flex items-center justify-between mb-4">
            <span className="p-3 rounded-xl bg-primary-500/10 text-primary-500">
              <Coins className="w-6 h-6" />
            </span>
            <span className="text-xs font-bold text-primary-500 bg-primary-500/10 px-2 py-1 rounded-md flex items-center gap-1">
              <TrendingUp className="w-3.5 h-3.5" /> +12%
            </span>
          </div>
          <h4 className="text-xs font-bold uppercase tracking-wider text-slate-400">Total Earnings</h4>
          <p className="text-3xl font-extrabold font-display text-slate-800 dark:text-white mt-1">
            ₱{kpis.totalRevenue.toFixed(2)}
          </p>
        </div>

        {/* Total Items Dispensed */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
          <div className="flex items-center justify-between mb-4">
            <span className="p-3 rounded-xl bg-emerald-500/10 text-emerald-500">
              <Boxes className="w-6 h-6" />
            </span>
          </div>
          <h4 className="text-xs font-bold uppercase tracking-wider text-slate-400">Total Dispensed</h4>
          <p className="text-3xl font-extrabold font-display text-slate-800 dark:text-white mt-1">
            {kpis.totalSales} <span className="text-xs text-slate-400 font-semibold font-sans">units</span>
          </p>
        </div>

        {/* Paper Sales */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
          <h4 className="text-xs font-bold uppercase tracking-wider text-slate-400 mb-2">Paper Operations</h4>
          <p className="text-3xl font-extrabold font-display text-slate-800 dark:text-white">
            {kpis.paperSalesCount} <span className="text-xs text-slate-400 font-semibold font-sans">dispenses</span>
          </p>
          <div className="mt-4 pt-4 border-t border-slate-100 dark:border-white/[0.04] flex items-center justify-between text-xs text-slate-500 font-semibold">
            <span>Sales Revenue:</span>
            <span className="text-slate-800 dark:text-white">₱{kpis.paperRevenue.toFixed(2)}</span>
          </div>
        </div>

        {/* Pen Sales */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
          <h4 className="text-xs font-bold uppercase tracking-wider text-slate-400 mb-2">Pen Operations</h4>
          <p className="text-3xl font-extrabold font-display text-slate-800 dark:text-white">
            {kpis.penSalesCount} <span className="text-xs text-slate-400 font-semibold font-sans">dispenses</span>
          </p>
          <div className="mt-4 pt-4 border-t border-slate-100 dark:border-white/[0.04] flex items-center justify-between text-xs text-slate-500 font-semibold">
            <span>Sales Revenue:</span>
            <span className="text-slate-800 dark:text-white">₱{kpis.penRevenue.toFixed(2)}</span>
          </div>
        </div>
      </div>

      {/* Warnings & Graph Row */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        
        {/* Sales Chart Area */}
        <div className="lg:col-span-2 p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex flex-col">
          <div className="mb-6">
            <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">Sales Activity History</h3>
            <p className="text-xs text-slate-400 mt-0.5">Showing total revenue and category distributions for the past 7 active days.</p>
          </div>
          
          <div className="flex-1 min-h-[300px]">
            {chartData.length > 0 ? (
              <ResponsiveContainer width="100%" height="100%">
                <AreaChart data={chartData} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                  <defs>
                    <linearGradient id="colorRevenue" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="5%" stopColor="#0ea5e9" stopOpacity={0.4}/>
                      <stop offset="95%" stopColor="#0ea5e9" stopOpacity={0}/>
                    </linearGradient>
                  </defs>
                  <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="rgba(255,255,255,0.03)" />
                  <XAxis 
                    dataKey="date" 
                    stroke="#94a3b8" 
                    fontSize={11}
                    tickLine={false} 
                    axisLine={false} 
                  />
                  <YAxis 
                    stroke="#94a3b8" 
                    fontSize={11}
                    tickLine={false} 
                    axisLine={false} 
                    tickFormatter={(val) => `₱${val}`}
                  />
                  <Tooltip 
                    contentStyle={{ 
                      backgroundColor: '#1e293b', 
                      border: 'none', 
                      borderRadius: '8px',
                      color: '#fff',
                      fontSize: '12px'
                    }}
                    itemStyle={{ color: '#0ea5e9' }}
                    labelStyle={{ fontWeight: 'bold' }}
                    formatter={(value) => [`₱${value}`, 'Daily Revenue']}
                  />
                  <Area 
                    type="monotone" 
                    dataKey="revenue" 
                    stroke="#0ea5e9" 
                    strokeWidth={2}
                    dot={{ r: 3, fill: '#0ea5e9' }}
                    activeDot={{ r: 6 }}
                    fillOpacity={1} 
                    fill="url(#colorRevenue)" 
                  />
                </AreaChart>
              </ResponsiveContainer>
            ) : (
              <div className="flex h-full items-center justify-center text-sm text-slate-400 font-semibold">
                No transaction records available.
              </div>
            )}
          </div>
        </div>

        {/* Low Stock Warning Panel */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex flex-col">
          <div className="flex items-center gap-2 mb-6">
            <AlertTriangle className="w-5 h-5 text-amber-500" />
            <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">Refill Alerts</h3>
          </div>

          <div className="flex-1 space-y-4">
            {kpis.lowStockCount > 0 ? (
              <>
                <div className="p-3.5 rounded-xl border border-amber-500/20 bg-amber-500/5 text-amber-600 dark:text-amber-400 text-xs font-semibold leading-relaxed">
                  Attention! There are {kpis.lowStockCount} item slots currently under the threshold of 15 units remaining. Refill is advised.
                </div>
                
                <div className="space-y-2 overflow-y-auto max-h-[220px] pr-1">
                  {kpis.lowStockItems.map((item, idx) => (
                    <div 
                      key={idx} 
                      className="flex items-center justify-between p-3 rounded-xl border border-slate-100 dark:border-white/[0.03] bg-slate-50 dark:bg-white/[0.01] text-xs"
                    >
                      <span className="font-semibold text-slate-700 dark:text-slate-300">{item}</span>
                      <span className="font-bold text-amber-500 bg-amber-500/10 px-2 py-0.5 rounded-md">Low Stock</span>
                    </div>
                  ))}
                </div>
              </>
            ) : (
              <div className="flex flex-col items-center justify-center h-full text-center p-6 border border-slate-100 dark:border-white/[0.03] rounded-2xl bg-slate-50/50 dark:bg-white/[0.01]">
                <Boxes className="w-10 h-10 text-emerald-500 mb-3" />
                <h4 className="font-bold text-sm text-slate-700 dark:text-slate-200">Inventory Healthy</h4>
                <p className="text-xs text-slate-400 mt-1 max-w-[200px]">All paper configurations and pen brand inventories are loaded above warning thresholds!</p>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
