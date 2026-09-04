import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { useAuth } from '../App';
import { 
  Search, Edit3, CheckCircle, AlertCircle, X, Layers, 
  Package, RefreshCw, HelpCircle, ArrowRightLeft, ShieldCheck, 
  Cpu, Activity, PlusCircle, Check
} from 'lucide-react';

export default function Inventory() {
  const { user } = useAuth();
  const [paper, setPaper] = useState([]);
  const [paperCompartments, setPaperCompartments] = useState([]);
  const [pen, setPen] = useState([]);
  const [penCompartments, setPenCompartments] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  
  // Modals
  const [editingMasterItem, setEditingMasterItem] = useState(null); // 'paper' or 'pen'
  const [editingBay, setEditingBay] = useState(null); // { type: 'paper' | 'pen', bay: object }
  const [formData, setFormData] = useState({});
  const [bayFormData, setBayFormData] = useState({
    assigned_product_id: '',
    pads_refilled: 1,
    pieces_refilled: 10,
    presence_status: 'HIGH',
    current_stock: 50
  });
  const [submitting, setSubmitting] = useState(false);

  // Example Formula Modal
  const [isFormulaModalOpen, setIsFormulaModalOpen] = useState(false);
  const [calcCoins, setCalcCoins] = useState(5);
  const [calcCost, setCalcCost] = useState(1);
  const [calcSheets, setCalcSheets] = useState(3);

  const fetchInventory = async () => {
    try {
      const res = await axios.get('/api/machine/inventory');
      setPaper(res.data.paper || []);
      setPaperCompartments(res.data.paper_compartments || []);
      setPen(res.data.pen || []);
      setPenCompartments(res.data.pen_compartments || []);
    } catch (err) {
      console.error('Error fetching inventory:', err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchInventory();
  }, []);

  const openMasterEditModal = (type, item) => {
    setEditingMasterItem(type);
    setFormData({ ...item });
  };

  const closeMasterEditModal = () => {
    setEditingMasterItem(null);
    setFormData({});
  };

  const openBayModal = (type, bay) => {
    setEditingBay({ type, bay });
    if (type === 'paper') {
      setBayFormData({
        assigned_product_id: bay.assigned_product_id || (paper[0]?.id || ''),
        pads_refilled: 1,
        presence_status: bay.presence_status || 'HIGH'
      });
    } else {
      setBayFormData({
        assigned_product_id: bay.assigned_product_id || (pen[0]?.id || ''),
        pieces_refilled: 10,
        current_stock: bay.current_stock || 0
      });
    }
  };

  const closeBayModal = () => {
    setEditingBay(null);
    setBayFormData({});
  };

  const handleMasterSubmit = async (e) => {
    e.preventDefault();
    setSubmitting(true);
    try {
      if (editingMasterItem === 'paper') {
        await axios.put(`/api/machine/paper/${formData.id}`, formData);
      } else {
        await axios.put(`/api/machine/pen/${formData.id}`, formData);
      }
      await fetchInventory();
      closeMasterEditModal();
    } catch (err) {
      console.error('Error updating master product:', err);
      alert('Failed to save master product setting. Please try again.');
    } finally {
      setSubmitting(false);
    }
  };

  const handleBaySubmit = async (e) => {
    e.preventDefault();
    setSubmitting(true);
    try {
      if (editingBay.type === 'paper') {
        await axios.put(`/api/machine/paper-compartments/${editingBay.bay.compartment_number}`, {
          assigned_product_id: bayFormData.assigned_product_id ? parseInt(bayFormData.assigned_product_id, 10) : null,
          pads_refilled: parseInt(bayFormData.pads_refilled || 0, 10),
          presence_status: bayFormData.presence_status
        });
      } else {
        await axios.put(`/api/machine/pen-compartments/${editingBay.bay.compartment_number}`, {
          assigned_product_id: bayFormData.assigned_product_id ? parseInt(bayFormData.assigned_product_id, 10) : null,
          pieces_refilled: parseInt(bayFormData.pieces_refilled || 0, 10),
          current_stock: parseInt(bayFormData.current_stock || 0, 10)
        });
      }
      await fetchInventory();
      closeBayModal();
    } catch (err) {
      console.error('Error updating compartment bay:', err);
      alert('Failed to update compartment bay. Please check stock availability.');
    } finally {
      setSubmitting(false);
    }
  };

  if (loading) {
    return (
      <div className="flex h-[70vh] items-center justify-center">
        <div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500"></div>
      </div>
    );
  }

  const filteredPaper = paper.filter(item => 
    item.brand_name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    item.paper_size.toLowerCase().includes(searchQuery.toLowerCase()) ||
    (item.location_status && item.location_status.toLowerCase().includes(searchQuery.toLowerCase()))
  );

  const filteredPen = pen.filter(item => 
    item.item_name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    (item.location_status && item.location_status.toLowerCase().includes(searchQuery.toLowerCase()))
  );

  return (
    <div className="space-y-10 max-w-7xl mx-auto font-sans pb-12">
      
      {/* Top Header */}
      <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4">
        <div>
          <div className="flex items-center gap-3">
            <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
              Inventory & Compartments
            </h1>
            <span className="bg-primary-500/10 text-primary-600 dark:text-primary-400 text-xs font-bold px-2.5 py-1 rounded-full border border-primary-500/20">
              Revamped 2-Bay Paper + 1-Bay Pen
            </span>
          </div>
          <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
            {user?.role === 'staff' 
              ? 'Monitor machine dispenser bays, PAD stock volumes, and presence sensors.' 
              : 'Dynamically assign items to dispenser bays, refill PADs from storage, and manage pricing.'}
          </p>
        </div>
        
        <div className="flex items-center gap-3">
          <button
            onClick={() => setIsFormulaModalOpen(true)}
            className="inline-flex items-center gap-1.5 px-4 py-2.5 bg-slate-100 dark:bg-white/[0.04] hover:bg-slate-200 dark:hover:bg-white/[0.08] text-slate-700 dark:text-slate-300 rounded-xl text-xs font-bold transition-colors border border-slate-200 dark:border-white/[0.06] shadow-sm"
          >
            <HelpCircle className="w-4 h-4 text-primary-500" />
            <span>Pricing Formula</span>
          </button>
          
          <div className="relative w-full md:w-72">
            <Search className="w-4 h-4 absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search brand, size, status..."
              className="w-full h-10 pl-9 pr-4 rounded-xl text-xs bg-white border border-slate-200 text-slate-800 placeholder-slate-400 dark:bg-[#161F30] dark:border-white/[0.08] dark:text-white dark:placeholder-slate-500 focus:border-primary-500 outline-none transition-colors shadow-sm"
            />
          </div>
        </div>
      </div>

      {/* ========================================================================= */}
      {/* SECTION 1: PHYSICAL DISPENSER COMPARTMENTS (LIVE MACHINE HARDWARE BAYS)   */}
      {/* ========================================================================= */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <div className="p-2 rounded-lg bg-primary-500/10 text-primary-500">
              <Cpu className="w-5 h-5" />
            </div>
            <div>
              <h2 className="font-display font-bold text-lg text-slate-800 dark:text-white">
                Live Physical Dispenser Bays
              </h2>
              <p className="text-xs text-slate-500 dark:text-slate-400">
                2 Stepper Paper Feeder Bays (L5290 Presence Detection) + 1 Pen Dispenser Bay
              </p>
            </div>
          </div>
          <button 
            onClick={fetchInventory} 
            className="flex items-center gap-1 text-xs text-slate-500 hover:text-primary-500 font-semibold transition-colors"
          >
            <RefreshCw className="w-3.5 h-3.5" />
            <span>Sync Live Status</span>
          </button>
        </div>

        {/* 2 Paper Bays Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {paperCompartments.map((bay) => {
            const isHigh = bay.presence_status === 'HIGH';
            return (
              <div 
                key={bay.compartment_number}
                className={`p-5 rounded-2xl border transition-all duration-300 relative overflow-hidden bg-white dark:bg-[#161F30] shadow-sm flex flex-col justify-between ${
                  isHigh 
                    ? 'border-slate-200 dark:border-white/[0.08] hover:border-primary-500/50' 
                    : 'border-red-300 dark:border-red-500/30 bg-red-50/20 dark:bg-red-950/10'
                }`}
              >
                <div>
                  <div className="flex items-center justify-between mb-3">
                    <span className="text-xs font-black uppercase tracking-wider px-2.5 py-0.5 rounded-md bg-slate-100 dark:bg-white/[0.05] text-slate-600 dark:text-slate-300">
                      Paper Bay {bay.compartment_number}
                    </span>
                    <span className="text-[11px] font-mono text-slate-400">
                      Motor D{32 + (bay.compartment_number - 1) * 2}
                    </span>
                  </div>

                  <h3 className="font-display font-bold text-base text-slate-800 dark:text-white leading-snug">
                    {bay.brand_name} {bay.paper_size ? `(${bay.paper_size})` : ''}
                  </h3>
                  
                  <div className="mt-2 text-xs text-slate-500 dark:text-slate-400 space-y-1">
                    <div className="flex justify-between">
                      <span>Rate:</span>
                      <span className="font-bold text-slate-700 dark:text-slate-200">
                        {bay.sheets_per_unit} sheets / ₱{bay.cost_per_unit}
                      </span>
                    </div>
                  </div>

                  {/* L5290 Presence Indicator */}
                  <div className="mt-4 pt-3 border-t border-slate-100 dark:border-white/[0.04]">
                    <div className="flex items-center justify-between">
                      <span className="text-[11px] font-medium text-slate-500">L5290 Sensor:</span>
                      <span className={`inline-flex items-center gap-1 px-2.5 py-0.5 rounded-full text-xs font-bold ${
                        isHigh 
                          ? 'bg-emerald-500/10 text-emerald-600 dark:text-emerald-400 border border-emerald-500/20' 
                          : 'bg-red-500/10 text-red-600 dark:text-red-400 border border-red-500/20 animate-pulse'
                      }`}>
                        {isHigh ? <CheckCircle className="w-3 h-3" /> : <AlertCircle className="w-3 h-3" />}
                        {isHigh ? 'Paper Present (HIGH)' : 'OUT OF PAPER (LOW)'}
                      </span>
                    </div>
                  </div>
                </div>

                {user?.role !== 'staff' && (
                  <div className="mt-5">
                    <button
                      onClick={() => openBayModal('paper', bay)}
                      className="w-full py-2 px-3 rounded-xl bg-slate-100 dark:bg-white/[0.04] hover:bg-primary-500 hover:text-white dark:hover:bg-primary-500 text-slate-700 dark:text-slate-300 font-bold text-xs transition-all flex items-center justify-center gap-1.5 border border-slate-200 dark:border-white/[0.06]"
                    >
                      <ArrowRightLeft className="w-3.5 h-3.5" />
                      <span>Reassign / Refill PAD</span>
                    </button>
                  </div>
                )}
              </div>
            );
          })}
        </div>

        {/* 1 Pen Bay Grid */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4 pt-2">
          {penCompartments.map((bay) => {
            const pct = Math.min(Math.round((bay.current_stock / (bay.max_capacity || 100)) * 100), 100);
            return (
              <div 
                key={bay.compartment_number}
                className="p-5 rounded-2xl border border-slate-200 dark:border-white/[0.08] bg-white dark:bg-[#161F30] shadow-sm flex flex-col justify-between"
              >
                <div>
                  <div className="flex items-center justify-between mb-3">
                    <span className="text-xs font-black uppercase tracking-wider px-2.5 py-0.5 rounded-md bg-blue-500/10 text-blue-600 dark:text-blue-400">
                      Pen Bay {bay.compartment_number}
                    </span>
                    <span className="text-[11px] font-mono text-slate-400">
                      Ch {bay.dispenser_channel}
                    </span>
                  </div>

                  <h3 className="font-display font-bold text-base text-slate-800 dark:text-white">
                    {bay.item_name}
                  </h3>
                  
                  <div className="mt-2 text-xs text-slate-500 dark:text-slate-400 flex justify-between">
                    <span>Cost / Piece:</span>
                    <span className="font-bold text-slate-700 dark:text-slate-200">₱{bay.cost_per_unit}</span>
                  </div>

                  <div className="mt-4">
                    <div className="flex justify-between text-xs font-bold mb-1.5">
                      <span className="text-slate-500">Bay Stock:</span>
                      <span className={bay.current_stock < 15 ? 'text-amber-500' : 'text-slate-700 dark:text-slate-200'}>
                        {bay.current_stock} / {bay.max_capacity} pcs
                      </span>
                    </div>
                    <div className="w-full h-2 rounded-full bg-slate-100 dark:bg-white/10 overflow-hidden">
                      <div 
                        className={`h-full rounded-full transition-all duration-500 ${
                          bay.current_stock < 15 ? 'bg-amber-500' : 'bg-primary-500'
                        }`}
                        style={{ width: `${pct}%` }}
                      />
                    </div>
                  </div>
                </div>

                {user?.role !== 'staff' && (
                  <div className="mt-5">
                    <button
                      onClick={() => openBayModal('pen', bay)}
                      className="w-full py-2 px-3 rounded-xl bg-slate-100 dark:bg-white/[0.04] hover:bg-primary-500 hover:text-white dark:hover:bg-primary-500 text-slate-700 dark:text-slate-300 font-bold text-xs transition-all flex items-center justify-center gap-1.5 border border-slate-200 dark:border-white/[0.06]"
                    >
                      <PlusCircle className="w-3.5 h-3.5" />
                      <span>Refill Pen Stock</span>
                    </button>
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>

      {/* ========================================================================= */}
      {/* SECTION 2: MASTER INVENTORY (STORAGE ROOM STOCK BY PAD / PIECES)          */}
      {/* ========================================================================= */}
      <div className="space-y-6 pt-4">
        
        {/* Master Paper Table */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
          <div className="flex items-center justify-between mb-6">
            <div>
              <h2 className="font-display font-bold text-xl text-slate-800 dark:text-white flex items-center gap-2">
                <span>Master Paper Catalog & Storage Stock</span>
                <span className="text-xs bg-primary-500/10 text-primary-500 px-2.5 py-0.5 rounded-full font-sans font-bold">
                  8 Product Types Tracked in Whole PADs
                </span>
              </h2>
              <p className="text-xs text-slate-400 mt-1">
                When you refill a machine bay, whole PADs are transferred from storage into the active compartment.
              </p>
            </div>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left text-sm border-collapse">
              <thead>
                <tr className="border-b border-slate-100 dark:border-white/[0.04] text-slate-400 text-xs font-bold uppercase tracking-wider">
                  <th className="py-3 px-4">Brand / Specification</th>
                  <th className="py-3 px-4 text-center">Sheets / Unit</th>
                  <th className="py-3 px-4 text-center">Price / Unit</th>
                  <th className="py-3 px-4 text-center">Storage Stock (PADs)</th>
                  <th className="py-3 px-4 text-center">Location State</th>
                  {user?.role !== 'staff' && <th className="py-3 px-4 text-right">Actions</th>}
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-100 dark:divide-white/[0.03]">
                {filteredPaper.map((item) => {
                  const isInBay = item.location_status === 'In compartment';
                  return (
                    <tr key={item.id} className="hover:bg-slate-50/50 dark:hover:bg-white/[0.01]">
                      <td className="py-4 px-4 font-semibold text-slate-800 dark:text-white">
                        <div className="flex items-center gap-2">
                          <span className={`w-2 h-2 rounded-full ${isInBay ? 'bg-emerald-500' : 'bg-slate-300'}`} />
                          <span>{item.brand_name} – {item.paper_size}</span>
                        </div>
                      </td>
                      <td className="py-4 px-4 text-center font-bold text-primary-500">
                        {item.sheets_per_unit} sheets
                      </td>
                      <td className="py-4 px-4 text-center font-bold">
                        ₱{item.cost_per_unit}
                      </td>
                      <td className="py-4 px-4 text-center">
                        <span className="inline-flex items-center gap-1 px-3 py-1 rounded-lg bg-slate-100 dark:bg-white/[0.04] font-bold text-slate-700 dark:text-slate-200">
                          <Package className="w-3.5 h-3.5 text-primary-500" />
                          <span>{item.stock_pads} PADs</span>
                        </span>
                      </td>
                      <td className="py-4 px-4 text-center">
                        {isInBay ? (
                          <span className="inline-flex items-center gap-1 px-2.5 py-1 rounded-full text-xs font-bold bg-emerald-500/10 text-emerald-600 dark:text-emerald-400 border border-emerald-500/20">
                            <CheckCircle className="w-3 h-3" />
                            Bay {item.assigned_bay} ({item.presence_status === 'HIGH' ? 'Active' : 'Empty'})
                          </span>
                        ) : (
                          <span className="inline-flex items-center gap-1 px-2.5 py-1 rounded-full text-xs font-bold bg-slate-100 dark:bg-white/[0.05] text-slate-500">
                            In Storage
                          </span>
                        )}
                      </td>
                      {user?.role !== 'staff' && (
                        <td className="py-4 px-4 text-right">
                          <button 
                            onClick={() => openMasterEditModal('paper', item)}
                            className="p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-white/[0.04] text-slate-400 hover:text-primary-500 transition-colors"
                            title="Edit Pricing & Storage Pads"
                          >
                            <Edit3 className="w-4 h-4" />
                          </button>
                        </td>
                      )}
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>

        {/* Master Ballpen Table */}
        <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
          <div className="flex items-center justify-between mb-6">
            <h2 className="font-display font-bold text-xl text-slate-800 dark:text-white flex items-center gap-2">
              <span>Master Ballpen Inventory</span>
              <span className="text-xs bg-blue-500/10 text-blue-500 px-2.5 py-0.5 rounded-full font-sans font-bold">
                Pieces Management
              </span>
            </h2>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left text-sm border-collapse">
              <thead>
                <tr className="border-b border-slate-100 dark:border-white/[0.04] text-slate-400 text-xs font-bold uppercase tracking-wider">
                  <th className="py-3 px-4">Pen Specification</th>
                  <th className="py-3 px-4 text-center">Cost / Piece</th>
                  <th className="py-3 px-4 text-center">Storage Stock (Pieces)</th>
                  <th className="py-3 px-4 text-center">Location State</th>
                  {user?.role !== 'staff' && <th className="py-3 px-4 text-right">Actions</th>}
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-100 dark:divide-white/[0.03]">
                {filteredPen.map((item) => (
                  <tr key={item.id} className="hover:bg-slate-50/50 dark:hover:bg-white/[0.01]">
                    <td className="py-4 px-4 font-semibold text-slate-800 dark:text-white">
                      {item.item_name}
                    </td>
                    <td className="py-4 px-4 text-center font-bold">₱{item.cost_per_unit}</td>
                    <td className="py-4 px-4 text-center">
                      <span className="inline-flex items-center gap-1 px-3 py-1 rounded-lg bg-slate-100 dark:bg-white/[0.04] font-bold text-slate-700 dark:text-slate-200">
                        <Package className="w-3.5 h-3.5 text-blue-500" />
                        <span>{item.storage_stock_pieces} pcs</span>
                      </span>
                    </td>
                    <td className="py-4 px-4 text-center">
                      {item.assigned_bay ? (
                        <span className="inline-flex items-center gap-1 px-2.5 py-1 rounded-full text-xs font-bold bg-blue-500/10 text-blue-500 border border-blue-500/20">
                          <CheckCircle className="w-3 h-3" />
                          Loaded in Bay {item.assigned_bay} ({item.current_bay_stock} pcs)
                        </span>
                      ) : (
                        <span className="inline-flex items-center gap-1 px-2.5 py-1 rounded-full text-xs font-bold bg-slate-100 dark:bg-white/[0.05] text-slate-500">
                          In Storage
                        </span>
                      )}
                    </td>
                    {user?.role !== 'staff' && (
                      <td className="py-4 px-4 text-right">
                        <button 
                          onClick={() => openMasterEditModal('pen', item)}
                          className="p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-white/[0.04] text-slate-400 hover:text-primary-500 transition-colors"
                          title="Edit Pricing & Storage Pieces"
                        >
                          <Edit3 className="w-4 h-4" />
                        </button>
                      </td>
                    )}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

      </div>

      {/* ========================================================================= */}
      {/* MODAL 1: REASSIGN / REFILL DISPENSER BAY MODAL                            */}
      {/* ========================================================================= */}
      {editingBay && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-slate-900/60 backdrop-blur-sm animate-fadeIn">
          <div className="bg-white dark:bg-[#161F30] rounded-3xl max-w-lg w-full p-6 shadow-2xl border border-slate-200 dark:border-white/[0.08]">
            <div className="flex items-center justify-between pb-4 border-b border-slate-100 dark:border-white/[0.06]">
              <div>
                <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">
                  {editingBay.type === 'paper' ? `Paper Feeder Bay ${editingBay.bay.compartment_number}` : `Pen Bay ${editingBay.bay.compartment_number}`}
                </h3>
                <p className="text-xs text-slate-400">
                  {editingBay.type === 'paper' 
                    ? 'Reassign paper specification and load whole PADs into this feeder.' 
                    : 'Reassign pen color and refill piece stock into this bay.'}
                </p>
              </div>
              <button onClick={closeBayModal} className="p-2 text-slate-400 hover:text-slate-600 dark:hover:text-white">
                <X className="w-5 h-5" />
              </button>
            </div>

            <form onSubmit={handleBaySubmit} className="mt-6 space-y-4">
              <div>
                <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">
                  Assigned Product Specification
                </label>
                <select
                  value={bayFormData.assigned_product_id}
                  onChange={(e) => setBayFormData({ ...bayFormData, assigned_product_id: e.target.value })}
                  className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                  required
                >
                  <option value="" disabled>Select product to assign...</option>
                  {editingBay.type === 'paper' 
                    ? paper.map(p => (
                        <option key={p.id} value={p.id}>
                          {p.brand_name} – {p.paper_size} ({p.stock_pads} PADs in storage)
                        </option>
                      ))
                    : pen.map(p => (
                        <option key={p.id} value={p.id}>
                          {p.item_name} ({p.storage_stock_pieces} pcs in storage)
                        </option>
                      ))
                  }
                </select>
              </div>

              {editingBay.type === 'paper' ? (
                <>
                  <div className="grid grid-cols-2 gap-4">
                    <div>
                      <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">
                        PADs to Load into Bay
                      </label>
                      <input
                        type="number"
                        min="0"
                        value={bayFormData.pads_refilled}
                        onChange={(e) => setBayFormData({ ...bayFormData, pads_refilled: e.target.value })}
                        className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        placeholder="e.g. 1"
                        required
                      />
                    </div>
                    <div>
                      <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">
                        L5290 Presence State
                      </label>
                      <select
                        value={bayFormData.presence_status}
                        onChange={(e) => setBayFormData({ ...bayFormData, presence_status: e.target.value })}
                        className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                      >
                        <option value="HIGH">HIGH (Paper Present / Ready)</option>
                        <option value="LOW">LOW (Empty / Out of Paper)</option>
                      </select>
                    </div>
                  </div>
                  <p className="text-[11px] text-slate-400 bg-slate-100 dark:bg-white/[0.02] p-3 rounded-xl">
                    💡 <strong>PAD Refill Rule:</strong> Loading PADs transfers that quantity out of Master Storage into this feeder bay.
                  </p>
                </>
              ) : (() => {
                const isPenReassign = editingBay.bay.assigned_product_id &&
                  bayFormData.assigned_product_id &&
                  parseInt(bayFormData.assigned_product_id, 10) !== editingBay.bay.assigned_product_id;
                const oldProductName = pen.find(p => p.id === editingBay.bay.assigned_product_id)?.item_name || 'previous pen';
                const oldBayStock = editingBay.bay.current_stock || 0;
                const baseStock = isPenReassign ? 0 : oldBayStock;
                const maxCap = editingBay.bay.max_capacity || 100;
                const penRefill = parseInt(bayFormData.pieces_refilled || 0, 10);
                const resultingStock = penRefill > 0
                  ? Math.min(baseStock + penRefill, maxCap)
                  : (bayFormData.current_stock !== undefined ? parseInt(bayFormData.current_stock || 0, 10) : baseStock);
                const selectedPenProduct = pen.find(p => p.id === parseInt(bayFormData.assigned_product_id, 10));
                const availableStorage = selectedPenProduct?.storage_stock_pieces || 0;

                return (
                  <>
                    {isPenReassign && (
                      <div className="p-3.5 rounded-2xl bg-amber-500/10 border border-amber-500/20 text-amber-600 dark:text-amber-400 text-xs space-y-1">
                        <div className="font-bold flex items-center gap-1.5">
                          <span>🔄</span>
                          <span>Reassigning Bay to {selectedPenProduct?.item_name || 'New Pen'}</span>
                        </div>
                        <p className="text-[11px] leading-relaxed text-slate-600 dark:text-slate-300">
                          The current <strong>{oldBayStock} pcs</strong> of <strong>{oldProductName}</strong> in this bay will be automatically returned to storage. This bay will start fresh with only the newly refilled pieces.
                        </p>
                      </div>
                    )}

                    <div className="grid grid-cols-2 gap-4">
                      <div>
                        <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">
                          Pieces to Refill (from Storage)
                        </label>
                        <input
                          type="number"
                          min="0"
                          max={availableStorage}
                          value={bayFormData.pieces_refilled}
                          onChange={(e) => setBayFormData({ ...bayFormData, pieces_refilled: e.target.value })}
                          className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                          placeholder="e.g. 10"
                        />
                        <span className="text-[10px] text-slate-400 mt-1 block">
                          Available in storage: <strong>{availableStorage} pcs</strong>
                        </span>
                      </div>
                      <div>
                        <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">
                          Resulting Bay Stock
                        </label>
                        <input
                          type="number"
                          min="0"
                          max={maxCap}
                          value={resultingStock}
                          onChange={(e) => {
                            setBayFormData({
                              ...bayFormData,
                              current_stock: e.target.value,
                              pieces_refilled: 0
                            });
                          }}
                          className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        />
                        <span className="text-[10px] text-slate-400 mt-1 block">
                          Max bay capacity: <strong>{maxCap} pcs</strong>
                        </span>
                      </div>
                    </div>

                    <p className="text-[11px] text-slate-400 bg-slate-100 dark:bg-white/[0.02] p-3 rounded-xl">
                      💡 <strong>Ballpen Stock Rule:</strong> Refilling transfers pieces out of Master Storage into this dispenser bay. If reassigning to a different pen, previous bay stock is returned to storage first.
                    </p>
                  </>
                );
              })()}

              <div className="flex justify-end gap-3 pt-4 border-t border-slate-100 dark:border-white/[0.06]">
                <button
                  type="button"
                  onClick={closeBayModal}
                  className="px-5 py-2.5 rounded-xl border border-slate-200 dark:border-white/[0.08] text-xs font-bold text-slate-600 dark:text-slate-300 hover:bg-slate-100 dark:hover:bg-white/[0.04]"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={submitting}
                  className="px-5 py-2.5 rounded-xl bg-primary-500 hover:bg-primary-600 text-white text-xs font-bold shadow-lg shadow-primary-500/20 disabled:opacity-50"
                >
                  {submitting ? 'Updating Bay...' : 'Save & Sync Bay'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* ========================================================================= */}
      {/* MODAL 2: MASTER INVENTORY EDIT MODAL (PRICING & STORAGE STOCK)             */}
      {/* ========================================================================= */}
      {editingMasterItem && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-slate-900/60 backdrop-blur-sm animate-fadeIn">
          <div className="bg-white dark:bg-[#161F30] rounded-3xl max-w-lg w-full p-6 shadow-2xl border border-slate-200 dark:border-white/[0.08]">
            <div className="flex items-center justify-between pb-4 border-b border-slate-100 dark:border-white/[0.06]">
              <div>
                <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">
                  Edit Master {editingMasterItem === 'paper' ? 'Paper Product' : 'Ballpen Product'}
                </h3>
                <p className="text-xs text-slate-400">
                  Update price per unit, sheets allocation, and master storage quantity.
                </p>
              </div>
              <button onClick={closeMasterEditModal} className="p-2 text-slate-400 hover:text-slate-600 dark:hover:text-white">
                <X className="w-5 h-5" />
              </button>
            </div>

            <form onSubmit={handleMasterSubmit} className="mt-6 space-y-4">
              {editingMasterItem === 'paper' ? (
                <>
                  <div className="grid grid-cols-2 gap-4">
                    <div>
                      <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Brand</label>
                      <input
                        type="text"
                        value={formData.brand_name || ''}
                        onChange={(e) => setFormData({ ...formData, brand_name: e.target.value })}
                        className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        required
                      />
                    </div>
                    <div>
                      <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Size</label>
                      <input
                        type="text"
                        value={formData.paper_size || ''}
                        onChange={(e) => setFormData({ ...formData, paper_size: e.target.value })}
                        className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        required
                      />
                    </div>
                  </div>
                  
                  <div className="grid grid-cols-3 gap-4">
                    <div>
                      <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Sheets / Unit</label>
                      <input
                        type="number"
                        min="1"
                        value={formData.sheets_per_unit || 1}
                        onChange={(e) => setFormData({ ...formData, sheets_per_unit: e.target.value })}
                        className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        required
                      />
                    </div>
                    <div>
                      <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Cost / Unit (₱)</label>
                      <input
                        type="number"
                        step="0.01"
                        min="0.01"
                        value={formData.cost_per_unit || 1}
                        onChange={(e) => setFormData({ ...formData, cost_per_unit: e.target.value })}
                        className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        required
                      />
                    </div>
                    <div>
                      <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Storage PADs</label>
                      <input
                        type="number"
                        min="0"
                        value={formData.stock_pads || 0}
                        onChange={(e) => setFormData({ ...formData, stock_pads: e.target.value })}
                        className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        required
                      />
                    </div>
                  </div>
                </>
              ) : (
                <div className="grid grid-cols-3 gap-4">
                  <div>
                    <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Item Name</label>
                    <input
                      type="text"
                      value={formData.item_name || ''}
                      onChange={(e) => setFormData({ ...formData, item_name: e.target.value })}
                      className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                      required
                    />
                  </div>
                  <div>
                    <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Cost / Piece (₱)</label>
                    <input
                      type="number"
                      step="0.01"
                      min="0.01"
                      value={formData.cost_per_unit || 1}
                      onChange={(e) => setFormData({ ...formData, cost_per_unit: e.target.value })}
                      className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                      required
                    />
                  </div>
                  <div>
                    <label className="block text-xs font-bold text-slate-600 dark:text-slate-300 mb-1.5">Storage Pieces</label>
                    <input
                      type="number"
                      min="0"
                      value={formData.storage_stock_pieces || 0}
                      onChange={(e) => setFormData({ ...formData, storage_stock_pieces: e.target.value })}
                      className="w-full h-11 px-3 rounded-xl bg-slate-50 dark:bg-white/[0.04] border border-slate-200 dark:border-white/[0.08] text-sm text-slate-800 dark:text-white outline-none focus:border-primary-500"
                      required
                    />
                  </div>
                </div>
              )}

              <div className="flex justify-end gap-3 pt-4 border-t border-slate-100 dark:border-white/[0.06]">
                <button
                  type="button"
                  onClick={closeMasterEditModal}
                  className="px-5 py-2.5 rounded-xl border border-slate-200 dark:border-white/[0.08] text-xs font-bold text-slate-600 dark:text-slate-300 hover:bg-slate-100 dark:hover:bg-white/[0.04]"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={submitting}
                  className="px-5 py-2.5 rounded-xl bg-primary-500 hover:bg-primary-600 text-white text-xs font-bold shadow-lg shadow-primary-500/20 disabled:opacity-50"
                >
                  {submitting ? 'Saving...' : 'Save Changes'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* ========================================================================= */}
      {/* MODAL 3: INTERACTIVE PRICING FORMULA HELPER                                */}
      {/* ========================================================================= */}
      {isFormulaModalOpen && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-slate-900/60 backdrop-blur-sm animate-fadeIn">
          <div className="bg-white dark:bg-[#161F30] rounded-3xl max-w-md w-full p-6 shadow-2xl border border-slate-200 dark:border-white/[0.08]">
            <div className="flex items-center justify-between pb-4 border-b border-slate-100 dark:border-white/[0.06]">
              <div className="flex items-center gap-2">
                <div className="p-2 rounded-lg bg-primary-500/10 text-primary-500">
                  <HelpCircle className="w-5 h-5" />
                </div>
                <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">
                  Pricing & Dispensing Math
                </h3>
              </div>
              <button onClick={() => setIsFormulaModalOpen(false)} className="p-2 text-slate-400 hover:text-slate-600 dark:hover:text-white">
                <X className="w-5 h-5" />
              </button>
            </div>

            <div className="mt-5 space-y-4 text-xs">
              <div className="p-4 rounded-xl bg-slate-50 dark:bg-white/[0.03] border border-slate-200 dark:border-white/[0.06] space-y-2">
                <p className="font-bold text-slate-700 dark:text-slate-200">
                  📐 Paper Dispense Formula:
                </p>
                <div className="font-mono text-primary-500 text-sm">
                  Total Sheets = (Inserted Coins / Cost per Unit) × Sheets per Unit
                </div>
              </div>

              <div className="grid grid-cols-3 gap-3">
                <div>
                  <label className="block text-slate-500 mb-1 font-semibold">Coins (₱)</label>
                  <input
                    type="number"
                    value={calcCoins}
                    onChange={(e) => setCalcCoins(Math.max(1, parseInt(e.target.value) || 1))}
                    className="w-full h-10 px-3 rounded-lg bg-white dark:bg-white/[0.05] border border-slate-200 dark:border-white/[0.08] text-slate-800 dark:text-white font-bold"
                  />
                </div>
                <div>
                  <label className="block text-slate-500 mb-1 font-semibold">Cost (₱)</label>
                  <input
                    type="number"
                    value={calcCost}
                    onChange={(e) => setCalcCost(Math.max(1, parseInt(e.target.value) || 1))}
                    className="w-full h-10 px-3 rounded-lg bg-white dark:bg-white/[0.05] border border-slate-200 dark:border-white/[0.08] text-slate-800 dark:text-white font-bold"
                  />
                </div>
                <div>
                  <label className="block text-slate-500 mb-1 font-semibold">Sheets/Unit</label>
                  <input
                    type="number"
                    value={calcSheets}
                    onChange={(e) => setCalcSheets(Math.max(1, parseInt(e.target.value) || 1))}
                    className="w-full h-10 px-3 rounded-lg bg-white dark:bg-white/[0.05] border border-slate-200 dark:border-white/[0.08] text-slate-800 dark:text-white font-bold"
                  />
                </div>
              </div>

              <div className="p-4 rounded-xl bg-primary-500/10 border border-primary-500/20 text-center">
                <span className="text-slate-600 dark:text-slate-300 font-medium">Customer Receives: </span>
                <span className="text-lg font-black text-primary-500">
                  {Math.floor(calcCoins / calcCost) * calcSheets} Total Sheets
                </span>
                {calcCoins % calcCost > 0 && (
                  <p className="text-[11px] text-amber-500 mt-1">
                    + ₱{calcCoins % calcCost} Returned via Coin Hopper
                  </p>
                )}
              </div>
            </div>

            <div className="mt-6">
              <button
                onClick={() => setIsFormulaModalOpen(false)}
                className="w-full py-2.5 rounded-xl bg-slate-100 dark:bg-white/[0.04] hover:bg-slate-200 text-slate-700 dark:text-slate-300 font-bold text-xs"
              >
                Close Calculator
              </button>
            </div>
          </div>
        </div>
      )}

    </div>
  );
}
