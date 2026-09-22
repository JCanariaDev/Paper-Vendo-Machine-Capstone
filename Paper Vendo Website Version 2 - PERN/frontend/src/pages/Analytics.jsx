import React, { useState, useEffect, useMemo } from 'react';
import axios from 'axios';
import { 
  BarChart, 
  Bar, 
  XAxis, 
  YAxis, 
  CartesianGrid, 
  Tooltip, 
  ResponsiveContainer, 
  AreaChart, 
  Area 
} from 'recharts';
import { 
  Clock, 
  Calendar, 
  DollarSign, 
  ShoppingBag, 
  AlertTriangle
} from 'lucide-react';

const PAPER_SIZES = [
  { key: '1/4', label: '1/4', sheetsPerUnit: 4, aliases: ['1/4', 'quarter'] },
  { key: 'crosswise', label: 'Crosswise', sheetsPerUnit: 3, aliases: ['crosswise'] },
  { key: 'lengthwise', label: 'Lengthwise', sheetsPerUnit: 3, aliases: ['lengthwise'] },
  { key: '1_whole', label: '1 Whole', sheetsPerUnit: 2, aliases: ['1_whole', '1 whole', 'whole'] },
];

const PAPER_GROUPS = [
  {
    key: 'budget',
    title: 'Budget Paper',
    brand: 'budget',
    unitCost: 1,
    dotClass: 'bg-primary-500',
    iconClass: 'bg-primary-500/10 text-primary-500',
  },
  {
    key: 'standard',
    title: 'Standard Paper',
    brand: 'standard',
    unitCost: 2,
    dotClass: 'bg-emerald-500',
    iconClass: 'bg-emerald-500/10 text-emerald-500',
  },
];

const PEN_GROUPS = [
  {
    key: 'blackPen',
    title: 'Black Ballpen',
    matcher: 'black',
    unitCost: 5,
    dotClass: 'bg-slate-700 dark:bg-slate-300',
    iconClass: 'bg-slate-500/10 text-slate-700 dark:text-slate-300',
  },
  {
    key: 'bluePen',
    title: 'Blue Ballpen',
    matcher: 'blue',
    unitCost: 5,
    dotClass: 'bg-blue-500',
    iconClass: 'bg-blue-500/10 text-blue-500',
  },
  {
    key: 'redPen',
    title: 'Red Ballpen',
    matcher: 'red',
    unitCost: 5,
    dotClass: 'bg-rose-500',
    iconClass: 'bg-rose-500/10 text-rose-500',
  },
];

const createMetrics = () => ({ count: 0, units: 0, revenue: 0 });

const toNumber = (value) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : 0;
};

const formatCurrency = (value) => `₱${toNumber(value).toFixed(2)}`;

const startOfLocalWeek = (date) => {
  const result = new Date(date);
  const day = result.getDay();
  result.setHours(0, 0, 0, 0);
  result.setDate(result.getDate() - (day === 0 ? 6 : day - 1));
  return result;
};

const getAnalyticsRange = (mode, fromDate, toDate) => {
  const now = new Date();
  let start = new Date(now);
  let end = new Date(now);

  if (mode === 'day') {
    start = fromDate ? new Date(`${fromDate}T00:00:00`) : new Date(now.getFullYear(), now.getMonth(), now.getDate());
    end = new Date(start);
  } else if (mode === 'range') {
    start = fromDate ? new Date(`${fromDate}T00:00:00`) : new Date(now.getFullYear(), now.getMonth(), now.getDate());
    end = toDate ? new Date(`${toDate}T00:00:00`) : new Date(start);
  } else if (mode === 'month') {
    start = new Date(now.getFullYear(), now.getMonth(), 1);
    end = new Date(now.getFullYear(), now.getMonth() + 1, 0);
  } else if (mode === 'week') {
    start = startOfLocalWeek(now);
    end = new Date(start);
    end.setDate(start.getDate() + 6);
  }

  start.setHours(0, 0, 0, 0);
  end.setHours(23, 59, 59, 999);
  return { start, end };
};

