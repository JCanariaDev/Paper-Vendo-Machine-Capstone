import express from 'express';
import { authenticateToken, authorizeRoles } from '../middleware/auth.js';

const asInt = (value, fallback = 0) => {
  const parsed = Number.parseInt(value, 10);
  return Number.isFinite(parsed) ? parsed : fallback;
};

const asMoney = (cents) => asInt(cents) / 100;

function flattenPaperSetting(row) {
  const channel = row.paper_channels || {};
  return {
    ...row,
    paper_channel_id: row.paper_channel_id,
    channel_code: channel.channel_code,
    motor_channel: channel.motor_channel,
    sensor_channel: channel.sensor_channel,
    cost_per_unit: asMoney(row.cost_per_unit_cents),
    current_stock: asInt(channel.current_sheet_stock),
    reserved_stock: asInt(channel.reserved_sheet_stock),
    max_capacity: asInt(channel.max_sheet_capacity),
    physical_status: channel.physical_status || 'Unknown'
  };
}

function flattenPenSetting(row) {
  return {
    ...row,
    cost_per_unit: asMoney(row.cost_per_unit_cents),
    current_stock: asInt(row.current_stock),
    reserved_stock: asInt(row.reserved_stock),
    max_capacity: asInt(row.max_capacity)
  };
}

function flattenTransactionLine(line) {
  const transaction = line.sales_transactions || {};
  return {
    id: line.id,
    transaction_id: transaction.id,
    status: transaction.status,
    item_type: line.item_type,
    brand_id: line.product_id,
    product_name: line.product_name,
    paper_size: line.paper_size,
    physical_channel: line.physical_channel,
    units_requested: asInt(line.units_requested),
    sheets_per_unit_snapshot: asInt(line.sheets_per_unit_snapshot, 1),
    qty_requested: asInt(line.qty_requested),
    qty_dispensed: asInt(line.qty_dispensed),
    unit_price_cents: asInt(line.unit_price_cents),
    line_total_cents: asInt(line.unit_price_cents) * asInt(line.units_requested),
    amount_paid: asMoney(asInt(line.unit_price_cents) * asInt(line.units_requested)),
    credit_received: asMoney(transaction.credit_received_cents),
    subtotal: asMoney(transaction.subtotal_cents),
    change_due: asMoney(transaction.change_due_cents),
    transaction_date: transaction.created_at,
    completed_at: transaction.completed_at,
    line_status: line.line_status
  };
}

async function getInventory(supabase) {
  const [paperResult, penResult] = await Promise.all([
    supabase.from('paper_settings').select('*, paper_channels(*)').order('id', { ascending: true }),
    supabase.from('ballpen_settings').select('*').order('id', { ascending: true })
  ]);
  if (paperResult.error) throw paperResult.error;
  if (penResult.error) throw penResult.error;
  return {
    paper: (paperResult.data || []).map(flattenPaperSetting),
    pen: (penResult.data || []).map(flattenPenSetting)
  };
}

async function getTransactionLines(supabase) {
  const { data, error } = await supabase
    .from('sales_transaction_lines')
    .select('*, sales_transactions!inner(id, status, credit_received_cents, subtotal_cents, change_due_cents, created_at, completed_at)');
  if (error) throw error;
  return (data || [])
    .map(flattenTransactionLine)
    .sort((a, b) => new Date(b.transaction_date) - new Date(a.transaction_date));
}

