import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { useAuth } from '../App';
import { Search, Edit3, CheckCircle, AlertCircle, X, ChevronRight, Info, HelpCircle } from 'lucide-react';

export default function Inventory() {
  const { user } = useAuth();
  const [paper, setPaper] = useState([]);
  const [pen, setPen] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  
  // Modal Editing States
  const [editingItem, setEditingItem] = useState(null); // 'paper' or 'pen'
  const [formData, setFormData] = useState({});
  const [submitting, setSubmitting] = useState(false);

  // Example Formula Modal States
  const [isFormulaModalOpen, setIsFormulaModalOpen] = useState(false);
  const [calcType, setCalcType] = useState('paper');
  const [calcCoins, setCalcCoins] = useState(5);
  const [calcCost, setCalcCost] = useState(1);
  const [calcSheets, setCalcSheets] = useState(4);

  const fetchInventory = async () => {
    try {
      const res = await axios.get('/api/machine/inventory');
      setPaper(res.data.paper);
      setPen(res.data.pen);
    } catch (err) {
      console.error('Error fetching inventory:', err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchInventory();
  }, []);

  const openEditModal = (type, item) => {
    setEditingItem(type);
    setFormData({ ...item });
  };

  const closeEditModal = () => {
    setEditingItem(null);
    setFormData({});
  };

  const handleInputChange = (e) => {
    const { name, value } = e.target;
    setFormData(prev => ({
      ...prev,
      [name]: value
    }));
  };

  const handleUpdate = async (e) => {
    e.preventDefault();
    setSubmitting(true);
    try {
      if (editingItem === 'paper') {
        await axios.put(`/api/machine/paper/${formData.id}`, formData);
      } else {
        await axios.put(`/api/machine/pen/${formData.id}`, formData);
      }
      await fetchInventory();
      closeEditModal();
    } catch (err) {
      console.error('Error updating settings:', err);
      alert('Failed to save settings. Please try again.');
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

  const getPercentage = (curr, max) => {
    return Math.min(Math.round((curr / max) * 100), 100);
  };

  const filteredPaper = paper.filter(item => 
    item.brand_name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    item.paper_size.toLowerCase().includes(searchQuery.toLowerCase()) ||
    item.physical_status.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const filteredPen = pen.filter(item => 
    item.item_name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    item.physical_status.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div className="space-y-10 max-w-7xl mx-auto font-sans">
      
      {/* Top Header */}
      <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4">
        <div className="flex flex-col sm:flex-row sm:items-center gap-3">
          <div>
            <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
              Inventory Control
            </h1>
            <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
              {user?.role === 'staff' 
                ? 'Monitor stock volumes, sheets allocation, and status.' 
                : 'Adjust item settings, pricing, sheets allocation, and monitor stock volumes.'}
            </p>
          </div>
          <button
            onClick={() => setIsFormulaModalOpen(true)}
            className="inline-flex items-center gap-1.5 px-4 py-2 bg-slate-100 dark:bg-white/[0.04] hover:bg-slate-200 dark:hover:bg-white/[0.08] text-slate-650 dark:text-slate-300 rounded-xl text-xs font-bold transition-colors w-fit border border-slate-200 dark:border-white/[0.06]"
          >
            <HelpCircle className="w-4 h-4 text-primary-500" />
            <span>Example Formula</span>
          </button>
        </div>
        
        {/* Search Bar */}
        <div className="relative w-full md:w-80 shrink-0">
          <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400 dark:text-slate-500">
            <Search className="w-4.5 h-4.5" />
          </span>
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search brand, size, status..."
            className="w-full h-11 pl-10 pr-4 rounded-xl text-sm bg-white border border-slate-200 text-slate-800 placeholder-slate-400 dark:bg-[#161F30] dark:border-white/[0.08] dark:text-white dark:placeholder-slate-500 focus:border-primary-500 outline-none transition-colors shadow-sm"
          />
        </div>
      </div>

      {/* 1. PAPER INVENTORY COMPONENT */}
      <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
        <h2 className="font-display font-bold text-xl text-slate-800 dark:text-white mb-6 flex items-center gap-2">
          <span>Paper Configuration Specs</span>
          <span className="text-xs bg-primary-500/10 text-primary-500 px-2 py-0.5 rounded-full font-sans">Active Rows</span>
        </h2>

        <div className="overflow-x-auto">
          <table className="w-full text-left text-sm border-collapse">
            <thead>
              <tr className="border-b border-slate-100 dark:border-white/[0.04] text-slate-400 font-bold">
                <th className="py-3 px-4">Brand / Specification</th>
                <th className="py-3 px-4 text-center">Sheets/Unit</th>
                <th className="py-3 px-4 text-center">Cost/Unit</th>
                <th className="py-3 px-4">Stock Levels</th>
                <th className="py-3 px-4 text-center">Status</th>
                {user?.role !== 'staff' && <th className="py-3 px-4 text-right">Actions</th>}
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-100 dark:divide-white/[0.03]">
              {filteredPaper.map((item) => {
                const stockPercent = getPercentage(item.current_stock, item.max_capacity);
                return (
                  <tr key={item.id} className="hover:bg-slate-50/50 dark:hover:bg-white/[0.01]">
                    <td className="py-4 px-4 font-semibold text-slate-800 dark:text-white">
                      {item.brand_name}
                    </td>
                    <td className="py-4 px-4 text-center font-bold text-primary-500">{item.sheets_per_unit} sheets</td>
                    <td className="py-4 px-4 text-center font-bold">₱{parseInt(item.cost_per_unit)}</td>
                    <td className="py-4 px-4 min-w-[200px]">
                      <div className="flex items-center gap-3">
                        <div className="flex-1 h-2 rounded-full bg-slate-100 dark:bg-white/10 overflow-hidden">
                          <div 
                            className={`h-full rounded-full transition-all duration-500 ${
                              stockPercent < 15 ? 'bg-amber-500' : 'bg-primary-500'
                            }`}
                            style={{ width: `${stockPercent}%` }}
                          />
                        </div>
                        <span className="text-xs font-bold text-slate-500 dark:text-slate-400">
                          {item.current_stock}/{item.max_capacity}
                        </span>
                      </div>
                    </td>
                    <td className="py-4 px-4 text-center">
                      <span className={`inline-flex items-center gap-1 px-2.5 py-0.5 rounded-full text-xs font-bold ${
                        item.physical_status === 'Good' 
                          ? 'bg-emerald-500/10 text-emerald-500' 
                          : 'bg-red-500/10 text-red-500'
                      }`}>
                        {item.physical_status === 'Good' ? <CheckCircle className="w-3.5 h-3.5" /> : <AlertCircle className="w-3.5 h-3.5" />}
                        {item.physical_status}
                      </span>
                    </td>
                    {user?.role !== 'staff' && (
                      <td className="py-4 px-4 text-right">
                        <button 
                          onClick={() => openEditModal('paper', item)}
                          className="p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-white/[0.04] text-slate-500 dark:text-slate-400 hover:text-primary-500 dark:hover:text-primary-400 transition-colors"
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

      {/* 2. BALLPEN INVENTORY COMPONENT */}
      <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
        <h2 className="font-display font-bold text-xl text-slate-800 dark:text-white mb-6 flex items-center gap-2">
          <span>Ballpen Configuration Specs</span>
          <span className="text-xs bg-emerald-500/10 text-emerald-500 px-2 py-0.5 rounded-full font-sans">Active Rows</span>
        </h2>

        <div className="overflow-x-auto">
          <table className="w-full text-left text-sm border-collapse">
            <thead>
              <tr className="border-b border-slate-100 dark:border-white/[0.04] text-slate-400 font-bold">
                <th className="py-3 px-4">Item Brand Name</th>
                <th className="py-3 px-4 text-center">Cost/Unit</th>
                <th className="py-3 px-4">Stock Levels</th>
                <th className="py-3 px-4 text-center">Status</th>
                {user?.role !== 'staff' && <th className="py-3 px-4 text-right">Actions</th>}
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-100 dark:divide-white/[0.03]">
              {filteredPen.map((item) => {
                const stockPercent = getPercentage(item.current_stock, item.max_capacity);
                return (
                  <tr key={item.id} className="hover:bg-slate-50/50 dark:hover:bg-white/[0.01]">
                    <td className="py-4 px-4 font-semibold text-slate-800 dark:text-white">
                      {item.item_name}
                    </td>
                    <td className="py-4 px-4 text-center font-bold">₱{parseInt(item.cost_per_unit)}</td>
                    <td className="py-4 px-4 min-w-[200px]">
                      <div className="flex items-center gap-3">
                        <div className="flex-1 h-2 rounded-full bg-slate-100 dark:bg-white/10 overflow-hidden">
                          <div 
                            className={`h-full rounded-full transition-all duration-500 ${
                              stockPercent < 15 ? 'bg-amber-500' : 'bg-primary-500'
                            }`}
                            style={{ width: `${stockPercent}%` }}
                          />
                        </div>
                        <span className="text-xs font-bold text-slate-500 dark:text-slate-400">
                          {item.current_stock}/{item.max_capacity}
                        </span>
                      </div>
                    </td>
                    <td className="py-4 px-4 text-center">
                      <span className={`inline-flex items-center gap-1 px-2.5 py-0.5 rounded-full text-xs font-bold ${
                        item.physical_status === 'Good' 
                          ? 'bg-emerald-500/10 text-emerald-500' 
                          : 'bg-red-500/10 text-red-500'
                      }`}>
                        {item.physical_status === 'Good' ? <CheckCircle className="w-3.5 h-3.5" /> : <AlertCircle className="w-3.5 h-3.5" />}
                        {item.physical_status}
                      </span>
                    </td>
                    {user?.role !== 'staff' && (
                      <td className="py-4 px-4 text-right">
                        <button 
                          onClick={() => openEditModal('pen', item)}
                          className="p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-white/[0.04] text-slate-500 dark:text-slate-400 hover:text-primary-500 dark:hover:text-primary-400 transition-colors"
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

      {/* 3. POPUP MODAL COMPONENT */}
      {editingItem && (
        <div className="fixed inset-0 bg-slate-950/60 backdrop-blur-sm z-50 flex items-center justify-center p-4">
          <div className="w-full max-w-[480px] rounded-3xl bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.08] p-6 shadow-2xl relative animate-[fadeIn_0.2s_ease-out]">
            
            {/* Header */}
            <div className="flex items-center justify-between mb-6 pb-4 border-b border-slate-100 dark:border-white/[0.04]">
              <div>
                <h3 className="font-display font-extrabold text-lg text-slate-800 dark:text-white">
                  Adjust Settings
                </h3>
                <p className="text-xs text-slate-400 mt-0.5">
                  Update database config specs.
                </p>
              </div>
              <button 
                onClick={closeEditModal}
                className="p-1.5 rounded-lg hover:bg-slate-100 dark:hover:bg-white/[0.04] text-slate-500 dark:text-slate-400 hover:text-slate-700 dark:hover:text-white transition-colors"
              >
                <X className="w-5 h-5" />
              </button>
            </div>

            {/* Edit Form */}
            <form onSubmit={handleUpdate} className="space-y-4">
              
              {/* Brand Description Title */}
              <div className="space-y-1.5">
                <label className="block text-xs font-bold text-slate-400 uppercase tracking-wider">Item Label Name</label>
                <input
                  type="text"
                  name={editingItem === 'paper' ? 'brand_name' : 'item_name'}
                  value={editingItem === 'paper' ? (formData.brand_name || '') : (formData.item_name || '')}
                  onChange={handleInputChange}
                  className="w-full h-11 px-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 dark:bg-slate-900 dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                  required
                />
              </div>

              {/* Layout size for paper */}
              {editingItem === 'paper' && (
                <div className="grid grid-cols-2 gap-4">
                  <div className="space-y-1.5">
                    <label className="block text-xs font-bold text-slate-400 uppercase tracking-wider">Sheets Amount</label>
                    <input
                      type="number"
                      name="sheets_per_unit"
                      value={formData.sheets_per_unit || 0}
                      onChange={handleInputChange}
                      min="1"
                      max="500"
                      step="1"
                      className="w-full h-11 px-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 dark:bg-slate-900 dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                      required
                    />
                  </div>
                  <div className="space-y-1.5">
                    <label className="block text-xs font-bold text-slate-400 uppercase tracking-wider">Paper Layout Size</label>
                    <input
                      type="text"
                      name="paper_size"
                      value={formData.paper_size || ''}
                      onChange={handleInputChange}
                      className="w-full h-11 px-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 dark:bg-slate-900 dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                      required
                    />
                  </div>
                </div>
              )}

              {/* Price, Stock, Capacity */}
              <div className="grid grid-cols-3 gap-4">
                <div className="space-y-1.5">
                  <label className="block text-xs font-bold text-slate-400 uppercase tracking-wider">Price (₱) — whole number, 1–100</label>
                  <input
                    type="number"
                    step="1"
                    min="1"
                    max="100"
                    name="cost_per_unit"
                    value={formData.cost_per_unit || 0}
                    onChange={handleInputChange}
                    className="w-full h-11 px-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 dark:bg-slate-900 dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                    required
                  />
                </div>
                <div className="space-y-1.5">
                  <label className="block text-xs font-bold text-slate-400 uppercase tracking-wider">Current Stock</label>
                  <input
                    type="number"
                    name="current_stock"
                    value={formData.current_stock || 0}
                    onChange={handleInputChange}
                    min="0"
                    step="1"
                    className="w-full h-11 px-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 dark:bg-slate-900 dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                    required
                  />
                </div>
                <div className="space-y-1.5">
                  <label className="block text-xs font-bold text-slate-400 uppercase tracking-wider">Max Capacity</label>
                  <input
                    type="number"
                    name="max_capacity"
                    value={formData.max_capacity || 0}
                    onChange={handleInputChange}
                    min="1"
                    step="1"
                    className="w-full h-11 px-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 dark:bg-slate-900 dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                    required
                  />
                </div>
              </div>

              {/* Status */}
              <div className="space-y-1.5">
                <label className="block text-xs font-bold text-slate-400 uppercase tracking-wider">Physical Hardware Status</label>
                <select
                  name="physical_status"
                  value={formData.physical_status || 'Good'}
                  onChange={handleInputChange}
                  className="w-full h-11 px-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 dark:bg-slate-900 dark:border-white/[0.08] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                >
                  <option value="Good">Good (Working normally)</option>
                  <option value="Empty/Critical">Empty/Critical (Refill alert)</option>
                  <option value="Under Maintenance">Under Maintenance</option>
                </select>
              </div>

              {/* Submit Button */}
              <div className="pt-4 flex items-center justify-end gap-3 border-t border-slate-100 dark:border-white/[0.04]">
                <button
                  type="button"
                  onClick={closeEditModal}
                  className="px-4 py-2 rounded-xl text-slate-500 dark:text-slate-400 hover:bg-slate-100 dark:hover:bg-white/[0.04] text-sm font-semibold transition-colors"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={submitting}
                  className="px-5 py-2.5 rounded-xl bg-primary-500 hover:bg-primary-600 disabled:opacity-75 text-white text-sm font-semibold transition-colors shadow-md shadow-primary-500/10"
                >
                  {submitting ? 'Saving...' : 'Save Settings'}
                </button>
              </div>

            </form>
          </div>
        </div>
      )}

      {/* 4. EXAMPLE FORMULA / CALCULATOR MODAL */}
      {isFormulaModalOpen && (
        <div className="fixed inset-0 bg-slate-950/60 backdrop-blur-sm z-50 flex items-center justify-center p-4 overflow-y-auto">
          <div className="w-full max-w-2xl rounded-3xl bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.08] p-6 shadow-2xl relative my-8 animate-[fadeIn_0.2s_ease-out]">
            
            {/* Header */}
            <div className="flex items-center justify-between mb-6 pb-4 border-b border-slate-100 dark:border-white/[0.04]">
              <div>
                <h3 className="font-display font-extrabold text-xl text-slate-800 dark:text-white flex items-center gap-2">
                  <HelpCircle className="w-6 h-6 text-primary-500" />
                  <span>Example Formula & Calculator</span>
                </h3>
                <p className="text-xs text-slate-400 mt-0.5">
                  See how the hardware calculates item outputs from coins inserted.
                </p>
              </div>
              <button 
                onClick={() => setIsFormulaModalOpen(false)}
                className="p-1.5 rounded-lg hover:bg-slate-100 dark:hover:bg-white/[0.04] text-slate-500 dark:text-slate-400 hover:text-slate-700 dark:hover:text-white transition-colors"
              >
                <X className="w-5 h-5" />
              </button>
            </div>

            <div className="space-y-6 max-h-[70vh] overflow-y-auto pr-1">
              
              {/* Static Examples Section */}
              <div>
                <h4 className="text-sm font-bold text-slate-800 dark:text-white mb-3">Concrete Math Examples</h4>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                  
                  {/* Paper Examples */}
                  <div className="p-4 rounded-2xl bg-slate-50 dark:bg-slate-900 border border-slate-100 dark:border-white/[0.04] space-y-3">
                    <span className="text-xs font-bold text-primary-500 uppercase tracking-wider">📄 Paper Examples</span>
                    <div className="space-y-2 text-xs">
                      <div className="p-2.5 rounded-lg bg-white dark:bg-[#161F30] border border-slate-100 dark:border-white/[0.04]">
                        <span className="font-bold text-slate-700 dark:text-slate-350">Example 1: Budget 1/4 (₱1 per unit, 4 sheets)</span>
                        <p className="text-slate-500 mt-1">Inserted: <b>₱5</b></p>
                        <p className="text-slate-500">Purchased Units: 5 / 1 = <b>5 units</b></p>
                        <p className="text-slate-500">Dispensed: 5 × 4 = <b>20 sheets</b></p>
                        <p className="text-slate-550 dark:text-slate-400 mt-1 italic">Total Cost: ₱5 | Change: ₱0</p>
                      </div>
                      <div className="p-2.5 rounded-lg bg-white dark:bg-[#161F30] border border-slate-100 dark:border-white/[0.04]">
                        <span className="font-bold text-slate-700 dark:text-slate-350">Example 2: Standard Whole (₱2 per unit, 2 sheets)</span>
                        <p className="text-slate-500 mt-1">Inserted: <b>₱5</b></p>
                        <p className="text-slate-500">Purchased Units: Floor(5 / 2) = <b>2 units</b></p>
                        <p className="text-slate-500">Dispensed: 2 × 2 = <b>4 sheets</b></p>
                        <p className="text-slate-550 dark:text-slate-400 mt-1 italic">Total Cost: ₱4 | Change: ₱1</p>
                      </div>
                    </div>
                  </div>

                  {/* Pen Examples */}
                  <div className="p-4 rounded-2xl bg-slate-50 dark:bg-slate-900 border border-slate-100 dark:border-white/[0.04] space-y-3">
                    <span className="text-xs font-bold text-emerald-500 uppercase tracking-wider">✒️ Ballpen Examples</span>
                    <div className="space-y-2 text-xs">
                      <div className="p-2.5 rounded-lg bg-white dark:bg-[#161F30] border border-slate-100 dark:border-white/[0.04]">
                        <span className="font-bold text-slate-700 dark:text-slate-350">Example 1: Budget Pen (₱5 per unit)</span>
                        <p className="text-slate-500 mt-1">Inserted: <b>₱5</b></p>
                        <p className="text-slate-500">Purchased Units: 5 / 5 = <b>1 unit</b></p>
                        <p className="text-slate-500">Dispensed: <b>1 piece</b></p>
                        <p className="text-slate-550 dark:text-slate-400 mt-1 italic">Total Cost: ₱5 | Change: ₱0</p>
                      </div>
                      <div className="p-2.5 rounded-lg bg-white dark:bg-[#161F30] border border-slate-100 dark:border-white/[0.04]">
                        <span className="font-bold text-slate-700 dark:text-slate-350">Example 2: Standard Pen (₱10 per unit)</span>
                        <p className="text-slate-500 mt-1">Inserted: <b>₱15</b></p>
                        <p className="text-slate-500">Purchased Units: Floor(15 / 10) = <b>1 unit</b></p>
                        <p className="text-slate-500">Dispensed: <b>1 piece</b></p>
                        <p className="text-slate-550 dark:text-slate-400 mt-1 italic">Total Cost: ₱10 | Change: ₱5</p>
                      </div>
                    </div>
                  </div>

                </div>
              </div>

              {/* Interactive Calculator Section */}
              <div className="p-5 rounded-2xl border border-slate-200 dark:border-white/[0.08] bg-slate-50/50 dark:bg-slate-900/30">
                <h4 className="text-sm font-bold text-slate-800 dark:text-white mb-4 flex items-center gap-1.5">
                  <span>Interactive Calculation Simulator</span>
                  <span className="text-[10px] bg-slate-500/10 text-slate-500 px-2 py-0.5 rounded-full font-normal">Offline Calculation Test</span>
                </h4>

                <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                  
                  {/* Inputs */}
                  <div className="space-y-4">
                    <div>
                      <label className="block text-xs font-bold text-slate-400 uppercase mb-1.5">Product Type</label>
                      <div className="flex gap-2">
                        <button
                          type="button"
                          onClick={() => setCalcType('paper')}
                          className={`flex-1 h-9 rounded-lg text-xs font-bold transition-all ${
                            calcType === 'paper' 
                              ? 'bg-primary-500 text-white shadow-sm' 
                              : 'bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.06] text-slate-500 hover:bg-slate-100'
                          }`}
                        >
                          Paper
                        </button>
                        <button
                          type="button"
                          onClick={() => setCalcType('pen')}
                          className={`flex-1 h-9 rounded-lg text-xs font-bold transition-all ${
                            calcType === 'pen' 
                              ? 'bg-primary-500 text-white shadow-sm' 
                              : 'bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.06] text-slate-500 hover:bg-slate-100'
                          }`}
                        >
                          Ballpen
                        </button>
                      </div>
                    </div>

                    <div className="grid grid-cols-2 gap-3">
                      <div>
                        <label className="block text-xs font-bold text-slate-400 uppercase mb-1.5">Coins Inserted (₱)</label>
                        <input
                          type="number"
                          min="0"
                          value={calcCoins}
                          onChange={(e) => setCalcCoins(Math.max(0, parseInt(e.target.value) || 0))}
                          className="w-full h-10 px-3 rounded-lg text-xs bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.06] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        />
                      </div>
                      <div>
                        <label className="block text-xs font-bold text-slate-400 uppercase mb-1.5">Cost/Unit (₱)</label>
                        <input
                          type="number"
                          min="1"
                          value={calcCost}
                          onChange={(e) => setCalcCost(Math.max(1, parseInt(e.target.value) || 1))}
                          className="w-full h-10 px-3 rounded-lg text-xs bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.06] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        />
                      </div>
                    </div>

                    {calcType === 'paper' && (
                      <div>
                        <label className="block text-xs font-bold text-slate-400 uppercase mb-1.5">Sheets per Unit</label>
                        <input
                          type="number"
                          min="1"
                          value={calcSheets}
                          onChange={(e) => setCalcSheets(Math.max(1, parseInt(e.target.value) || 1))}
                          className="w-full h-10 px-3 rounded-lg text-xs bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.06] text-slate-800 dark:text-white outline-none focus:border-primary-500"
                        />
                      </div>
                    )}
                  </div>

                  {/* Outputs */}
                  <div className="p-4 rounded-xl bg-white dark:bg-[#161F30] border border-slate-200 dark:border-white/[0.04] space-y-4">
                    <span className="text-xs font-bold text-slate-400 uppercase tracking-wider block">Calculation Outputs</span>
                    
                    {(() => {
                      const units = Math.floor(calcCoins / calcCost);
                      const totalDispensed = calcType === 'paper' ? (units * calcSheets) : (units > 0 ? 1 : 0);
                      const totalCost = units * calcCost;
                      const change = calcCoins - totalCost;

                      return (
                        <div className="space-y-3 text-xs">
                          <div className="flex justify-between pb-1.5 border-b border-slate-100 dark:border-white/[0.04]">
                            <span className="text-slate-500">Purchased Units:</span>
                            <span className="font-bold text-slate-800 dark:text-white">
                              Floor({calcCoins} / {calcCost}) = {units} {units === 1 ? 'unit' : 'units'}
                            </span>
                          </div>
                          <div className="flex justify-between pb-1.5 border-b border-slate-100 dark:border-white/[0.04]">
                            <span className="text-slate-500">Total Dispensed:</span>
                            <span className="font-bold text-primary-500">
                              {calcType === 'paper' 
                                ? `${units} units × ${calcSheets} sheets = ${totalDispensed} sheets` 
                                : `${totalDispensed} piece (Standard Single Dispense)`}
                            </span>
                          </div>
                          <div className="flex justify-between pb-1.5 border-b border-slate-100 dark:border-white/[0.04]">
                            <span className="text-slate-500">Credits Deducted:</span>
                            <span className="font-bold text-slate-800 dark:text-white">
                              {units} units × ₱{calcCost} = ₱{totalCost}
                            </span>
                          </div>
                          <div className="flex justify-between">
                            <span className="text-slate-500">Change Returned:</span>
                            <span className="font-bold text-emerald-500">
                              ₱{calcCoins} - ₱{totalCost} = ₱{change}
                            </span>
                          </div>
                        </div>
                      );
                    })()}
                  </div>

                </div>
              </div>

            </div>

            {/* Close Button Footer */}
            <div className="pt-4 mt-6 flex items-center justify-end border-t border-slate-100 dark:border-white/[0.04]">
              <button
                type="button"
                onClick={() => setIsFormulaModalOpen(false)}
                className="px-5 py-2.5 rounded-xl bg-primary-500 hover:bg-primary-600 text-white text-sm font-semibold transition-colors shadow-md"
              >
                Close Examples
              </button>
            </div>

          </div>
        </div>
      )}

    </div>
  );
}
