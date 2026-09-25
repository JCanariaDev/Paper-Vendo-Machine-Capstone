import express from 'express';
import crypto from 'crypto';
import { authenticateToken, authorizeRoles } from '../middleware/auth.js';

const asInt = (value, fallback = 0) => {
  const parsed = Number.parseInt(value, 10);
  return Number.isFinite(parsed) ? parsed : fallback;
};

const asMoney = (cents) => asInt(cents) / 100;

async function recordRefillHistory(supabase, entry) {
  const { error } = await supabase.from('inventory_refill_history').insert(entry);
  if (error) {
    // A history write must not undo an already-completed stock transfer.
    console.error('Could not record inventory refill history:', error.message);
  }
}

function encryptNetworkPassword(password) {
  const key = Buffer.from(process.env.NETWORK_CONFIG_ENCRYPTION_KEY || '', 'base64');
  if (key.length !== 32) {
    throw new Error('NETWORK_CONFIG_ENCRYPTION_KEY must be a base64-encoded 32-byte key.');
  }

  const iv = crypto.randomBytes(12);
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  const encrypted = Buffer.concat([cipher.update(password, 'utf8'), cipher.final()]);

  return {
    password_ciphertext: encrypted.toString('base64'),
    password_iv: iv.toString('base64'),
    password_auth_tag: cipher.getAuthTag().toString('base64')
  };
}

function decryptNetworkPassword(row) {
  const key = Buffer.from(process.env.NETWORK_CONFIG_ENCRYPTION_KEY || '', 'base64');
  if (key.length !== 32) {
    throw new Error('NETWORK_CONFIG_ENCRYPTION_KEY must be a base64-encoded 32-byte key.');
  }

  const decipher = crypto.createDecipheriv(
    'aes-256-gcm',
    key,
    Buffer.from(row.password_iv, 'base64')
  );
  decipher.setAuthTag(Buffer.from(row.password_auth_tag, 'base64'));
  return Buffer.concat([
    decipher.update(Buffer.from(row.password_ciphertext, 'base64')),
    decipher.final()
  ]).toString('utf8');
}

function deviceTokenMatches(req) {
  const expected = process.env.ESP32_DEVICE_CONFIG_TOKEN || '';
  const received = typeof req.headers['x-device-token'] === 'string'
    ? req.headers['x-device-token']
    : '';
  if (!expected || !received) return false;
  const expectedBuffer = Buffer.from(expected);
  const receivedBuffer = Buffer.from(received);
  return expectedBuffer.length === receivedBuffer.length
    && crypto.timingSafeEqual(expectedBuffer, receivedBuffer);
}

