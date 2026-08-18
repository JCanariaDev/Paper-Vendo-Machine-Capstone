import express from 'express';
import { authenticateToken, authorizeRoles } from '../middleware/auth.js';

const asInt = (value, fallback = 0) => {
  const parsed = Number.parseInt(value, 10);
  return Number.isFinite(parsed) ? parsed : fallback;
};

const asMoney = (cents) => asInt(cents) / 100;

function flattenPaperInventory(row, assignedBays = []) {
  const bay = assignedBays.find((b) => b.assigned_product_id === row.id);
  return {
    ...row,
    cost_per_unit: asMoney(row.cost_per_unit_cents),
    stock_pads: asInt(row.stock_pads),
    sheets_per_unit: asInt(row.sheets_per_unit, 1),
    assigned_bay: bay ? bay.compartment_number : null,
    presence_status: bay ? bay.presence_status : 'N/A'
  };
}

function flattenPaperCompartment(row) {
  const product = row.paper_inventory || {};
  return {
    id: row.id,
    compartment_number: row.compartment_number,
    assigned_product_id: row.assigned_product_id,
    brand_name: product.brand_name || 'Unassigned',
    paper_size: product.paper_size || '',
    sheets_per_unit: asInt(product.sheets_per_unit, 1),
    cost_per_unit: asMoney(product.cost_per_unit_cents),
    presence_status: row.presence_status || 'HIGH', // 'HIGH' (Has Paper) or 'LOW' (Empty)
    motor_channel: row.motor_channel,
    sensor_channel: row.sensor_channel,
    physical_status: row.physical_status || 'Good',
    updated_at: row.updated_at
  };
}

function flattenPenInventory(row, assignedBays = []) {
  const bay = assignedBays.find((b) => b.assigned_product_id === row.id);
  return {
    ...row,
    cost_per_unit: asMoney(row.cost_per_unit_cents),
    storage_stock_pieces: asInt(row.storage_stock_pieces),
    assigned_bay: bay ? bay.compartment_number : null,
    current_bay_stock: bay ? asInt(bay.current_piece_stock) : 0
  };
}