export function createMachineRouter(supabase) {
  const router = express.Router();
  router.use(authenticateToken);

  router.get('/status', async (_req, res) => {
    try {
      const { data, error } = await supabase.from('machine_status').select('*');
      if (error) throw error;
      return res.status(200).json(data);
    } catch (err) {
      console.error('Error fetching status:', err);
      return res.status(500).json({ message: 'Failed to retrieve machine status.' });
    }
  });

  router.get('/inventory', async (_req, res) => {
    try {
      return res.status(200).json(await getInventory(supabase));
    } catch (err) {
      console.error('Error fetching inventory:', err);
      return res.status(500).json({ message: 'Failed to retrieve inventory.' });
    }
  });

  router.get('/transactions', async (req, res) => {
    const { limit, startDate, endDate, itemType, status } = req.query;
    try {
      let lines = await getTransactionLines(supabase);
      if (startDate) lines = lines.filter((line) => new Date(line.transaction_date) >= new Date(`${startDate}T00:00:00Z`));
      if (endDate) lines = lines.filter((line) => new Date(line.transaction_date) <= new Date(`${endDate}T23:59:59Z`));
      if (itemType && itemType !== 'all') lines = lines.filter((line) => line.item_type === itemType);
      if (status && status !== 'all') lines = lines.filter((line) => line.status === status);
      if (limit) lines = lines.slice(0, asInt(limit));
      return res.status(200).json(lines);
    } catch (err) {
      console.error('Error fetching transactions:', err);
      return res.status(500).json({ message: 'Failed to retrieve sales transactions.' });
    }
  });

  router.put('/paper/:id', authorizeRoles('superadmin'), async (req, res) => {
    const { id } = req.params;
    const { brand_name, paper_size, cost_per_unit, sheets_per_unit, current_stock, max_capacity, physical_status, active } = req.body;
    try {
      const { data: existing, error: existingError } = await supabase
        .from('paper_settings').select('paper_channel_id').eq('id', id).single();
      if (existingError) throw existingError;

      const { data, error } = await supabase
        .from('paper_settings')
        .update({
          brand_name,
          paper_size,
          cost_per_unit_cents: Math.round(Number(cost_per_unit) * 100),
          sheets_per_unit: asInt(sheets_per_unit, 1),
          active: active !== false
        })
        .eq('id', id)
        .select('*, paper_channels(*)');
      if (error) throw error;

      const { error: channelError } = await supabase
        .from('paper_channels')
        .update({
          current_sheet_stock: asInt(current_stock),
          max_sheet_capacity: asInt(max_capacity),
          physical_status,
          updated_at: new Date().toISOString()
        })
        .eq('id', existing.paper_channel_id);
      if (channelError) throw channelError;
      return res.status(200).json({ message: 'Paper product and shared physical channel updated.', data: flattenPaperSetting(data[0]) });
    } catch (err) {
      console.error('Error updating paper setting:', err);
      return res.status(500).json({ message: 'Failed to update paper setting.' });
    }
  });

  router.put('/pen/:id', authorizeRoles('superadmin'), async (req, res) => {
    const { id } = req.params;
    const { item_name, cost_per_unit, current_stock, max_capacity, physical_status, active } = req.body;
    try {
      const { data, error } = await supabase
        .from('ballpen_settings')
        .update({
          item_name,
          cost_per_unit_cents: Math.round(Number(cost_per_unit) * 100),
          current_stock: asInt(current_stock),
          max_capacity: asInt(max_capacity),
          physical_status,
          active: active !== false,
          updated_at: new Date().toISOString()
        })
        .eq('id', id)
        .select();
      if (error) throw error;
      return res.status(200).json({ message: 'Ballpen setting updated.', data: flattenPenSetting(data[0]) });
    } catch (err) {
      console.error('Error updating pen setting:', err);
      return res.status(500).json({ message: 'Failed to update pen setting.' });
    }
  });

  router.get('/analytics', async (_req, res) => {
    try {
      const [sales, inventory] = await Promise.all([getTransactionLines(supabase), getInventory(supabase)]);
      const completedSales = sales.filter((sale) => sale.status === 'COMPLETED');
      const productBreakdownMap = new Map();
      const hourlySales = Array.from({ length: 24 }, (_, hour) => ({ hour: `${String(hour).padStart(2, '0')}:00`, transactions: 0, revenue: 0 }));
      const dayNames = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
      const dayOfWeekSales = dayNames.map((day) => ({ day, transactions: 0, revenue: 0 }));
      const dailySalesMap = new Map();
      const transactionIds = new Set();
      let totalRevenue = 0;
      let paperSalesCount = 0;
      let penSalesCount = 0;
      let paperRevenue = 0;
      let penRevenue = 0;

      completedSales.forEach((sale) => {
        const revenue = sale.amount_paid;
        const date = new Date(sale.transaction_date);
        const key = `${sale.item_type}-${sale.brand_id}`;
        transactionIds.add(sale.transaction_id);
        totalRevenue += revenue;
        hourlySales[date.getHours()].transactions += 1;
        hourlySales[date.getHours()].revenue += revenue;
        dayOfWeekSales[date.getDay()].transactions += 1;
        dayOfWeekSales[date.getDay()].revenue += revenue;
        const dayKey = date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
        const daily = dailySalesMap.get(dayKey) || { date: dayKey, paper: 0, pen: 0, revenue: 0 };
        daily[sale.item_type] += sale.qty_dispensed;
        daily.revenue += revenue;
        dailySalesMap.set(dayKey, daily);

        if (!productBreakdownMap.has(key)) {
          productBreakdownMap.set(key, {
            id: sale.brand_id,
            brand_id: sale.brand_id,
            name: sale.product_name,
            item_type: sale.item_type,
            paper_size: sale.paper_size,
            sheets_per_unit: sale.sheets_per_unit_snapshot,
            cost_per_unit: asMoney(sale.unit_price_cents),
            count: 0,
            units: 0,
            revenue: 0
          });
        }
        const product = productBreakdownMap.get(key);
        product.count += sale.qty_dispensed;
        product.units += sale.units_requested;
        product.revenue += revenue;
        if (sale.item_type === 'paper') { paperSalesCount += sale.qty_dispensed; paperRevenue += revenue; }
        else { penSalesCount += sale.qty_dispensed; penRevenue += revenue; }
      });

      const lowStockItems = [
        ...inventory.paper.filter((item) => item.current_stock < 15).map((item) => `${item.paper_size} channel`),
        ...inventory.pen.filter((item) => item.current_stock < 15).map((item) => item.item_name)
      ];
      const peakHour = hourlySales.reduce((best, item) => item.transactions > best.transactions ? item : best, hourlySales[0]);
      const peakDay = dayOfWeekSales.reduce((best, item) => item.transactions > best.transactions ? item : best, dayOfWeekSales[0]);
      return res.status(200).json({
        kpis: {
          totalRevenue,
          totalSales: completedSales.reduce((sum, sale) => sum + sale.units_requested, 0),
          paperSalesCount,
          penSalesCount,
          paperRevenue,
          penRevenue,
          lowStockCount: lowStockItems.length,
          lowStockItems,
          peakHourStr: peakHour.transactions ? peakHour.hour : 'N/A',
          peakDayStr: peakDay.transactions ? peakDay.day : 'N/A',
          avgTransactionValue: transactionIds.size ? totalRevenue / transactionIds.size : 0
        },
        chartData: Array.from(dailySalesMap.values()).slice(-7),
        hourlySales,
        dayOfWeekSales,
        productBreakdown: Array.from(productBreakdownMap.values())
      });
    } catch (err) {
      console.error('Analytics fetch error:', err);
      return res.status(500).json({ message: 'Failed to aggregate analytics.' });
    }
  });

  return router;
}