function flattenPaperInventory(row, assignedBays = []) {
  const bay = assignedBays.find((b) => b.assigned_product_id === row.id);
  return {
    ...row,
    cost_per_unit: asMoney(row.cost_per_unit_cents),
    stock_pads: asInt(row.stock_pads),
    sheets_per_unit: asInt(row.sheets_per_unit, 1),
    assigned_bay: bay ? bay.compartment_number : null,
    current_bay_pads: bay ? asInt(bay.current_pad_stock, bay.presence_status === 'HIGH' ? 1 : 0) : 0,
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
    current_pad_stock: asInt(row.current_pad_stock, row.presence_status === 'HIGH' ? 1 : 0),
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
  const dueCents = asInt(transaction.change_due_cents);
  const paidCents = asInt(transaction.change_paid_cents);
  const owedCents = Math.max(0, dueCents - paidCents);

  return {
    id: line.id,
    transaction_id: transaction.id,
    tr_number: transaction.tr_number || (transaction.id ? `TR-${String(transaction.id).slice(0, 5).toUpperCase()}` : 'TR-00000'),
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
    change_due: asMoney(dueCents),
    change_paid: asMoney(paidCents),
    change_owed: asMoney(owedCents),
    refund_paid_cents: asInt(transaction.refund_paid_cents),
    failure_reason: transaction.failure_reason,
    transaction_date: transaction.created_at,
    completed_at: transaction.completed_at,
    line_status: line.line_status,
    refund_paid: asMoney(transaction.refund_paid_cents)
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
    .select('*, sales_transactions!inner(id, tr_number, status, credit_received_cents, subtotal_cents, change_due_cents, change_paid_cents, refund_paid_cents, failure_reason, created_at, completed_at)');
  if (error) throw error;
  return (data || [])
    .map(flattenTransactionLine)
    .sort((a, b) => new Date(b.transaction_date) - new Date(a.transaction_date));
}

export function createMachineRouter(supabase, networkConfigSupabase) {
  const router = express.Router();

  // Device-only route. It must be registered before the dashboard JWT guard.
  // The ESP32 receives only the active credentials and never the ciphertext.
  router.get('/network-config/device', async (req, res) => {
    if (!deviceTokenMatches(req)) {
      return res.status(401).json({ message: 'Invalid device credentials.' });
    }
    if (!networkConfigSupabase) {
      return res.status(503).json({ message: 'Network configuration is not enabled on the server.' });
    }

    try {
      const { data, error } = await networkConfigSupabase
        .from('machine_network_config')
        .select('ssid, password_ciphertext, password_iv, password_auth_tag, status, updated_at')
        .eq('id', 1)
        .maybeSingle();

      if (error) throw error;
      if (!data) return res.status(404).json({ configured: false });

      return res.status(200).json({
        configured: true,
        config: {
          ssid: data.ssid,
          password: decryptNetworkPassword(data),
          status: data.status,
          updated_at: data.updated_at
        }
      });
    } catch (err) {
      console.error('Error delivering device network configuration:', err.message);
      return res.status(500).json({ message: 'Failed to retrieve device network configuration.' });
    }
  });

  router.post('/network-config/device/ack', async (req, res) => {
    if (!deviceTokenMatches(req)) {
      return res.status(401).json({ message: 'Invalid device credentials.' });
    }
    if (!networkConfigSupabase) {
      return res.status(503).json({ message: 'Network configuration is not enabled on the server.' });
    }

    const version = typeof req.body?.version === 'string' ? req.body.version : '';
    if (!version) return res.status(400).json({ message: 'Configuration version is required.' });

    try {
      const { data, error } = await networkConfigSupabase
        .from('machine_network_config')
        .update({ status: 'APPLIED' })
        .eq('id', 1)
        .eq('updated_at', version)
        .select('ssid, status, configured_at, updated_at')
        .maybeSingle();

      if (error) throw error;
      if (!data) return res.status(409).json({ message: 'Configuration version is no longer current.' });
      return res.status(200).json({ applied: true, config: data });
    } catch (err) {
      console.error('Error acknowledging device network configuration:', err.message);
      return res.status(500).json({ message: 'Failed to acknowledge device network configuration.' });
    }
  });

  // Heartbeat endpoint for ESP32 (in case device pings backend directly)
  router.post('/heartbeat', async (_req, res) => {
    try {
      const nowIso = new Date().toISOString();
      const { data, error } = await supabase
        .from('machine_online_status')
        .update({
          status: 'Online',
          last_heartbeat: nowIso,
          updated_at: nowIso
        })
        .eq('id', 1)
        .select('*')
        .maybeSingle();

      if (error) throw error;
      return res.status(200).json({ status: 'Online', heartbeat: nowIso, data });
    } catch (err) {
      console.error('Error handling device heartbeat:', err.message);
      return res.status(500).json({ message: 'Failed to record device heartbeat.' });
    }
  });

  router.use(authenticateToken);

  router.get('/online-status', async (_req, res) => {
    try {
      const timeoutSeconds = Number.parseInt(process.env.HEARTBEAT_TIMEOUT_SECONDS || '10', 10);
      const timeoutMs = (Number.isFinite(timeoutSeconds) && timeoutSeconds > 0 ? timeoutSeconds : 10) * 1000;

      const { data, error } = await supabase
        .from('machine_online_status')
        .select('*')
        .eq('id', 1)
        .maybeSingle();

      if (error) throw error;

      let isOnline = false;
      let elapsedMs = null;
      if (data) {
        elapsedMs = Date.now() - new Date(data.last_heartbeat).getTime();
        isOnline = data.status === 'Online' && elapsedMs <= timeoutMs;
      }

      return res.status(200).json({
        status: isOnline ? 'Online' : 'Offline',
        is_online: isOnline,
        last_heartbeat: data?.last_heartbeat || null,
        elapsed_seconds: elapsedMs !== null ? Math.round(elapsedMs / 1000) : null,
        timeout_seconds: timeoutSeconds
      });
    } catch (err) {
      console.error('Error fetching online status:', err);
      return res.status(500).json({ message: 'Failed to retrieve machine online status.' });
    }
  });

  router.get('/status', async (_req, res) => {
    try {
      const timeoutSeconds = Number.parseInt(process.env.HEARTBEAT_TIMEOUT_SECONDS || '10', 10);
      const timeoutMs = (Number.isFinite(timeoutSeconds) && timeoutSeconds > 0 ? timeoutSeconds : 10) * 1000;

      const [statusResult, onlineResult] = await Promise.all([
        supabase.from('machine_status').select('*'),
        supabase.from('machine_online_status').select('*').eq('id', 1).maybeSingle()
      ]);

      if (statusResult.error) throw statusResult.error;

      let onlineData = onlineResult.data;
      let effectiveStatus = 'Offline';
      let lastHeartbeat = null;

      if (onlineData) {
        lastHeartbeat = onlineData.last_heartbeat;
        const elapsed = Date.now() - new Date(onlineData.last_heartbeat).getTime();
        const isFresh = elapsed <= timeoutMs;

        if (onlineData.status === 'Online' && isFresh) {
          effectiveStatus = 'Online';
        } else if (onlineData.status === 'Online' && !isFresh) {
          // If DB still says Online but heartbeat expired, eagerly correct DB
          effectiveStatus = 'Offline';
          supabase
            .from('machine_online_status')
            .update({ status: 'Offline', updated_at: new Date().toISOString() })
            .eq('id', 1)
            .then(() => {});
        } else {
          effectiveStatus = 'Offline';
        }
      }

      // Combine machine_status rows + is_running row for complete backward compatibility
      const combined = (statusResult.data || []).filter((item) => item.status_key !== 'is_running');
      combined.unshift({
        id: onlineData?.id || 0,
        status_key: 'is_running',
        status_value: effectiveStatus,
        last_heartbeat: lastHeartbeat,
        updated_at: onlineData?.updated_at || new Date().toISOString()
      });

      return res.status(200).json(combined);
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

  router.get('/inventory/refill-history', async (_req, res) => {
    try {
      const { data, error } = await supabase
        .from('inventory_refill_history')
        .select('*')
        .order('created_at', { ascending: false })
        .limit(500);
      if (error) throw error;
      return res.status(200).json(data || []);
    } catch (err) {
      console.error('Error fetching inventory refill history:', err);
      return res.status(500).json({ message: 'Failed to retrieve refill history.' });
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

  router.post('/transactions/:transactionId/release-change', authorizeRoles('superadmin', 'staff'), async (req, res) => {
    try {
      const { data, error } = await supabase.rpc('machine_release_change', {
        p_transaction_id: req.params.transactionId
      });
      if (error) throw error;
      return res.status(200).json({ message: 'Change marked as released.', transaction: data?.[0] || null });
    } catch (err) {
      console.error('Error releasing transaction change:', err);
      return res.status(400).json({ message: err.message || 'Could not release the transaction change.' });
    }
  });

  // This records a physical cash handover by an authorized administrator.
  // The dashboard is intentionally not permitted to run the hopper remotely.
  router.post('/transactions/:transactionId/record-failed-dispense-refund', authorizeRoles('superadmin', 'staff'), async (req, res) => {
    try {
      const { data, error } = await supabase.rpc('machine_record_failed_dispense_refund', {
        p_transaction_id: req.params.transactionId
      });
      if (error) throw error;
      return res.status(200).json({ message: 'Failed-dispense credit refund recorded.', transaction: data?.[0] || null });
    } catch (err) {
      console.error('Error recording failed-dispense refund:', err);
      return res.status(400).json({ message: err.message || 'Could not record the failed-dispense credit refund.' });
    }
  });

  router.get('/logs', async (req, res) => {
    const limit = Math.min(Math.max(asInt(req.query.limit, 200), 1), 1000);
    try {
      let query = supabase
        .from('machine_logs')
        .select('id, level, source, event_type, message, transaction_id, tr_number, metadata, created_at')
        .order('created_at', { ascending: false })
        .limit(limit);

      if (req.query.level && req.query.level !== 'all') {
        query = query.eq('level', req.query.level);
      }
      if (req.query.source && req.query.source !== 'all') {
        query = query.eq('source', req.query.source);
      }

      const { data, error } = await query;
      if (error) throw error;
      return res.status(200).json(data || []);
    } catch (err) {
      console.error('Error fetching machine logs:', err);
      return res.status(500).json({ message: 'Failed to retrieve machine logs.' });
    }
  });

  router.get('/options', async (_req, res) => {
    try {
      const { data, error } = await supabase
        .from('machine_options')
        .select('id, minimum_credits, maximum_credits, minimum_ballpens_per_transaction, maximum_ballpens_per_transaction, updated_at')
        .eq('id', 1)
        .maybeSingle();
      if (error) throw error;
      return res.status(200).json(data || { id: 1, minimum_credits: 1, maximum_credits: 30, minimum_ballpens_per_transaction: 1, maximum_ballpens_per_transaction: 5 });
    } catch (err) {
      console.error('Error fetching machine options:', err);
      return res.status(500).json({ message: 'Failed to retrieve machine options.' });
    }
  });

  router.put('/options', authorizeRoles('superadmin'), async (req, res) => {
    const minimumCredits = asInt(req.body.minimum_credits, 1);
    const maximumCredits = asInt(req.body.maximum_credits, 30);
    const minimumBallpens = asInt(req.body.minimum_ballpens_per_transaction, 1);
    const maximumBallpens = asInt(req.body.maximum_ballpens_per_transaction, 5);
    if (minimumCredits < 0 || maximumCredits < minimumCredits || maximumCredits > 10000 || minimumBallpens < 1 || maximumBallpens < minimumBallpens || maximumBallpens > 5) {
      return res.status(400).json({ message: 'Credits must be between 0 and 10,000, and ballpen limits must be between 1 and 5 with minimum no greater than maximum.' });
    }
    try {
      const { data, error } = await supabase
        .from('machine_options')
        .upsert({
          id: 1,
          minimum_credits: minimumCredits,
          maximum_credits: maximumCredits,
          minimum_ballpens_per_transaction: minimumBallpens,
          maximum_ballpens_per_transaction: maximumBallpens,
          updated_at: new Date().toISOString(),
          updated_by: req.user.id
        }, { onConflict: 'id' })
        .select('id, minimum_credits, maximum_credits, minimum_ballpens_per_transaction, maximum_ballpens_per_transaction, updated_at')
        .single();
      if (error) throw error;
      return res.status(200).json({ message: 'Machine options saved.', options: data });
    } catch (err) {
      console.error('Error saving machine options:', err);
      return res.status(500).json({ message: 'Failed to save machine options.' });
    }
  });

  // Wi-Fi credentials are staged here for a future device-configuration flow.
  // The password is encrypted before it reaches Supabase and is never returned.
  router.get('/network-config', authorizeRoles('superadmin'), async (_req, res) => {
    if (!networkConfigSupabase) {
      return res.status(503).json({ message: 'Network configuration is not enabled on the server yet.' });
    }
    try {
      const { data, error } = await networkConfigSupabase
        .from('machine_network_config')
        .select('ssid, status, configured_at, updated_at')
        .eq('id', 1)
        .maybeSingle();

      if (error) throw error;
      return res.status(200).json({ configured: Boolean(data), config: data || null });
    } catch (err) {
      console.error('Error fetching network configuration:', err);
      return res.status(500).json({ message: 'Failed to retrieve network configuration.' });
    }
  });

  router.put('/network-config', authorizeRoles('superadmin'), async (req, res) => {
    const ssid = typeof req.body.ssid === 'string' ? req.body.ssid.trim() : '';
    const password = typeof req.body.password === 'string' ? req.body.password : '';

    if (!ssid || ssid.length > 32) {
      return res.status(400).json({ message: 'SSID must contain between 1 and 32 characters.' });
    }
    if (password.length < 8 || password.length > 63) {
      return res.status(400).json({ message: 'Wi-Fi password must contain between 8 and 63 characters.' });
    }
    if (!networkConfigSupabase) {
      return res.status(503).json({ message: 'Network configuration is not enabled on the server yet.' });
    }

    try {
      const encrypted = encryptNetworkPassword(password);
      const { data, error } = await networkConfigSupabase
        .from('machine_network_config')
        .upsert([{
          id: 1,
          ssid,
          ...encrypted,
          status: 'PENDING_DEVICE_APPLY',
          configured_by: req.user.id,
          configured_at: new Date().toISOString(),
          updated_at: new Date().toISOString()
        }], { onConflict: 'id' })
        .select('ssid, status, configured_at, updated_at')
        .single();

      if (error) throw error;
      return res.status(200).json({
        message: 'Network configuration staged. The ESP32 will apply it during its next configuration check.',
        config: data
      });
    } catch (err) {
      console.error('Error staging network configuration:', err.message);
      if (err.message.includes('NETWORK_CONFIG_ENCRYPTION_KEY')) {
        return res.status(503).json({ message: 'Network configuration is not enabled on the server yet.' });
      }
      return res.status(500).json({ message: 'Failed to save network configuration.' });
    }
  });

  // Reassign / Refill Paper Compartment Bay (1-2)
  router.put('/paper-compartments/:compartment_number', authorizeRoles('superadmin'), async (req, res) => {
    const compartmentNumber = asInt(req.params.compartment_number);
    const { assigned_product_id, pads_refilled = 0, presence_status = 'HIGH', physical_status = 'Good' } = req.body;

    try {
      const productId = assigned_product_id ? asInt(assigned_product_id) : null;
      const pads = asInt(pads_refilled, 0);
      const { data: beforeComp } = await supabase
        .from('paper_compartments')
        .select('assigned_product_id, current_pad_stock')
        .eq('compartment_number', compartmentNumber)
        .single();

      // Call database procedure to reassign and deduct pads safely
      const { error: rpcError } = await supabase.rpc('admin_reassign_paper_bay', {
        p_compartment_number: compartmentNumber,
        p_new_product_id: productId,
        p_pads_refilled: pads,
        p_presence_status: presence_status
      });

      if (rpcError) {
        console.warn('RPC admin_reassign_paper_bay error, using direct table fallback:', rpcError.message);
        const { data: comp } = await supabase.from('paper_compartments').select('*').eq('compartment_number', compartmentNumber).single();
        const oldProductId = comp?.assigned_product_id;
        const currentBayPads = comp?.current_pad_stock !== undefined ? asInt(comp.current_pad_stock) : (comp?.presence_status === 'HIGH' ? 1 : 0);
        const isReassign = (oldProductId && productId && oldProductId !== productId);

        // If reassigning to a different product:
        if (isReassign) {
          // Return all N pads currently in the bay back to the old product's storage
          if (currentBayPads > 0) {
            const { data: oldProd } = await supabase.from('paper_inventory').select('stock_pads').eq('id', oldProductId).single();
            if (oldProd) {
              await supabase.from('paper_inventory').update({
                stock_pads: oldProd.stock_pads + currentBayPads,
                updated_at: new Date().toISOString()
              }).eq('id', oldProductId);
            }
          }

          // Check if old product is assigned to any other paper bay
          const { data: otherComp } = await supabase.from('paper_compartments').select('id').eq('assigned_product_id', oldProductId).neq('compartment_number', compartmentNumber);
          if (!otherComp || otherComp.length === 0) {
            const { data: oldProd } = await supabase.from('paper_inventory').select('stock_pads').eq('id', oldProductId).single();
            await supabase.from('paper_inventory').update({
              location_status: (oldProd?.stock_pads > 0) ? 'In stock' : 'Out of stock',
              updated_at: new Date().toISOString()
            }).eq('id', oldProductId);
          }
        }

        const basePads = isReassign ? 0 : currentBayPads;
        let newBayPads = basePads + pads;
        if (presence_status === 'LOW' && pads === 0) {
          newBayPads = 0;
        }

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
          current_pad_stock: newBayPads,
          presence_status: newBayPads > 0 ? 'HIGH' : presence_status,
          physical_status,
          updated_at: new Date().toISOString()
        }).eq('compartment_number', compartmentNumber);
      }

      const { data: afterComp } = await supabase
        .from('paper_compartments')
        .select('assigned_product_id, current_pad_stock')
        .eq('compartment_number', compartmentNumber)
        .single();
      const { data: product } = productId
        ? await supabase.from('paper_inventory').select('brand_name, paper_size').eq('id', productId).single()
        : { data: null };
      const wasReassigned = beforeComp?.assigned_product_id && productId && beforeComp.assigned_product_id !== productId;
      if (pads > 0 || wasReassigned) {
        await recordRefillHistory(supabase, {
          item_type: 'paper',
          compartment_number: compartmentNumber,
          product_id: productId,
          product_name: product ? `${product.brand_name} ${product.paper_size}` : 'Unassigned paper bay',
          operation: wasReassigned ? 'REASSIGNMENT' : 'REFILL',
          quantity_added: pads,
          quantity_unit: 'pads',
          previous_compartment_stock: asInt(beforeComp?.current_pad_stock),
          resulting_compartment_stock: asInt(afterComp?.current_pad_stock),
          performed_by: req.user?.username || null
        });
      }

      return res.status(200).json({ message: `Paper Compartment ${compartmentNumber} updated successfully.` });
    } catch (err) {
      console.error('Error updating paper compartment:', err);
      return res.status(500).json({ message: 'Failed to update paper compartment.' });
    }
  });

  // Reassign / Refill Pen Compartment Bay (1)
  router.put('/pen-compartments/:compartment_number', authorizeRoles('superadmin'), async (req, res) => {
    const compartmentNumber = asInt(req.params.compartment_number);
    const { assigned_product_id, pieces_refilled = 0, current_stock, max_capacity, physical_status = 'Good' } = req.body;

    try {
      const productId = assigned_product_id ? asInt(assigned_product_id) : null;
      const refilled = asInt(pieces_refilled, 0);
      const directStock = (current_stock !== undefined && refilled === 0) ? asInt(current_stock) : null;
      const { data: beforeComp } = await supabase
        .from('ballpen_compartments')
        .select('assigned_product_id, current_piece_stock')
        .eq('compartment_number', compartmentNumber)
        .single();

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
        const oldProductId = comp?.assigned_product_id;
        const oldCurrentStock = comp?.current_piece_stock || 0;
        const isReassign = (oldProductId && productId && oldProductId !== productId);

        let baseStock = oldCurrentStock;

        // If reassigning to a different product, return old stock to storage
        if (isReassign) {
          if (oldCurrentStock > 0) {
            const { data: oldProd } = await supabase.from('ballpen_inventory').select('storage_stock_pieces').eq('id', oldProductId).single();
            if (oldProd) {
              await supabase.from('ballpen_inventory').update({
                storage_stock_pieces: oldProd.storage_stock_pieces + oldCurrentStock,
                location_status: 'In stock',
                updated_at: new Date().toISOString()
              }).eq('id', oldProductId);
            }
          }
          baseStock = 0;
        }

        let newStock = baseStock;

        if (productId && refilled > 0) {
          const { data: prod } = await supabase.from('ballpen_inventory').select('storage_stock_pieces').eq('id', productId).single();
          if (prod && prod.storage_stock_pieces >= refilled) {
            await supabase.from('ballpen_inventory').update({
              storage_stock_pieces: prod.storage_stock_pieces - refilled,
              location_status: 'In compartment',
              updated_at: new Date().toISOString()
            }).eq('id', productId);
            newStock = Math.min(baseStock + refilled, maxCap);
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

      const { data: afterComp } = await supabase
        .from('ballpen_compartments')
        .select('assigned_product_id, current_piece_stock')
        .eq('compartment_number', compartmentNumber)
        .single();
      const { data: product } = productId
        ? await supabase.from('ballpen_inventory').select('item_name').eq('id', productId).single()
        : { data: null };
      const wasReassigned = beforeComp?.assigned_product_id && productId && beforeComp.assigned_product_id !== productId;
      if (refilled > 0 || directStock !== null || wasReassigned) {
        await recordRefillHistory(supabase, {
          item_type: 'pen',
          compartment_number: compartmentNumber,
          product_id: productId,
          product_name: product?.item_name || 'Unassigned ballpen bay',
          operation: wasReassigned ? 'REASSIGNMENT' : refilled > 0 ? 'REFILL' : 'ADJUSTMENT',
          quantity_added: refilled,
          quantity_unit: 'pieces',
          previous_compartment_stock: asInt(beforeComp?.current_piece_stock),
          resulting_compartment_stock: asInt(afterComp?.current_piece_stock),
          performed_by: req.user?.username || null
        });
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

  router.post('/paper', authorizeRoles('superadmin'), async (req, res) => {
    const { brand_name, paper_size, cost_per_unit, sheets_per_unit = 1, stock_pads = 0 } = req.body;
    try {
      const { data, error } = await supabase.from('paper_inventory').insert({
        brand_name: String(brand_name || '').trim(),
        paper_size: String(paper_size || '').trim(),
        cost_per_unit_cents: Math.round(Number(cost_per_unit) * 100),
        sheets_per_unit: asInt(sheets_per_unit, 1),
        stock_pads: asInt(stock_pads, 0),
        location_status: 'In stock',
        active: true
      }).select().single();
      if (error) throw error;
      return res.status(201).json({ message: 'Paper product added to the catalog.', data });
    } catch (err) {
      console.error('Error adding paper product:', err);
      return res.status(400).json({ message: err.code === '23505' ? 'That paper brand and size already exists.' : 'Failed to add paper product.' });
    }
  });

  router.patch('/paper/:id/archive', authorizeRoles('superadmin'), async (req, res) => {
    try {
      const { data: bay } = await supabase.from('paper_compartments').select('compartment_number').eq('assigned_product_id', asInt(req.params.id)).gt('current_pad_stock', 0).maybeSingle();
      if (bay) return res.status(409).json({ message: `Remove the product from Paper Bay ${bay.compartment_number} before archiving it.` });
      const { data, error } = await supabase.from('paper_inventory').update({ active: false, location_status: 'Out of stock', updated_at: new Date().toISOString() }).eq('id', asInt(req.params.id)).select().single();
      if (error) throw error;
      return res.status(200).json({ message: 'Paper product archived. Historical records were preserved.', data });
    } catch (err) {
      console.error('Error archiving paper product:', err);
      return res.status(400).json({ message: 'Failed to archive paper product.' });
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

  router.post('/pen', authorizeRoles('superadmin'), async (req, res) => {
    const { item_name, cost_per_unit, storage_stock_pieces = 0 } = req.body;
    try {
      const { data, error } = await supabase.from('ballpen_inventory').insert({
        item_name: String(item_name || '').trim(),
        cost_per_unit_cents: Math.round(Number(cost_per_unit) * 100),
        storage_stock_pieces: asInt(storage_stock_pieces, 0),
        location_status: 'In stock',
        active: true
      }).select().single();
      if (error) throw error;
      return res.status(201).json({ message: 'Ballpen product added to the catalog.', data });
    } catch (err) {
      console.error('Error adding ballpen product:', err);
      return res.status(400).json({ message: err.code === '23505' ? 'That ballpen name already exists.' : 'Failed to add ballpen product.' });
    }
  });

  router.patch('/pen/:id/archive', authorizeRoles('superadmin'), async (req, res) => {
    try {
      const { data: bay } = await supabase.from('ballpen_compartments').select('compartment_number').eq('assigned_product_id', asInt(req.params.id)).gt('current_piece_stock', 0).maybeSingle();
      if (bay) return res.status(409).json({ message: `Remove the product from Ballpen Bay ${bay.compartment_number} before archiving it.` });
      const { data, error } = await supabase.from('ballpen_inventory').update({ active: false, location_status: 'Out of stock', updated_at: new Date().toISOString() }).eq('id', asInt(req.params.id)).select().single();
      if (error) throw error;
      return res.status(200).json({ message: 'Ballpen product archived. Historical records were preserved.', data });
    } catch (err) {
      console.error('Error archiving ballpen product:', err);
      return res.status(400).json({ message: 'Failed to archive ballpen product.' });
    }
  });

  router.get('/analytics', async (_req, res) => {
    try {
      const [sales, inventory] = await Promise.all([getTransactionLines(supabase), getInventory(supabase)]);
      const completedSales = sales.filter((sale) =>
        ['COMPLETED', 'COMPLETED_CHANGE_OWED', 'PARTIAL_SUCCESS'].includes(sale.status) ||
        (sale.line_status === 'DISPENSED' && sale.qty_dispensed > 0)
      );
      const analyticsTimeZone = process.env.MACHINE_TIMEZONE || 'Asia/Manila';
      const machineDateFormatter = new Intl.DateTimeFormat('en-US', {
        timeZone: analyticsTimeZone,
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        hourCycle: 'h23',
        weekday: 'short'
      });
      const weekdayIndexes = { Sun: 0, Mon: 1, Tue: 2, Wed: 3, Thu: 4, Fri: 5, Sat: 6 };
      const analyticsSales = completedSales.map((sale) => {
        const parts = Object.fromEntries(machineDateFormatter.formatToParts(new Date(sale.transaction_date)).map((part) => [part.type, part.value]));
        return {
          ...sale,
          analytics_time_zone: analyticsTimeZone,
          transaction_local_date: `${parts.year}-${parts.month}-${parts.day}`,
          transaction_local_hour: Number(parts.hour),
          transaction_local_weekday: weekdayIndexes[parts.weekday]
        };
      });

      const productBreakdownMap = new Map();
      const hourlySales = Array.from({ length: 24 }, (_, hour) => ({ hour: `${String(hour).padStart(2, '0')}:00`, transactions: 0, revenue: 0 }));
      const dayNames = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
      const dayOfWeekSales = dayNames.map((day) => ({ day, transactions: 0, revenue: 0 }));

      // Anchor rolling 7-day window to latest completed transaction date or today
      const latestDate = completedSales.length > 0 
        ? new Date(Math.max(...completedSales.map((s) => new Date(s.transaction_date).getTime()), Date.now()))
        : new Date();

      const dayFormatter = new Intl.DateTimeFormat('en-US', { month: 'short', day: 'numeric' });
      const past7Days = [];
      for (let i = 6; i >= 0; i--) {
        const d = new Date(latestDate);
        d.setDate(d.getDate() - i);
        past7Days.push({
          date: dayFormatter.format(d),
          paper: 0,
          pen: 0,
          revenue: 0
        });
      }

      const transactionIds = new Set();
      let totalRevenue = 0;
      let paperSalesCount = 0;
      let penSalesCount = 0;
      let paperRevenue = 0;
      let penRevenue = 0;

      analyticsSales.forEach((sale) => {
        const revenue = Number(sale.amount_paid || 0);
        const date = new Date(sale.transaction_date);
        const key = `${sale.item_type}-${sale.brand_id}`;
        transactionIds.add(sale.transaction_id);
        totalRevenue = Number((totalRevenue + revenue).toFixed(2));

        const hour = sale.transaction_local_hour;
        if (hour >= 0 && hour < 24) {
          hourlySales[hour].transactions += 1;
          hourlySales[hour].revenue = Number((hourlySales[hour].revenue + revenue).toFixed(2));
        }

        const day = sale.transaction_local_weekday;
        if (day >= 0 && day < 7) {
          dayOfWeekSales[day].transactions += 1;
          dayOfWeekSales[day].revenue = Number((dayOfWeekSales[day].revenue + revenue).toFixed(2));
        }

        const saleDateStr = new Intl.DateTimeFormat('en-US', { timeZone: analyticsTimeZone, month: 'short', day: 'numeric' }).format(date);
        const daySlot = past7Days.find((slot) => slot.date === saleDateStr);
        if (daySlot) {
          daySlot[sale.item_type] += sale.qty_dispensed;
          daySlot.revenue = Number((daySlot.revenue + revenue).toFixed(2));
        }

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
        product.revenue = Number((product.revenue + revenue).toFixed(2));
        if (sale.item_type === 'paper') { 
          paperSalesCount += sale.qty_dispensed; 
          paperRevenue = Number((paperRevenue + revenue).toFixed(2)); 
        } else { 
          penSalesCount += sale.qty_dispensed; 
          penRevenue = Number((penRevenue + revenue).toFixed(2)); 
        }
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
          avgTransactionValue: transactionIds.size ? Number((totalRevenue / transactionIds.size).toFixed(2)) : 0
        },
        chartData: past7Days,
        hourlySales,
        dayOfWeekSales,
        analyticsSales,
        analyticsTimeZone,
        productBreakdown: Array.from(productBreakdownMap.values())
      });
    } catch (err) {
      console.error('Analytics fetch error:', err);
      return res.status(500).json({ message: 'Failed to aggregate analytics.' });
    }
  });

  return router;
}