const inRange = (dateValue, range) => {
  const date = new Date(dateValue);
  return Number.isFinite(date.getTime()) && date >= range.start && date <= range.end;
};

const getSaleHour = (sale) => Number.isInteger(sale?.transaction_local_hour)
  ? sale.transaction_local_hour
  : new Date(sale.transaction_date).getHours();

const getSaleWeekday = (sale) => Number.isInteger(sale?.transaction_local_weekday)
  ? sale.transaction_local_weekday
  : new Date(sale.transaction_date).getDay();

const getItemName = (item) => (
  item?.name ||
  item?.product_name ||
  item?.brand_name ||
  item?.item_name ||
  ''
);

const getItemType = (item) => String(item?.item_type || item?.type || '').toLowerCase();

const resolvePaperSizeKey = (item) => {
  const rawSize = String(item?.paper_size || item?.size || '').toLowerCase().replace(/-/g, ' ').trim();
  const name = String(getItemName(item)).toLowerCase().replace(/-/g, ' ');
  const source = `${rawSize} ${name}`;

  return PAPER_SIZES.find((size) =>
    size.aliases.some((alias) => source.includes(alias.toLowerCase().replace(/-/g, ' ')))
  )?.key;
};

const inferUnits = (item, unitCost) => {
  const units = toNumber(item?.units);
  if (units > 0) return units;

  const revenue = toNumber(item?.revenue ?? item?.amount_paid ?? item?.amount);
  return unitCost > 0 ? revenue / unitCost : 0;
};

const buildProductBoxes = (productBreakdown = []) => {
  const paperBuckets = PAPER_GROUPS.reduce((groups, group) => {
    groups[group.key] = PAPER_SIZES.reduce((sizes, size) => {
      sizes[size.key] = createMetrics();
      return sizes;
    }, {});
    return groups;
  }, {});

  const penBuckets = PEN_GROUPS.reduce((groups, group) => {
    groups[group.key] = createMetrics();
    return groups;
  }, {});

  productBreakdown.forEach((item) => {
    const name = String(getItemName(item)).toLowerCase();
    const itemType = getItemType(item);
    const revenue = toNumber(item?.revenue ?? item?.amount_paid ?? item?.amount);
    const count = toNumber(item?.count ?? item?.qty_dispensed ?? item?.quantity);

    if (itemType === 'paper' || (!name.includes('ballpen') && !name.includes('pen'))) {
      const paperGroup = PAPER_GROUPS.find((group) => name.includes(group.brand));
      const sizeKey = resolvePaperSizeKey(item);

      if (!paperGroup || !sizeKey) return;

      const bucket = paperBuckets[paperGroup.key][sizeKey];
      bucket.count += count;
      bucket.units += inferUnits(item, paperGroup.unitCost);
      bucket.revenue += revenue;
      return;
    }

    const penGroup = PEN_GROUPS.find((group) => name.includes(group.matcher)) || PEN_GROUPS[0];
    if (!penGroup) return;

    const bucket = penBuckets[penGroup.key];
    bucket.count += count;
    bucket.units += inferUnits(item, penGroup.unitCost);
    bucket.revenue += revenue;
  });

  const paperBoxes = PAPER_GROUPS.map((group) => {
    const rows = PAPER_SIZES.map((size) => ({
      ...size,
      ...paperBuckets[group.key][size.key],
    }));

    return {
      ...group,
      rows,
      total: rows.reduce((total, row) => ({
        count: total.count + row.count,
        units: total.units + row.units,
        revenue: total.revenue + row.revenue,
      }), createMetrics()),
    };
  });

  const penBoxes = PEN_GROUPS.map((group) => ({
    ...group,
    ...penBuckets[group.key],
  }));

  return { paperBoxes, penBoxes };
};

