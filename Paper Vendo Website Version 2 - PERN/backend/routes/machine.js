import express from 'express';
import { authenticateToken, authorizeRoles } from '../middleware/auth.js';

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

  // 3. GET SALES TRANSACTIONS (WITH FILTERS AND LIMIT)
  router.get('/transactions', async (req, res) => {
    const { limit, startDate, endDate, itemType } = req.query;
    try {
      let query = supabase
        .from('sales_transactions')
        .select('*')
        .order('transaction_date', { ascending: false });

      if (startDate) {
        query = query.gte('transaction_date', `${startDate}T00:00:00Z`);
      }
      if (endDate) {
        query = query.lte('transaction_date', `${endDate}T23:59:59Z`);
      }
      if (itemType && itemType !== 'all') {
        query = query.eq('item_type', itemType);
      }
      if (limit) {
        query = query.limit(parseInt(limit));
      }

      const { data, error } = await query;
      if (error) throw error;
      return res.status(200).json(data);
    } catch (err) {
      console.error('Error fetching transactions:', err);
      return res.status(500).json({ message: 'Failed to retrieve sales transactions.' });
    }
  });

  // 4. UPDATE PAPER SETTING (Superadmin Only)
  router.put('/paper/:id', authorizeRoles('superadmin'), async (req, res) => {
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

  // 5. UPDATE BALLPEN SETTING (Superadmin Only)
  router.put('/pen/:id', authorizeRoles('superadmin'), async (req, res) => {
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

      // Peak hour calculation helper
      const hourlySales = Array.from({ length: 24 }, (_, i) => ({
        hour: `${String(i).padStart(2, '0')}:00`,
        transactions: 0,
        revenue: 0
      }));

      // Day of week calculation helper
      const daysOfWeek = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
      const dayOfWeekSales = daysOfWeek.map(day => ({ day, transactions: 0, revenue: 0 }));

      // Product breakdown helper
      const productBreakdown = [
        { name: 'Paper (1/4 Size)', count: 0, revenue: 0 },
        { name: 'Paper (Crosswise)', count: 0, revenue: 0 },
        { name: 'Paper (Lengthwise)', count: 0, revenue: 0 },
        { name: 'Paper (1 Whole)', count: 0, revenue: 0 },
        { name: 'Ballpen', count: 0, revenue: 0 }
      ];

      sales.forEach((s) => {
        const rev = parseFloat(s.amount_paid);
        totalSales += s.qty_dispensed;
        totalRevenue += rev;

        // Peak hour aggregation
        const tDate = new Date(s.transaction_date);
        const hr = tDate.getHours();
        hourlySales[hr].transactions += 1;
        hourlySales[hr].revenue += rev;

        // Day of week aggregation
        const dayIdx = tDate.getDay();
        dayOfWeekSales[dayIdx].transactions += 1;
        dayOfWeekSales[dayIdx].revenue += rev;

        if (s.item_type === 'paper') {
          paperSalesCount += s.qty_dispensed;
          paperRevenue += rev;

          if (s.paper_size === '1/4') {
            productBreakdown[0].count += s.qty_dispensed;
            productBreakdown[0].revenue += rev;
          } else if (s.paper_size === 'crosswise') {
            productBreakdown[1].count += s.qty_dispensed;
            productBreakdown[1].revenue += rev;
          } else if (s.paper_size === 'lengthwise') {
            productBreakdown[2].count += s.qty_dispensed;
            productBreakdown[2].revenue += rev;
          } else if (s.paper_size === '1_whole') {
            productBreakdown[3].count += s.qty_dispensed;
            productBreakdown[3].revenue += rev;
          } else {
            productBreakdown[0].count += s.qty_dispensed;
            productBreakdown[0].revenue += rev;
          }
        } else {
          penSalesCount += s.qty_dispensed;
          penRevenue += rev;
          productBreakdown[4].count += s.qty_dispensed;
          productBreakdown[4].revenue += rev;
        }
      });

      // Find peak hour and peak day
      let maxHourTx = -1;
      let peakHourIdx = 12; // default
      hourlySales.forEach((h, idx) => {
        if (h.transactions > maxHourTx) {
          maxHourTx = h.transactions;
          peakHourIdx = idx;
        }
      });
      const formatHour = (h) => {
        const suffix = h >= 12 ? 'PM' : 'AM';
        const formattedHour = h % 12 === 0 ? 12 : h % 12;
        return `${formattedHour} ${suffix}`;
      };
      const peakHourStr = maxHourTx > 0 ? `${formatHour(peakHourIdx)} - ${formatHour((peakHourIdx + 1) % 24)}` : 'N/A';

      let maxDayTx = -1;
      let peakDayStr = 'N/A';
      dayOfWeekSales.forEach((d) => {
        if (d.transactions > maxDayTx) {
          maxDayTx = d.transactions;
          peakDayStr = d.day;
        }
      });
      if (maxDayTx === 0) peakDayStr = 'N/A';

      const avgTransactionValue = sales.length > 0 ? totalRevenue / sales.length : 0;

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
          lowStockItems,
          peakHourStr,
          peakDayStr,
          avgTransactionValue
        },
        chartData,
        hourlySales,
        dayOfWeekSales,
        productBreakdown
      });
    } catch (err) {
      console.error('Analytics fetch error:', err);
      return res.status(500).json({ message: 'Failed to aggregate analytics.' });
    }
  });

  return router;
}
