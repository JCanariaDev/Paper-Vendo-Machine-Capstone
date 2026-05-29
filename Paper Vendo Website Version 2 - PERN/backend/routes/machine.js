import express from 'express';
import { authenticateToken } from '../middleware/auth.js';

export function createMachineRouter(supabase) {
  const router = express.Router();

  // Apply Auth Middleware to protect all admin endpoints
  router.use(authenticateToken);

  // 1. GET MACHINE STATUS
  router.get('/status', async (req, res) => {
    try {
      const { data, error } = await supabase
        .from('machine_status')
        .select('*');

      if (error) throw error;
      return res.status(200).json(data);
    } catch (err) {
      console.error('Error fetching status:', err);
      return res.status(500).json({ message: 'Failed to retrieve machine status.' });
    }
  });

  // 2. GET INVENTORY SETTINGS (PAPER & BALLPENS)
  router.get('/inventory', async (req, res) => {
    try {
      const { data: paper, error: paperErr } = await supabase
        .from('paper_settings')
        .select('*')
        .order('id', { ascending: true });

      const { data: pen, error: penErr } = await supabase
        .from('ballpen_settings')
        .select('*')
        .order('id', { ascending: true });

      if (paperErr) throw paperErr;
      if (penErr) throw penErr;

      return res.status(200).json({ paper, pen });
    } catch (err) {
      console.error('Error fetching inventory:', err);
      return res.status(500).json({ message: 'Failed to retrieve inventory.' });
    }
  });

  // 3. GET SALES TRANSACTIONS (WITH LIMIT)
  router.get('/transactions', async (req, res) => {
    const limit = parseInt(req.query.limit) || 100;
    try {
      const { data, error } = await supabase
        .from('sales_transactions')
        .select('*')
        .order('transaction_date', { ascending: false })
        .limit(limit);

      if (error) throw error;
      return res.status(200).json(data);
    } catch (err) {
      console.error('Error fetching transactions:', err);
      return res.status(500).json({ message: 'Failed to retrieve sales transactions.' });
    }
  });

  // 4. UPDATE PAPER SETTING
  router.put('/paper/:id', async (req, res) => {
    const { id } = req.params;
    const { brand_name, paper_size, cost_per_unit, sheets_per_unit, current_stock, max_capacity, physical_status } = req.body;

    try {
      const { data, error } = await supabase
        .from('paper_settings')
        .update({
          brand_name,
          paper_size,
          cost_per_unit: parseFloat(cost_per_unit),
          sheets_per_unit: parseInt(sheets_per_unit),
          current_stock: parseInt(current_stock),
          max_capacity: parseInt(max_capacity),
          physical_status
        })
        .eq('id', id)
        .select();

      if (error) throw error;
      return res.status(200).json({ message: 'Paper setting updated.', data: data[0] });
    } catch (err) {
      console.error('Error updating paper setting:', err);
      return res.status(500).json({ message: 'Failed to update paper setting.' });
    }
  });

  // 5. UPDATE BALLPEN SETTING
  router.put('/pen/:id', async (req, res) => {
    const { id } = req.params;
    const { item_name, cost_per_unit, current_stock, max_capacity, physical_status } = req.body;

    try {
      const { data, error } = await supabase
        .from('ballpen_settings')
        .update({
          item_name,
          cost_per_unit: parseFloat(cost_per_unit),
          current_stock: parseInt(current_stock),
          max_capacity: parseInt(max_capacity),
          physical_status
        })
        .eq('id', id)
        .select();

      if (error) throw error;
      return res.status(200).json({ message: 'Ballpen setting updated.', data: data[0] });
    } catch (err) {
      console.error('Error updating pen setting:', err);
      return res.status(500).json({ message: 'Failed to update pen setting.' });
    }
  });

  // 6. GET INTEGRATED ANALYTICS (KPIs & Graph Data)
  router.get('/analytics', async (req, res) => {
    try {
      // Get all transactions
      const { data: sales, error } = await supabase
        .from('sales_transactions')
        .select('*');

      if (error) throw error;

      // Get settings for inventory health check
      const { data: paper } = await supabase.from('paper_settings').select('brand_name, paper_size, current_stock');
      const { data: pen } = await supabase.from('ballpen_settings').select('item_name, current_stock');

      // Calculate total earnings & counts
      let totalSales = 0;
      let totalRevenue = 0;
      let paperSalesCount = 0;
      let penSalesCount = 0;
      let paperRevenue = 0;
      let penRevenue = 0;

      sales.forEach((s) => {
        totalSales += s.qty_dispensed;
        totalRevenue += parseFloat(s.amount_paid);
        if (s.item_type === 'paper') {
          paperSalesCount += s.qty_dispensed;
          paperRevenue += parseFloat(s.amount_paid);
        } else {
          penSalesCount += s.qty_dispensed;
          penRevenue += parseFloat(s.amount_paid);
        }
      });

      // Calculate low stock items (threshold = 15 units)
      let lowStockCount = 0;
      const lowStockItems = [];

      paper?.forEach(item => {
        if (item.current_stock < 15) {
          lowStockCount++;
          lowStockItems.push(`${item.brand_name} (${item.paper_size})`);
        }
      });

      pen?.forEach(item => {
        if (item.current_stock < 15) {
          lowStockCount++;
          lowStockItems.push(item.item_name);
        }
      });

      // Aggregate sales by date for charts
      const dailySalesMap = {};
      sales.forEach((s) => {
        const dateStr = new Date(s.transaction_date).toLocaleDateString('en-US', {
          month: 'short',
          day: 'numeric',
        });
        if (!dailySalesMap[dateStr]) {
          dailySalesMap[dateStr] = { date: dateStr, paper: 0, pen: 0, revenue: 0 };
        }
        if (s.item_type === 'paper') {
          dailySalesMap[dateStr].paper += s.qty_dispensed;
        } else {
          dailySalesMap[dateStr].pen += s.qty_dispensed;
        }
        dailySalesMap[dateStr].revenue += parseFloat(s.amount_paid);
      });

      // Convert daily map into a sorted array (taking last 7 records for view stability)
      const chartData = Object.values(dailySalesMap)
        .slice(-7); // Get the 7 most recent days of sales activity

      return res.status(200).json({
        kpis: {
          totalRevenue,
          totalSales,
          paperSalesCount,
          penSalesCount,
          paperRevenue,
          penRevenue,
          lowStockCount,
          lowStockItems
        },
        chartData
      });
    } catch (err) {
      console.error('Analytics fetch error:', err);
      return res.status(500).json({ message: 'Failed to aggregate analytics.' });
    }
  });

  return router;
}
