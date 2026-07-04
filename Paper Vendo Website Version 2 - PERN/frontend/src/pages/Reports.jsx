import React, { useState, useEffect } from 'react';
import axios from 'axios';
import jsPDF from 'jspdf';
import autoTable from 'jspdf-autotable';
import { 
  FileText, 
  Download, 
  Filter, 
  RefreshCw, 
  Calendar, 
  ShoppingBag, 
  ChevronRight,
  ArrowRight,
  AlertCircle
} from 'lucide-react';

export default function Reports() {
  // Setup default dates (Last 30 days)
  const getPastDateStr = (daysAgo) => {
    const d = new Date();
    d.setDate(d.getDate() - daysAgo);
    return d.toISOString().split('T')[0];
  };

  const getTodayStr = () => {
    return new Date().toISOString().split('T')[0];
  };

  const [startDate, setStartDate] = useState(getPastDateStr(30));
  const [endDate, setEndDate] = useState(getTodayStr());
  const [itemType, setItemType] = useState('all');
  const [transactions, setTransactions] = useState([]);
  const [paperSettings, setPaperSettings] = useState([]);
  const [penSettings, setPenSettings] = useState([]);
  const [loading, setLoading] = useState(false);
  const [exporting, setExporting] = useState(null); // 'pdf' or 'csv'
  const [error, setError] = useState('');

  const paperMap = {};
  paperSettings.forEach(p => {
    paperMap[p.id] = { name: p.brand_name, sheets: p.sheets_per_unit };
  });

  const penMap = {};
  penSettings.forEach(p => {
    penMap[p.id] = { name: p.item_name };
  });

  const fetchInventorySettings = async () => {
    try {
      const res = await axios.get('/api/machine/inventory');
      setPaperSettings(res.data.paper);
      setPenSettings(res.data.pen);
    } catch (err) {
      console.error('Error fetching inventory config:', err);
    }
  };

  const generateReport = async () => {
    setLoading(true);
    setError('');
    try {
      const res = await axios.get('/api/machine/transactions', {
        params: {
          startDate,
          endDate,
          itemType,
          limit: 1000 // Large limit to capture all matches for the report
        }
      });
      setTransactions(res.data);
    } catch (err) {
      console.error('Error generating report:', err);
      setError('Failed to fetch transactions matching the filters.');
    } finally {
      setLoading(false);
    }
  };

  // Run automatically on mount to show default data
  useEffect(() => {
    fetchInventorySettings();
    generateReport();
  }, []);

  const exportCSV = () => {
    if (transactions.length === 0) return;
    setExporting('csv');
    try {
      const headers = ['Transaction ID', 'Timestamp', 'Item Category', 'Item Specification', 'Purchased Units', 'Dispensed Qty', 'Amount Paid (PHP)'];
      const rows = transactions.map(t => {
        const sheetsPerUnit = t.item_type === 'paper' ? (paperMap[t.brand_id]?.sheets || 4) : 1;
        const units = t.item_type === 'paper' ? Math.round(t.qty_dispensed / sheetsPerUnit) : t.qty_dispensed;
        const labelUnit = t.item_type === 'paper' ? 'sheets' : 'pcs';
        return [
          t.id,
          new Date(t.transaction_date).toLocaleString(),
          t.item_type.toUpperCase(),
          t.item_type === 'paper' 
            ? (paperMap[t.brand_id]?.name || `Paper (${t.paper_size})`) 
            : (penMap[t.brand_id]?.name || 'Ballpen'),
          `${units} ${units === 1 ? 'unit' : 'units'}`,
          `${t.qty_dispensed} ${labelUnit}`,
          parseFloat(t.amount_paid).toFixed(2)
        ];
      });
      
      const csvContent = "\uFEFF" // UTF-8 BOM for Excel support
        + [headers.join(','), ...rows.map(e => e.map(val => `"${String(val).replace(/"/g, '""')}"`).join(','))].join('\n');
      
      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      const url = URL.createObjectURL(blob);
      const link = document.createElement("a");
      link.setAttribute("href", url);
      link.setAttribute("download", `paper_vendo_sales_report_${startDate}_to_${endDate}.csv`);
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
    } catch (err) {
      console.error('CSV export failed:', err);
      alert('Failed to export CSV. Please try again.');
    } finally {
      setExporting(null);
    }
  };

  const exportPDF = () => {
    if (transactions.length === 0) return;
    setExporting('pdf');
    try {
      const doc = new jsPDF();
      
      // Document Title & Metadata
      doc.setFont("Helvetica", "bold");
      doc.setFontSize(18);
      doc.text("PAPER VENDO MACHINE SALES REPORT", 14, 18);
      
      doc.setFontSize(9);
      doc.setFont("Helvetica", "normal");
      doc.text(`Generated: ${new Date().toLocaleString()}`, 14, 25);
      doc.text(`Reporting Period: ${startDate} to ${endDate}`, 14, 30);
      doc.text(`Filter Category: ${itemType.toUpperCase()}`, 14, 35);
      
      // Calculate Summary Stats
      const totalAmount = transactions.reduce((sum, t) => sum + parseFloat(t.amount_paid), 0);
      const totalUnits = transactions.reduce((sum, t) => {
        const sheetsPerUnit = t.item_type === 'paper' ? (paperMap[t.brand_id]?.sheets || 4) : 1;
        const units = t.item_type === 'paper' ? Math.round(t.qty_dispensed / sheetsPerUnit) : t.qty_dispensed;
        return sum + units;
      }, 0);
      const totalSheetsDispensed = transactions.filter(t => t.item_type === 'paper').reduce((sum, t) => sum + t.qty_dispensed, 0);
      const totalPensDispensed = transactions.filter(t => t.item_type === 'pen').reduce((sum, t) => sum + t.qty_dispensed, 0);
      
      doc.text(`Total Sales Amount: PHP ${totalAmount.toFixed(2)}`, 125, 22);
      doc.text(`Total Units Sold: ${totalUnits} units`, 125, 27);
      doc.text(`Total Physical Dispensed:`, 125, 32);
      doc.setFont("Helvetica", "oblique");
      doc.text(`- ${totalSheetsDispensed} sheets of paper`, 130, 36);
      doc.text(`- ${totalPensDispensed} pieces of pens`, 130, 40);
      doc.setFont("Helvetica", "normal");
      doc.text(`Total Records: ${transactions.length} sales`, 14, 42);

      // Separator line
      doc.setDrawColor(220, 220, 220);
      doc.line(14, 45, 196, 45);

      // Generate Table
      const columns = ['ID', 'Date & Time', 'Category', 'Details', 'Units', 'Dispensed Qty', 'Paid (PHP)'];
      const body = transactions.map(t => {
        const sheetsPerUnit = t.item_type === 'paper' ? (paperMap[t.brand_id]?.sheets || 4) : 1;
        const units = t.item_type === 'paper' ? Math.round(t.qty_dispensed / sheetsPerUnit) : t.qty_dispensed;
        const labelUnit = t.item_type === 'paper' ? 'sheets' : 'pcs';
        return [
          t.id,
          new Date(t.transaction_date).toLocaleString(),
          t.item_type.toUpperCase(),
          t.item_type === 'paper' 
            ? (paperMap[t.brand_id]?.name || `Paper (${t.paper_size})`) 
            : (penMap[t.brand_id]?.name || 'Ballpen'),
          `${units} ${units === 1 ? 'unit' : 'units'}`,
          `${t.qty_dispensed} ${labelUnit}`,
          `PHP ${parseFloat(t.amount_paid).toFixed(2)}`
        ];
      });

      autoTable(doc, {
        startY: 50,
        head: [columns],
        body: body,
        theme: 'striped',
        headStyles: { fillColor: [14, 165, 233] }, // Primary sky/sky blue color matching site theme
        styles: { fontSize: 8 },
        columnStyles: {
          0: { cellWidth: 15 },
          1: { cellWidth: 45 },
          2: { cellWidth: 25 },
          3: { cellWidth: 35 },
          4: { cellWidth: 20 },
          5: { cellWidth: 25 },
          6: { cellWidth: 25 }
        }
      });

      doc.save(`paper_vendo_sales_report_${startDate}_to_${endDate}.pdf`);
    } catch (err) {
      console.error('PDF export failed:', err);
      alert('Failed to generate PDF. Please try again.');
    } finally {
      setExporting(null);
    }
  };

  return (
    <div className="space-y-10 max-w-7xl mx-auto font-sans">
      
      {/* Top Header */}
      <div>
        <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
          System Reports
        </h1>
        <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
          Query the system audit database and export formatted spreadsheets (CSV) or printable files (PDF).
        </p>
      </div>

      {/* Query Filters Card */}
      <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm space-y-6">
        <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white flex items-center gap-2">
          <Filter className="w-5 h-5 text-primary-500" />
          <span>Filters Configuration</span>
        </h3>

        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6">
          {/* Start Date */}
          <div className="space-y-1.5">
            <label className="block text-xs font-bold text-slate-400 dark:text-slate-500 uppercase tracking-wider">Start Date</label>
            <div className="relative">
              <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400">
                <Calendar className="w-4 h-4" />
              </span>
              <input
                type="date"
                value={startDate}
                onChange={(e) => setStartDate(e.target.value)}
                className="w-full h-11 pl-10 pr-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 text-slate-800 dark:bg-slate-900 dark:border-white/[0.08] dark:text-white outline-none focus:border-primary-500 transition-colors"
              />
            </div>
          </div>

          {/* End Date */}
          <div className="space-y-1.5">
            <label className="block text-xs font-bold text-slate-400 dark:text-slate-500 uppercase tracking-wider">End Date</label>
            <div className="relative">
              <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400">
                <Calendar className="w-4 h-4" />
              </span>
              <input
                type="date"
                value={endDate}
                onChange={(e) => setEndDate(e.target.value)}
                className="w-full h-11 pl-10 pr-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 text-slate-800 dark:bg-slate-900 dark:border-white/[0.08] dark:text-white outline-none focus:border-primary-500 transition-colors"
              />
            </div>
          </div>

          {/* Item Category */}
          <div className="space-y-1.5">
            <label className="block text-xs font-bold text-slate-400 dark:text-slate-500 uppercase tracking-wider">Item Type</label>
            <div className="relative">
              <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400">
                <ShoppingBag className="w-4 h-4" />
              </span>
              <select
                value={itemType}
                onChange={(e) => setItemType(e.target.value)}
                className="w-full h-11 pl-10 pr-3.5 rounded-xl text-sm bg-slate-50 border border-slate-200 text-slate-800 dark:bg-slate-900 dark:border-white/[0.08] dark:text-white outline-none focus:border-primary-500 transition-colors"
              >
                <option value="all">All Category Products</option>
                <option value="paper">Paper Sheets</option>
                <option value="pen">Ballpens</option>
              </select>
            </div>
          </div>

          {/* Generate Button */}
          <div className="flex items-end">
            <button
              onClick={generateReport}
              disabled={loading}
              className="w-full h-11 rounded-xl bg-primary-500 hover:bg-primary-600 disabled:opacity-75 text-white font-semibold text-sm shadow-md shadow-primary-500/10 flex items-center justify-center gap-2 transition-colors"
            >
              {loading ? (
                <RefreshCw className="w-4 h-4 animate-spin" />
              ) : (
                <FileText className="w-4 h-4" />
              )}
              <span>Query Database</span>
            </button>
          </div>
        </div>
      </div>

      {/* Query Status Details */}
      {error && (
        <div className="flex items-start gap-2.5 p-4 rounded-xl border border-red-500/20 bg-red-500/10 text-red-500 dark:text-red-400 text-xs font-semibold">
          <AlertCircle className="w-4 h-4 shrink-0 mt-0.5" />
          <span>{error}</span>
        </div>
      )}

      {/* Report Summary & Exports Section */}
      <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm space-y-6">
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
          <div>
            <h3 className="font-display font-bold text-lg text-slate-800 dark:text-white">Query Audit Outputs</h3>
            <p className="text-xs text-slate-400 mt-0.5">Found {transactions.length} transaction entries for specified filter criteria.</p>
          </div>
          
          <div className="flex items-center gap-3">
            <button
              onClick={exportCSV}
              disabled={transactions.length === 0 || exporting === 'csv'}
              className="px-4 py-2.5 rounded-xl border border-slate-200 text-slate-600 hover:bg-slate-50 dark:border-white/[0.08] dark:text-slate-300 dark:hover:bg-white/[0.02] text-xs font-bold transition-all flex items-center gap-2 disabled:opacity-50"
            >
              <Download className="w-4 h-4" />
              <span>Export CSV</span>
            </button>
            <button
              onClick={exportPDF}
              disabled={transactions.length === 0 || exporting === 'pdf'}
              className="px-4 py-2.5 rounded-xl bg-[#0EA5E9] hover:bg-[#0284C7] text-white text-xs font-bold shadow-md shadow-[#0EA5E9]/10 transition-all flex items-center gap-2 disabled:opacity-50"
            >
              <FileText className="w-4 h-4" />
              <span>Generate PDF</span>
            </button>
          </div>
        </div>

        {/* Table Content */}
        <div className="overflow-x-auto">
          <table className="w-full text-left text-sm border-collapse">
            <thead>
              <tr className="border-b border-slate-100 dark:border-white/[0.04] text-slate-400 font-bold">
                <th className="py-3 px-4">Sales ID</th>
                <th className="py-3 px-4">Dispensed Date</th>
                <th className="py-3 px-4">Item Type</th>
                <th className="py-3 px-4">Specs Description</th>
                <th className="py-3 px-4 text-center">Purchased Units</th>
                <th className="py-3 px-4 text-center">Dispensed Qty</th>
                <th className="py-3 px-4 text-right">Amount Paid</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-100 dark:divide-white/[0.03] text-slate-700 dark:text-slate-350">
              {transactions.length === 0 ? (
                <tr>
                  <td colSpan="7" className="py-10 text-center font-semibold text-slate-400">
                    No transactions found for the selected filter range.
                  </td>
                </tr>
              ) : (
                transactions.map((t) => {
                  const sheetsPerUnit = t.item_type === 'paper' ? (paperMap[t.brand_id]?.sheets || 4) : 1;
                  const units = t.item_type === 'paper' ? Math.round(t.qty_dispensed / sheetsPerUnit) : t.qty_dispensed;
                  const labelUnit = t.item_type === 'paper' ? 'sheets' : 'pcs';
                  return (
                    <tr key={t.id} className="hover:bg-slate-50/50 dark:hover:bg-white/[0.01]">
                      <td className="py-3.5 px-4 font-mono font-bold text-slate-500">#{t.id}</td>
                      <td className="py-3.5 px-4 text-xs">
                        {new Date(t.transaction_date).toLocaleString('en-US', {
                          month: 'short',
                          day: 'numeric',
                          year: 'numeric',
                          hour: 'numeric',
                          minute: '2-digit',
                          second: '2-digit',
                          hour12: true
                        })}
                      </td>
                      <td className="py-3.5 px-4">
                        <span className={`inline-block text-[10px] font-extrabold uppercase px-2 py-0.5 rounded-full ${
                          t.item_type === 'paper' 
                            ? 'bg-primary-500/10 text-primary-500' 
                            : 'bg-emerald-500/10 text-emerald-500'
                        }`}>
                          {t.item_type}
                        </span>
                      </td>
                      <td className="py-3.5 px-4 font-semibold text-slate-800 dark:text-white">
                        {t.item_type === 'paper' 
                          ? (paperMap[t.brand_id]?.name || `Paper Specs (${t.paper_size})`) 
                          : (penMap[t.brand_id]?.name || 'Ballpen Item')}
                      </td>
                      <td className="py-3.5 px-4 text-center font-bold">{units} {units === 1 ? 'unit' : 'units'}</td>
                      <td className="py-3.5 px-4 text-center font-semibold text-slate-500">{t.qty_dispensed} {labelUnit}</td>
                      <td className="py-3.5 px-4 text-right font-extrabold text-primary-500">
                        ₱{parseFloat(t.amount_paid).toFixed(2)}
                      </td>
                    </tr>
                  );
                })
              )}
            </tbody>
          </table>
        </div>

      </div>

    </div>
  );
}