export default function Analytics() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [activeProductTab, setActiveProductTab] = useState('paper');
  const [peakFilter, setPeakFilter] = useState('week');
  const [peakFromDate, setPeakFromDate] = useState('');
  const [peakToDate, setPeakToDate] = useState('');
  const [volumeFilter, setVolumeFilter] = useState('week');
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

  const { kpis, productBreakdown } = data;
  const analyticsSales = data.analyticsSales || data.sales || [];
  const { paperBoxes, penBoxes } = buildProductBoxes(productBreakdown);

  const peakSales = useMemo(() => {
    const range = getAnalyticsRange(peakFilter, peakFromDate, peakToDate);
    return analyticsSales.filter((sale) => inRange(sale.transaction_date, range));
  }, [analyticsSales, peakFilter, peakFromDate, peakToDate]);

  const peakHourlySales = useMemo(() => {
    const buckets = Array.from({ length: 24 }, (_, hour) => ({
      hour: `${String(hour).padStart(2, '0')}:00`,
      transactions: 0,
      revenue: 0,
      transactionIds: new Set(),
    }));
    peakSales.forEach((sale) => {
      const hour = getSaleHour(sale);
      if (!buckets[hour]) return;
      buckets[hour].transactionIds.add(sale.transaction_id);
      buckets[hour].revenue += toNumber(sale.amount_paid);
    });
    return buckets.map(({ transactionIds, revenue, ...bucket }) => ({
      ...bucket,
      transactions: transactionIds.size,
      revenue: Number(revenue.toFixed(2)),
    }));
  }, [peakSales]);

  const peakHour = peakHourlySales.reduce((best, bucket) => bucket.transactions > best.transactions ? bucket : best, peakHourlySales[0]);

  const volumeAnalytics = useMemo(() => {
    const range = getAnalyticsRange(volumeFilter);
    const filteredSales = analyticsSales.filter((sale) => inRange(sale.transaction_date, range));
    let buckets;

    if (volumeFilter === 'day') {
      buckets = Array.from({ length: 24 }, (_, hour) => ({ label: `${String(hour).padStart(2, '0')}:00`, transactions: 0, revenue: 0, transactionIds: new Set() }));
    } else if (volumeFilter === 'month') {
      const daysInMonth = new Date(range.start.getFullYear(), range.start.getMonth() + 1, 0).getDate();
      buckets = Array.from({ length: daysInMonth }, (_, index) => ({ label: String(index + 1), transactions: 0, revenue: 0, transactionIds: new Set() }));
    } else {
      buckets = Array.from({ length: 7 }, (_, index) => ({ label: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'][index], transactions: 0, revenue: 0, transactionIds: new Set() }));
    }

    const topItems = new Map();
    filteredSales.forEach((sale) => {
      const date = new Date(sale.transaction_date);
      const bucketIndex = volumeFilter === 'day'
        ? getSaleHour(sale)
        : volumeFilter === 'month'
          ? date.getDate() - 1
          : (getSaleWeekday(sale) + 6) % 7;
      const bucket = buckets[bucketIndex];
      if (bucket) {
        bucket.transactionIds.add(sale.transaction_id);
        bucket.revenue += toNumber(sale.amount_paid);
      }

      const itemKey = `${sale.item_type}-${sale.brand_id || sale.product_name}`;
      const item = topItems.get(itemKey) || { name: sale.product_name, itemType: sale.item_type, units: 0, revenue: 0 };
      item.units += toNumber(sale.qty_dispensed);
      item.revenue += toNumber(sale.amount_paid);
      topItems.set(itemKey, item);
    });

    return {
      filteredSales,
      chart: buckets.map(({ transactionIds, revenue, ...bucket }) => ({ ...bucket, transactions: transactionIds.size, revenue: Number(revenue.toFixed(2)) })),
      topItems: Array.from(topItems.values()).sort((a, b) => b.units - a.units || b.revenue - a.revenue).slice(0, 5),
    };
  }, [analyticsSales, volumeFilter]);

  const volumePeak = volumeAnalytics.chart.reduce((best, bucket) => bucket.transactions > best.transactions ? bucket : best, volumeAnalytics.chart[0]);
  const productBoxes = activeProductTab === 'paper' ? paperBoxes : penBoxes;

  return (
    <div className="space-y-10 max-w-7xl mx-auto font-sans">
      
      {/* Top Header */}
      <div>
        <div className="flex items-center gap-3">
          <div className="flex items-center justify-center w-12 h-12 rounded-2xl bg-white border border-slate-200 dark:border-white/[0.08] p-2 shadow-sm">
            <img src="/logo.png" alt="P&B V Machine Logo" className="w-full h-full object-contain" />
          </div>
          <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
            Advance Analytics
          </h1>
        </div>
        <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
          Review local-time performance, transaction volumes, and the items customers buy most.
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
            <span className="block text-lg font-extrabold text-slate-850 dark:text-white mt-0.5 truncate">{peakHour?.transactions ? peakHour.hour : 'N/A'}</span>
          </div>
        </div>

        {/* KPI 2: Peak Day */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex items-center gap-4">
          <div className="p-3.5 rounded-xl bg-emerald-500/10 text-emerald-500 shrink-0">
            <Calendar className="w-6 h-6" />
          </div>
          <div>
            <span className="block text-[11px] font-bold text-slate-400 uppercase tracking-wider">Most Active Day</span>
            <span className="block text-lg font-extrabold text-slate-850 dark:text-white mt-0.5">{volumePeak?.transactions ? volumePeak.label : 'N/A'}</span>
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
      <div className="grid grid-cols-1 items-start lg:grid-cols-3 gap-6">
        
        {/* Peak Hours Area Chart */}
        <div className="lg:col-span-2 rounded-2xl border border-slate-200 bg-white p-6 shadow-sm dark:border-white/[0.06] dark:bg-[#161F30]">
          <div className="mb-4 flex flex-col gap-3 sm:flex-row sm:items-start sm:justify-between">
            <div>
              <h3 className="font-display text-lg font-bold text-slate-800 dark:text-white">Peak Hour Distribution</h3>
              <p className="mt-0.5 text-xs text-slate-400">Local-time transaction activity for the selected date range.</p>
            </div>
            <div className="flex flex-wrap items-center gap-2">
              <select value={peakFilter} onChange={(event) => setPeakFilter(event.target.value)} className="h-9 rounded-lg border border-slate-200 bg-white px-3 text-xs font-bold text-slate-600 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-slate-200">
                <option value="day">Daily</option><option value="week">Weekly</option><option value="month">Monthly</option><option value="range">Date range</option>
              </select>
              {(peakFilter === 'day' || peakFilter === 'range') && <input type="date" value={peakFromDate} onChange={(event) => setPeakFromDate(event.target.value)} className="h-9 rounded-lg border border-slate-200 bg-white px-2 text-xs font-semibold text-slate-600 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-slate-200" aria-label="Peak hour start date" />}
              {peakFilter === 'range' && <input type="date" value={peakToDate} onChange={(event) => setPeakToDate(event.target.value)} className="h-9 rounded-lg border border-slate-200 bg-white px-2 text-xs font-semibold text-slate-600 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-slate-200" aria-label="Peak hour end date" />}
            </div>
          </div>
          <div className="h-72 w-full">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={peakHourlySales} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                <defs><linearGradient id="colorHour" x1="0" y1="0" x2="0" y2="1"><stop offset="5%" stopColor="#0EA5E9" stopOpacity={0.2}/><stop offset="95%" stopColor="#0EA5E9" stopOpacity={0}/></linearGradient></defs>
                <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="rgba(148, 163, 184, 0.08)" />
                <XAxis dataKey="hour" stroke="#94A3B8" fontSize={10} tickLine={false} interval={1} />
                <YAxis stroke="#94A3B8" fontSize={10} tickLine={false} allowDecimals={false} />
                <Tooltip contentStyle={{ backgroundColor: 'rgba(30, 41, 59, 0.95)', borderColor: 'rgba(255,255,255,0.06)', color: '#fff', borderRadius: '12px', fontSize: '12px' }} />
                <Area type="monotone" dataKey="transactions" stroke="#0EA5E9" strokeWidth={2} dot={{ r: 3, fill: '#0EA5E9' }} activeDot={{ r: 6 }} fillOpacity={1} fill="url(#colorHour)" name="Transactions" />
              </AreaChart>
            </ResponsiveContainer>
          </div>
          <div className="mt-4 flex items-center justify-between rounded-xl bg-primary-50 px-4 py-3 text-sm dark:bg-primary-950/30">
            <span className="font-semibold text-slate-500 dark:text-slate-300">Peak hour</span>
            <span className="font-extrabold text-primary-600 dark:text-primary-300">{peakHour?.transactions ? `${peakHour.hour} · ${peakHour.transactions} transactions` : 'No transactions in this range'}</span>
          </div>
        </div>

        {/* Product Sales Tabs */}
        <div className="h-[420px] min-h-0 overflow-hidden rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
          <div className="flex items-center justify-between gap-3 border-b border-slate-100 px-5 py-4 dark:border-white/[0.06]">
            <div>
              <h3 className="font-display font-bold text-base text-slate-800 dark:text-white">Product Sales</h3>
              <p className="mt-0.5 text-xs text-slate-400">View paper and ballpen performance.</p>
            </div>
            <div className="flex rounded-xl bg-slate-100 p-1 dark:bg-slate-900/60">
              <button type="button" onClick={() => setActiveProductTab('paper')} className={`rounded-lg px-3 py-1.5 text-xs font-bold transition-colors ${activeProductTab === 'paper' ? 'bg-white text-primary-600 shadow-sm dark:bg-[#253149] dark:text-white' : 'text-slate-500 hover:text-slate-800 dark:text-slate-400 dark:hover:text-white'}`}>Paper</button>
              <button type="button" onClick={() => setActiveProductTab('pen')} className={`rounded-lg px-3 py-1.5 text-xs font-bold transition-colors ${activeProductTab === 'pen' ? 'bg-white text-primary-600 shadow-sm dark:bg-[#253149] dark:text-white' : 'text-slate-500 hover:text-slate-800 dark:text-slate-400 dark:hover:text-white'}`}>Ballpens</button>
            </div>
          </div>
          <div className="h-[350px] space-y-4 overflow-y-auto p-4 pr-3">
          {productBoxes.map((box) => {
            const isPaperBox = Boolean(box.rows);
            const total = isPaperBox ? box.total : box;
            const physicalLabel = isPaperBox ? 'sheets' : (total.count === 1 ? 'piece' : 'pieces');
            const unitLabel = total.units === 1 ? 'unit' : 'units';

            return (
              <div key={box.key} className="p-5 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
                <div className="flex items-start justify-between gap-3">
                  <div className="min-w-0">
                    <div className="flex items-center gap-2">
                      <span className={`w-2.5 h-2.5 rounded-full shrink-0 ${box.dotClass}`} />
                      <h3 className="font-display font-bold text-base text-slate-800 dark:text-white truncate">{box.title}</h3>
                    </div>
                    <p className="text-xs text-slate-400 mt-1">
                      {total.count} {physicalLabel} ({total.units} {unitLabel}) - {formatCurrency(total.revenue)}
                    </p>
                  </div>
                  <div className={`p-2.5 rounded-xl shrink-0 ${box.iconClass}`}>
                    <ShoppingBag className="w-5 h-5" />
                  </div>
                </div>

                {isPaperBox ? (
                  <div className="mt-4 space-y-2">
                    {box.rows.map((row) => {
                      const rowUnitLabel = row.units === 1 ? 'unit' : 'units';
                      return (
                        <div key={row.key} className="flex items-center justify-between gap-3 rounded-xl bg-slate-50 dark:bg-slate-900/40 px-3 py-2 text-xs">
                          <div className="min-w-0">
                            <span className="block font-bold text-slate-700 dark:text-slate-200 truncate">{row.label}</span>
                            <span className="block text-[10px] text-slate-400">{row.sheetsPerUnit} sheets per unit</span>
                          </div>
                          <span className="shrink-0 text-right font-bold text-slate-800 dark:text-white">
                            {row.count} sheets ({row.units} {rowUnitLabel}) - {formatCurrency(row.revenue)}
                          </span>
                        </div>
                      );
                    })}
                  </div>
                ) : (
                  <div className="mt-4 rounded-xl bg-slate-50 dark:bg-slate-900/40 px-3 py-2 text-xs font-bold text-slate-800 dark:text-white">
                    {box.count} {physicalLabel} ({box.units} {unitLabel}) - {formatCurrency(box.revenue)}
                  </div>
                )}
              </div>
            );
          })}
          </div>
        </div>

      </div>

      {/* Transaction Volume and Top Items */}
      <div className="rounded-2xl border border-slate-200 bg-white p-6 shadow-sm dark:border-white/[0.06] dark:bg-[#161F30]">
        <div className="mb-6 flex flex-col gap-3 sm:flex-row sm:items-start sm:justify-between">
          <div>
            <h3 className="font-display text-lg font-bold text-slate-800 dark:text-white">Transaction Volumes</h3>
            <p className="mt-0.5 text-xs text-slate-400">Compare completed transaction activity and the most purchased items.</p>
          </div>
          <select value={volumeFilter} onChange={(event) => setVolumeFilter(event.target.value)} className="h-9 rounded-lg border border-slate-200 bg-white px-3 text-xs font-bold text-slate-600 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-[#161F30] dark:text-slate-200">
            <option value="day">Daily</option><option value="week">Weekly</option><option value="month">Monthly</option>
          </select>
        </div>
        <div className="h-72 w-full">
          <ResponsiveContainer width="100%" height="100%">
            <BarChart data={volumeAnalytics.chart} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
              <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="rgba(148, 163, 184, 0.08)" />
              <XAxis dataKey="label" stroke="#94A3B8" fontSize={10} tickLine={false} interval={volumeFilter === 'month' ? 2 : 0} />
              <YAxis stroke="#94A3B8" fontSize={11} tickLine={false} allowDecimals={false} />
              <Tooltip contentStyle={{ backgroundColor: 'rgba(30, 41, 59, 0.95)', borderColor: 'rgba(255,255,255,0.06)', color: '#fff', borderRadius: '12px', fontSize: '12px' }} />
              <Bar dataKey="transactions" fill="#10B981" radius={[8, 8, 0, 0]} maxBarSize={45} name="Transactions" />
            </BarChart>
          </ResponsiveContainer>
        </div>
        <div className="mt-6 border-t border-slate-100 pt-5 dark:border-white/[0.06]">
          <h4 className="mb-3 text-xs font-extrabold uppercase tracking-[0.16em] text-slate-400">Top 5 Most Bought Items</h4>
          {volumeAnalytics.topItems.length ? <div className="grid grid-cols-1 gap-2 md:grid-cols-2 xl:grid-cols-5">{volumeAnalytics.topItems.map((item, index) => <div key={`${item.itemType}-${item.name}`} className="rounded-xl bg-slate-50 px-3 py-3 dark:bg-slate-900/40"><div className="flex items-center justify-between gap-2"><span className="text-xs font-extrabold text-primary-500">#{index + 1}</span><span className="text-xs font-bold text-slate-400">{item.units} sold</span></div><p className="mt-2 truncate text-sm font-extrabold text-slate-700 dark:text-slate-200">{item.name}</p><p className="mt-1 text-xs font-semibold text-slate-400">{formatCurrency(item.revenue)}</p></div>)}</div> : <p className="text-sm font-semibold text-slate-400">No completed transactions in this period.</p>}
        </div>
      </div>

    </div>
  );
}
