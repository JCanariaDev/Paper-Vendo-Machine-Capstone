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

      // Get settings for inventory health check and dynamic unit calculation mapping
      const { data: paper } = await supabase.from('paper_settings').select('id, brand_name, paper_size, current_stock, sheets_per_unit, cost_per_unit');
      const { data: pen } = await supabase.from('ballpen_settings').select('id, item_name, current_stock, cost_per_unit');

      // Create maps for name lookup and sheets count mapping
      const paperMap = {};
      paper?.forEach(p => {
        paperMap[p.id] = {
          name: p.brand_name,
          paperSize: p.paper_size,
          sheets: parseInt(p.sheets_per_unit) || 1,
          cost: parseFloat(p.cost_per_unit) || 0,
          breakdownKey: `paper-${p.id}`
        };
      });

      const penMap = {};
      pen?.forEach(p => {
        penMap[p.id] = {
          name: p.item_name,
          cost: parseFloat(p.cost_per_unit) || 0,
          breakdownKey: `pen-${p.id}`
        };
      });

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

      // Product breakdown helper. Paper stays split by exact size so the frontend
      // never averages mixed sheet-per-unit values together.
      const productBreakdown = [
        ...(paper || []).map(item => ({
          id: item.id,
          brand_id: item.id,
          name: item.brand_name,
          item_type: 'paper',
          paper_size: item.paper_size,
          sheets_per_unit: parseInt(item.sheets_per_unit) || 1,
          cost_per_unit: parseFloat(item.cost_per_unit) || 0,
          count: 0,
          units: 0,
          revenue: 0
        })),
        ...(pen || []).map(item => ({
          id: item.id,
          brand_id: item.id,
          name: item.item_name,
          item_type: 'pen',
          cost_per_unit: parseFloat(item.cost_per_unit) || 0,
          count: 0,
          units: 0,
          revenue: 0
        }))
      ];
      const productBreakdownMap = {};
      productBreakdown.forEach(item => {
        productBreakdownMap[`${item.item_type}-${item.id}`] = item;
      });

      sales.forEach((s) => {
        const rev = parseFloat(s.amount_paid) || 0;
        const qty = parseInt(s.qty_dispensed) || 0;
        totalSales += qty;
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
          paperSalesCount += qty;
          paperRevenue += rev;

          const paperItem = paperMap[s.brand_id];
          const sheetsPerUnit = paperItem ? paperItem.sheets : 1;
          const units = paperItem?.cost > 0 ? rev / paperItem.cost : Math.round(qty / sheetsPerUnit);
          let targetGroup = paperItem ? productBreakdownMap[paperItem.breakdownKey] : null;

          if (!targetGroup) {
            targetGroup = {
              id: s.brand_id,
              brand_id: s.brand_id,
              name: `Paper (${s.paper_size || 'Unknown Size'})`,
              item_type: 'paper',
              paper_size: s.paper_size,
              sheets_per_unit: sheetsPerUnit,
              cost_per_unit: paperItem?.cost || 0,
              count: 0,
              units: 0,
              revenue: 0
            };
            productBreakdown.push(targetGroup);
            productBreakdownMap[`paper-${s.brand_id || s.paper_size || productBreakdown.length}`] = targetGroup;
          }

          targetGroup.count += qty;
          targetGroup.units += units;
          targetGroup.revenue += rev;
        } else {
          penSalesCount += qty;
          penRevenue += rev;

          const penItem = penMap[s.brand_id];
          const units = penItem?.cost > 0 ? rev / penItem.cost : qty;
          let targetGroup = penItem ? productBreakdownMap[penItem.breakdownKey] : null;

          if (!targetGroup) {
            targetGroup = {
              id: s.brand_id,
              brand_id: s.brand_id,
              name: 'Unknown Ballpen',
              item_type: 'pen',
              cost_per_unit: penItem?.cost || 0,
              count: 0,
              units: 0,
              revenue: 0
            };
            productBreakdown.push(targetGroup);
            productBreakdownMap[`pen-${s.brand_id || productBreakdown.length}`] = targetGroup;
          }

          targetGroup.count += qty;
          targetGroup.units += units;
          targetGroup.revenue += rev;
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