function flattenPenCompartment(row) {
  const product = row.ballpen_inventory || {};
  return {
    id: row.id,
    compartment_number: row.compartment_number,
    assigned_product_id: row.assigned_product_id,
    item_name: product.item_name || 'Unassigned',
    cost_per_unit: asMoney(product.cost_per_unit_cents),
    current_stock: asInt(row.current_piece_stock),
    reserved_stock: asInt(row.reserved_piece_stock),
    max_capacity: asInt(row.max_piece_capacity, 100),
    dispenser_channel: row.dispenser_channel,
    physical_status: row.physical_status || 'Good',
    active: row.active !== false,
    updated_at: row.updated_at
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
  const [paperInvRes, paperCompRes, penInvRes, penCompRes] = await Promise.all([
    supabase.from('paper_inventory').select('*').order('id', { ascending: true }),
    supabase.from('paper_compartments').select('*, paper_inventory(*)').order('compartment_number', { ascending: true }),
    supabase.from('ballpen_inventory').select('*').order('id', { ascending: true }),
    supabase.from('ballpen_compartments').select('*, ballpen_inventory(*)').order('compartment_number', { ascending: true })
  ]);

  if (paperInvRes.error) throw paperInvRes.error;
  if (paperCompRes.error) throw paperCompRes.error;
  if (penInvRes.error) throw penInvRes.error;
  if (penCompRes.error) throw penCompRes.error;

  const paperBays = paperCompRes.data || [];
  const penBays = penCompRes.data || [];

  return {
    paper: (paperInvRes.data || []).map((row) => flattenPaperInventory(row, paperBays)),
    paper_compartments: paperBays.map(flattenPaperCompartment),
    pen: (penInvRes.data || []).map((row) => flattenPenInventory(row, penBays)),
    pen_compartments: penBays.map(flattenPenCompartment)
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

  // Reassign / Refill Paper Compartment Bay (1-4)
  router.put('/paper-compartments/:compartment_number', authorizeRoles('superadmin'), async (req, res) => {
    const compartmentNumber = asInt(req.params.compartment_number);
    const { assigned_product_id, pads_refilled = 0, presence_status = 'HIGH', physical_status = 'Good' } = req.body;

    try {
      const productId = assigned_product_id ? asInt(assigned_product_id) : null;
      const pads = asInt(pads_refilled, 0);

      // Call database procedure to reassign and deduct pads safely
      const { error: rpcError } = await supabase.rpc('admin_reassign_paper_bay', {
        p_compartment_number: compartmentNumber,
        p_new_product_id: productId,
        p_pads_refilled: pads,
        p_presence_status: presence_status
      });

      if (rpcError) {
        // Fallback direct update if procedure is not yet created
        if (productId) {
          if (pads > 0) {
            const { data: prod } = await supabase.from('paper_inventory').select('stock_pads').eq('id', productId).single();
            if (prod && prod.stock_pads >= pads) {
              await supabase.from('paper_inventory').update({
                stock_pads: prod.stock_pads - pads,
                location_status: 'In compartment',
                updated_at: new Date().toISOString()
              }).eq('id', productId);
            }
          } else {
            await supabase.from('paper_inventory').update({
              location_status: 'In compartment',
              updated_at: new Date().toISOString()
            }).eq('id', productId);
          }
        }
        await supabase.from('paper_compartments').update({
          assigned_product_id: productId,
          presence_status: presence_status,
          physical_status,
          updated_at: new Date().toISOString()
        }).eq('compartment_number', compartmentNumber);
      }

      return res.status(200).json({ message: `Paper Compartment ${compartmentNumber} updated successfully.` });
    } catch (err) {
      console.error('Error updating paper compartment:', err);
      return res.status(500).json({ message: 'Failed to update paper compartment.' });
    }
  });

  // Reassign / Refill Pen Compartment Bay (1-3)
  router.put('/pen-compartments/:compartment_number', authorizeRoles('superadmin'), async (req, res) => {
    const compartmentNumber = asInt(req.params.compartment_number);
    const { assigned_product_id, pieces_refilled = 0, current_stock, max_capacity, physical_status = 'Good' } = req.body;

    try {
      const productId = assigned_product_id ? asInt(assigned_product_id) : null;
      const refilled = asInt(pieces_refilled, 0);
      const directStock = (current_stock !== undefined && refilled === 0) ? asInt(current_stock) : null;

      // 1. Try atomic database stored procedure
      const { error: rpcError } = await supabase.rpc('admin_reassign_pen_bay', {
        p_compartment_number: compartmentNumber,
        p_new_product_id: productId,
        p_pieces_refilled: refilled,
        p_direct_stock: directStock
      });

      if (rpcError) {
        console.warn('RPC admin_reassign_pen_bay error, using direct table fallback:', rpcError.message);
        const { data: comp } = await supabase.from('ballpen_compartments').select('*').eq('compartment_number', compartmentNumber).single();
        const maxCap = max_capacity ? asInt(max_capacity) : (comp?.max_piece_capacity || 100);
        let newStock = comp?.current_piece_stock || 0;

        if (productId && refilled > 0) {
          const { data: prod } = await supabase.from('ballpen_inventory').select('storage_stock_pieces').eq('id', productId).single();
          if (prod && prod.storage_stock_pieces >= refilled) {
            await supabase.from('ballpen_inventory').update({
              storage_stock_pieces: prod.storage_stock_pieces - refilled,
              location_status: 'In compartment',
              updated_at: new Date().toISOString()
            }).eq('id', productId);
            newStock = Math.min(newStock + refilled, maxCap);
          }
        } else if (current_stock !== undefined) {
          newStock = Math.min(asInt(current_stock), maxCap);
        }

        if (productId) {
          await supabase.from('ballpen_inventory').update({
            location_status: 'In compartment',
            updated_at: new Date().toISOString()
          }).eq('id', productId);
        }

        const { error } = await supabase.from('ballpen_compartments').update({
          assigned_product_id: productId,
          current_piece_stock: newStock,
          max_piece_capacity: maxCap,
          physical_status,
          updated_at: new Date().toISOString()
        }).eq('compartment_number', compartmentNumber);

        if (error) throw error;
      }

      return res.status(200).json({ message: `Ballpen Compartment ${compartmentNumber} updated successfully.` });
    } catch (err) {
      console.error('Error updating ballpen compartment:', err);
      return res.status(500).json({ message: 'Failed to update ballpen compartment.' });
    }
  });

  // Master Paper Product Update
  router.put('/paper/:id', authorizeRoles('superadmin'), async (req, res) => {
    const { id } = req.params;
    const { brand_name, paper_size, cost_per_unit, sheets_per_unit, stock_pads, location_status, active } = req.body;
    try {
      const { data, error } = await supabase
        .from('paper_inventory')
        .update({
          brand_name,
          paper_size,
          cost_per_unit_cents: Math.round(Number(cost_per_unit) * 100),
          sheets_per_unit: asInt(sheets_per_unit, 1),
          stock_pads: asInt(stock_pads, 0),
          location_status: location_status || 'In stock',
          active: active !== false,
          updated_at: new Date().toISOString()
        })
        .eq('id', id)
        .select();

      if (error) throw error;
      return res.status(200).json({ message: 'Paper product updated in master inventory.', data: data[0] });
    } catch (err) {
      console.error('Error updating paper inventory:', err);
      return res.status(500).json({ message: 'Failed to update paper inventory.' });
    }
  });

  // Master Pen Product Update
  router.put('/pen/:id', authorizeRoles('superadmin'), async (req, res) => {
    const { id } = req.params;
    const { item_name, cost_per_unit, storage_stock_pieces, location_status, active } = req.body;
    try {
      const { data, error } = await supabase
        .from('ballpen_inventory')
        .update({
          item_name,
          cost_per_unit_cents: Math.round(Number(cost_per_unit) * 100),
          storage_stock_pieces: asInt(storage_stock_pieces, 0),
          location_status: location_status || 'In compartment',
          active: active !== false,
          updated_at: new Date().toISOString()
        })
        .eq('id', id)
        .select();

      if (error) throw error;
      return res.status(200).json({ message: 'Ballpen product updated in master inventory.', data: data[0] });
    } catch (err) {
      console.error('Error updating pen inventory:', err);
      return res.status(500).json({ message: 'Failed to update pen inventory.' });
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
        ...inventory.paper_compartments.filter((bay) => bay.presence_status === 'LOW').map((bay) => `Paper Bay ${bay.compartment_number} (${bay.brand_name} ${bay.paper_size}) - EMPTY`),
        ...inventory.pen_compartments.filter((bay) => bay.current_stock < 15).map((bay) => `Pen Bay ${bay.compartment_number} (${bay.item_name}) - ${bay.current_stock} pcs left`)
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
