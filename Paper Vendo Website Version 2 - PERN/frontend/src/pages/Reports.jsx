import React, { useCallback, useEffect, useMemo, useState } from 'react';
import axios from 'axios';
import jsPDF from 'jspdf';
import autoTable from 'jspdf-autotable';
import {
  AlertCircle,
  Calendar,
  CheckCircle2,
  ChevronLeft,
  ChevronRight,
  Clock3,
  Download,
  FileText,
  Filter,
  HandCoins,
  RefreshCw,
  Search,
  ShoppingBag
} from 'lucide-react';

const PAGE_SIZE = 15;
const currency = (value) => `PHP ${Number(value || 0).toLocaleString(undefined, {
  minimumFractionDigits: 2,
  maximumFractionDigits: 2
})}`;

const localDateKey = (date) => `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;

function dateInputValue(daysAgo = 0) {
  const date = new Date();
  date.setDate(date.getDate() - daysAgo);
  return localDateKey(date);
}

function localDayStartIso(dateValue) {
  const [year, month, day] = dateValue.split('-').map(Number);
  return new Date(year, month - 1, day).toISOString();
}

function localDayAfterIso(dateValue) {
  const [year, month, day] = dateValue.split('-').map(Number);
  return new Date(year, month - 1, day + 1).toISOString();
}

function formatDateTime(value, timeZone) {
  const date = new Date(value);
  if (!Number.isFinite(date.getTime())) return '—';
  return new Intl.DateTimeFormat(undefined, {
    year: 'numeric',
    month: 'short',
    day: 'numeric',
    hour: 'numeric',
    minute: '2-digit',
    second: '2-digit',
    timeZone,
    timeZoneName: 'short'
  }).format(date);
}

function getStatusInfo(transaction) {
  const status = transaction.status || 'PENDING';
  if (status.startsWith('FAILED') || status === 'CANCELLED') {
    return { key: 'failed', label: status.replaceAll('_', ' '), tone: 'red', icon: AlertCircle };
  }
  if (status === 'PARTIAL_SUCCESS') {
    return { key: 'partial', label: 'PARTIAL SUCCESS', tone: 'amber', icon: AlertCircle };
  }
  if (status === 'REFUNDED') {
    return { key: 'refunded', label: 'REFUNDED', tone: 'blue', icon: HandCoins };
  }
  if (status === 'COMPLETED_CHANGE_OWED' || Number(transaction.change_owed) > 0) {
    return { key: 'change', label: 'CHANGE OWED', tone: 'amber', icon: Clock3 };
  }
  if (status === 'COMPLETED') {
    return { key: 'completed', label: 'COMPLETED', tone: 'green', icon: CheckCircle2 };
  }
  return { key: 'other', label: status.replaceAll('_', ' '), tone: 'slate', icon: Clock3 };
}

const statusStyles = {
  red: 'border-red-200 bg-red-50 text-red-700 dark:border-red-900/50 dark:bg-red-950/30 dark:text-red-300',
  amber: 'border-amber-200 bg-amber-50 text-amber-700 dark:border-amber-900/50 dark:bg-amber-950/30 dark:text-amber-300',
  blue: 'border-sky-200 bg-sky-50 text-sky-700 dark:border-sky-900/50 dark:bg-sky-950/30 dark:text-sky-300',
  green: 'border-emerald-200 bg-emerald-50 text-emerald-700 dark:border-emerald-900/50 dark:bg-emerald-950/30 dark:text-emerald-300',
  slate: 'border-slate-200 bg-slate-50 text-slate-600 dark:border-white/[0.08] dark:bg-white/[0.04] dark:text-slate-300'
};

function groupTransactionLines(lines) {
  const groups = new Map();
  lines.forEach((line) => {
    const key = String(line.transaction_id || line.tr_number || line.id);
    if (!groups.has(key)) groups.set(key, { ...line, lines: [] });
    groups.get(key).lines.push(line);
  });

  return Array.from(groups.values()).map((transaction) => {
    const first = transaction.lines[0];
    const requestedUnits = transaction.lines.reduce((sum, line) => sum + Number(line.units_requested || 0), 0);
    const dispensedQty = transaction.lines.reduce((sum, line) => sum + Number(line.qty_dispensed || 0), 0);
    const itemSubtotal = transaction.lines.reduce((sum, line) => sum + Number(line.amount_paid || 0), 0);
    const productLabels = transaction.lines.map((line) => {
      const name = line.product_name || (line.item_type === 'paper' ? `Paper (${line.paper_size || 'unspecified'})` : 'Ballpen');
      const unit = line.item_type === 'paper' ? 'unit' : 'pc';
      return `${name} × ${line.units_requested || 0} ${unit}${Number(line.units_requested) === 1 ? '' : 's'}`;
    });

    return {
      ...transaction,
      lines: transaction.lines,
      itemSubtotal,
      requestedUnits,
      dispensedQty,
      itemSummary: productLabels.join(' · '),
      creditReceived: Number(first.credit_received || 0),
      changeDue: Number(first.change_due || 0),
      changePaid: Number(first.change_paid || 0),
      changeOwed: Number(first.change_owed || 0),
      refundPaid: Number(first.refund_paid || 0),
      netRetained: Number(first.credit_received || 0) - Number(first.change_paid || 0) - Number(first.refund_paid || 0)
    };
  }).sort((a, b) => new Date(b.transaction_date) - new Date(a.transaction_date));
}

function csvCell(value) {
  let text = String(value ?? '');
  if (/^[\s]*[=+\-@]/.test(text)) text = `'${text}`;
  return `"${text.replace(/"/g, '""')}"`;
}

function saveBlob(content, mimeType, filename) {
  const blob = new Blob([content], { type: mimeType });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  link.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export default function Reports() {
  const timeZone = Intl.DateTimeFormat().resolvedOptions().timeZone || 'Local time';
  const [startDate, setStartDate] = useState(() => dateInputValue(30));
  const [endDate, setEndDate] = useState(() => dateInputValue());
  const [itemType, setItemType] = useState('all');
  const [statusFilter, setStatusFilter] = useState('all');
  const [searchQuery, setSearchQuery] = useState('');
  const [transactions, setTransactions] = useState([]);
  const [loading, setLoading] = useState(false);
  const [exporting, setExporting] = useState(null);
  const [error, setError] = useState('');
  const [lastUpdated, setLastUpdated] = useState(null);
  const [page, setPage] = useState(1);

  const generateReport = useCallback(async () => {
    if (!startDate || !endDate || startDate > endDate) {
      setError('Choose a valid date range. The start date must not be after the end date.');
      setTransactions([]);
      return;
    }

    setLoading(true);
    setError('');
    setTransactions([]);
    setPage(1);
    try {
      const response = await axios.get('/api/machine/transactions', {
        params: {
          startAt: localDayStartIso(startDate),
          endAt: localDayAfterIso(endDate),
          itemType
        }
      });
      setTransactions(Array.isArray(response.data) ? response.data : []);
      setLastUpdated(new Date());
    } catch (requestError) {
      console.error('Error generating report:', requestError);
      setError(requestError.response?.data?.message || 'Could not load report data. Check the connection and try again.');
    } finally {
      setLoading(false);
    }
  }, [endDate, itemType, startDate]);

  useEffect(() => {
    generateReport();
  }, [generateReport]);

  const groupedTransactions = useMemo(() => groupTransactionLines(transactions), [transactions]);
  const visibleTransactions = useMemo(() => {
    const query = searchQuery.trim().toLowerCase();
    return groupedTransactions.filter((transaction) => {
      const status = getStatusInfo(transaction);
      const matchesStatus = statusFilter === 'all' || status.key === statusFilter;
      const searchText = [
        transaction.tr_number,
        transaction.transaction_id,
        transaction.status,
        transaction.failure_reason,
        transaction.itemSummary
      ].filter(Boolean).join(' ').toLowerCase();
      return matchesStatus && (!query || searchText.includes(query));
    });
  }, [groupedTransactions, searchQuery, statusFilter]);

  useEffect(() => setPage(1), [searchQuery, statusFilter]);

  const summary = useMemo(() => visibleTransactions.reduce((totals, transaction) => {
    const status = getStatusInfo(transaction).key;
    totals.transactions++;
    totals.requestedUnits += transaction.requestedUnits;
    totals.dispensedQty += transaction.dispensedQty;
    totals.creditReceived += transaction.creditReceived;
    totals.changePaid += transaction.changePaid;
    totals.changeOwed += transaction.changeOwed;
    totals.refundPaid += transaction.refundPaid;
    totals.netRetained += transaction.netRetained;
    if (status === 'completed') totals.completed++;
    if (status === 'failed' || status === 'partial') totals.needsReview++;
    return totals;
  }, {
    transactions: 0,
    requestedUnits: 0,
    dispensedQty: 0,
    creditReceived: 0,
    changePaid: 0,
    changeOwed: 0,
    refundPaid: 0,
    netRetained: 0,
    completed: 0,
    needsReview: 0
  }), [visibleTransactions]);

  const totalPages = Math.max(1, Math.ceil(visibleTransactions.length / PAGE_SIZE));
  const pageTransactions = visibleTransactions.slice((page - 1) * PAGE_SIZE, page * PAGE_SIZE);
  const filenameBase = `paper-vendo-report_${startDate}_to_${endDate}`;

  const exportCSV = () => {
    if (!visibleTransactions.length) return;
    setExporting('csv');
    setError('');
    try {
      const rows = [
        ['Paper Vendo Machine Report'],
        [`Period: ${startDate} through ${endDate} (${timeZone})`],
        [`Item filter: ${itemType}`, `Status filter: ${statusFilter}`, `Generated: ${formatDateTime(new Date(), timeZone)}`],
        [],
        ['TR Number', 'Transaction ID', 'Date & Time', 'Status', 'Items', 'Requested Units', 'Dispensed Qty', 'Item Subtotal (PHP)', 'Credits Received (PHP)', 'Change Due (PHP)', 'Change Paid (PHP)', 'Change Owed (PHP)', 'Refund Recorded (PHP)', 'Net Retained (PHP)', 'Failure Reason']
      ];
      visibleTransactions.forEach((transaction) => rows.push([
        transaction.tr_number,
        transaction.transaction_id,
        formatDateTime(transaction.transaction_date, timeZone),
        transaction.status,
        transaction.itemSummary,
        transaction.requestedUnits,
        transaction.dispensedQty,
        transaction.itemSubtotal.toFixed(2),
        transaction.creditReceived.toFixed(2),
        transaction.changeDue.toFixed(2),
        transaction.changePaid.toFixed(2),
        transaction.changeOwed.toFixed(2),
        transaction.refundPaid.toFixed(2),
        transaction.netRetained.toFixed(2),
        transaction.failure_reason || ''
      ]));
      rows.push([], ['Summary', `Transactions: ${summary.transactions}`, `Completed: ${summary.completed}`, `Failed/partial: ${summary.needsReview}`]);
      const csv = '\uFEFF' + rows.map((row) => row.map(csvCell).join(',')).join('\r\n');
      saveBlob(csv, 'text/csv;charset=utf-8;', `${filenameBase}.csv`);
    } catch (exportError) {
      console.error('CSV export failed:', exportError);
      setError('Could not export CSV. Please try again.');
    } finally {
      setExporting(null);
    }
  };

  const exportPDF = () => {
    if (!visibleTransactions.length) return;
    setExporting('pdf');
    setError('');
    try {
      const doc = new jsPDF({ orientation: 'landscape', unit: 'mm', format: 'a4' });
      const pageWidth = doc.internal.pageSize.getWidth();
      const pageHeight = doc.internal.pageSize.getHeight();
      const generatedAt = formatDateTime(new Date(), timeZone);

      doc.setFont('helvetica', 'bold');
      doc.setFontSize(17);
      doc.setTextColor(15, 23, 42);
      doc.text('PAPER VENDO | TRANSACTION REPORT', 12, 15);
      doc.setFont('helvetica', 'normal');
      doc.setFontSize(8.5);
      doc.setTextColor(71, 85, 105);
      doc.text(`Period: ${startDate} to ${endDate} (${timeZone})`, 12, 22);
      doc.text(`Filters: ${itemType === 'all' ? 'All products' : itemType} · ${statusFilter === 'all' ? 'All statuses' : statusFilter}`, 12, 27);
      doc.text(`Generated: ${generatedAt}`, 12, 32);
      doc.text(`Records: ${summary.transactions}   Completed: ${summary.completed}   Failed/partial: ${summary.needsReview}`, 12, 39);
      doc.text(`Credits received: ${currency(summary.creditReceived)}   Change paid: ${currency(summary.changePaid)}   Refunds recorded: ${currency(summary.refundPaid)}`, 12, 45);
      doc.text(`Net retained: ${currency(summary.netRetained)}   Change still owed: ${currency(summary.changeOwed)}   Requested units: ${summary.requestedUnits}   Dispensed output: ${summary.dispensedQty}`, 12, 51);

      autoTable(doc, {
        startY: 58,
        margin: { left: 10, right: 10, bottom: 14 },
        head: [[
          'TR / ID', 'Date & Time', 'Status / Issue', 'Items', 'Units', 'Output',
          'Credits', 'Change Due / Paid / Owed', 'Refund', 'Net Retained'
        ]],
        body: visibleTransactions.map((transaction) => [
          `${transaction.tr_number || '—'}\n${transaction.transaction_id || ''}`,
          formatDateTime(transaction.transaction_date, timeZone),
          `${getStatusInfo(transaction).label}${transaction.failure_reason ? `\n${transaction.failure_reason}` : ''}`,
          transaction.itemSummary,
          String(transaction.requestedUnits),
          String(transaction.dispensedQty),
          currency(transaction.creditReceived),
          `${currency(transaction.changeDue)} / ${currency(transaction.changePaid)} / ${currency(transaction.changeOwed)}`,
          currency(transaction.refundPaid),
          currency(transaction.netRetained)
        ]),
        theme: 'striped',
        headStyles: { fillColor: [15, 83, 92], textColor: [255, 255, 255], fontSize: 7.5 },
        alternateRowStyles: { fillColor: [241, 248, 246] },
        styles: { font: 'helvetica', fontSize: 7, cellPadding: 2.2, overflow: 'linebreak', valign: 'middle' },
        columnStyles: {
          0: { cellWidth: 24 },
          1: { cellWidth: 32 },
          2: { cellWidth: 36 },
          3: { cellWidth: 45 },
          4: { cellWidth: 12, halign: 'center' },
          5: { cellWidth: 12, halign: 'center' },
          6: { cellWidth: 22, halign: 'right' },
          7: { cellWidth: 36, halign: 'right' },
          8: { cellWidth: 21, halign: 'right' },
          9: { cellWidth: 24, halign: 'right' }
        },
        didDrawPage: () => {
          doc.setFontSize(7.5);
          doc.setTextColor(100, 116, 139);
          doc.text(`Paper Vendo · ${timeZone}`, 10, pageHeight - 6);
        }
      });

      const pageCount = doc.internal.getNumberOfPages();
      for (let pageNumber = 1; pageNumber <= pageCount; pageNumber++) {
        doc.setPage(pageNumber);
        doc.setFontSize(7.5);
        doc.setTextColor(100, 116, 139);
        doc.text(`Page ${pageNumber} of ${pageCount}`, pageWidth - 10, pageHeight - 6, { align: 'right' });
      }
      doc.save(`${filenameBase}.pdf`);
    } catch (exportError) {
      console.error('PDF export failed:', exportError);
      setError('Could not generate the PDF. Please try again.');
    } finally {
      setExporting(null);
    }
  };

  const cardClass = 'rounded-2xl border border-slate-200 bg-white p-5 shadow-sm dark:border-white/[0.06] dark:bg-[#161F30]';

  return (
    <div className="mx-auto max-w-7xl space-y-7 font-sans">
      <header className="flex flex-col justify-between gap-4 sm:flex-row sm:items-end">
        <div>
          <div className="flex items-center gap-3">
            <div className="flex h-12 w-12 items-center justify-center rounded-2xl border border-slate-200 bg-white p-2 shadow-sm dark:border-white/[0.08]">
              <img src="/logo.png" alt="P&B V Machine" className="h-full w-full object-contain" />
            </div>
            <div>
              <p className="text-xs font-bold uppercase tracking-[0.2em] text-primary-500">Operations intelligence</p>
              <h1 className="font-display text-3xl font-extrabold leading-tight text-slate-800 dark:text-white md:text-4xl">Reports</h1>
            </div>
          </div>
          <p className="mt-2 max-w-2xl text-sm text-slate-500 dark:text-slate-400">
            Review transaction outcomes, dispensing, settlement, and customer balances. Report dates use your local timezone: <span className="font-semibold">{timeZone}</span>.
          </p>
        </div>
        {lastUpdated && (
          <p className="text-xs text-slate-400">Updated {formatDateTime(lastUpdated, timeZone)}</p>
        )}
      </header>

      <section className={`${cardClass} space-y-5`}>
        <div className="flex items-center gap-2">
          <Filter className="h-5 w-5 text-primary-500" />
          <div>
            <h2 className="font-display text-lg font-bold text-slate-800 dark:text-white">Report filters</h2>
            <p className="text-xs text-slate-500 dark:text-slate-400">Choose the local date range and report scope.</p>
          </div>
        </div>
        <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 xl:grid-cols-5">
          <label className="space-y-1.5 text-xs font-bold uppercase tracking-wider text-slate-400">
            Start date
            <span className="relative block">
              <Calendar className="absolute left-3.5 top-1/2 h-4 w-4 -translate-y-1/2" />
              <input type="date" value={startDate} onChange={(event) => setStartDate(event.target.value)} className="h-11 w-full rounded-xl border border-slate-200 bg-slate-50 pl-10 pr-3 text-sm font-medium normal-case tracking-normal text-slate-800 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-slate-900 dark:text-white" />
            </span>
          </label>
          <label className="space-y-1.5 text-xs font-bold uppercase tracking-wider text-slate-400">
            End date
            <span className="relative block">
              <Calendar className="absolute left-3.5 top-1/2 h-4 w-4 -translate-y-1/2" />
              <input type="date" value={endDate} onChange={(event) => setEndDate(event.target.value)} className="h-11 w-full rounded-xl border border-slate-200 bg-slate-50 pl-10 pr-3 text-sm font-medium normal-case tracking-normal text-slate-800 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-slate-900 dark:text-white" />
            </span>
          </label>
          <label className="space-y-1.5 text-xs font-bold uppercase tracking-wider text-slate-400">
            Item type
            <span className="relative block">
              <ShoppingBag className="absolute left-3.5 top-1/2 h-4 w-4 -translate-y-1/2" />
              <select value={itemType} onChange={(event) => setItemType(event.target.value)} className="h-11 w-full rounded-xl border border-slate-200 bg-slate-50 pl-10 pr-3 text-sm font-medium normal-case tracking-normal text-slate-800 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-slate-900 dark:text-white">
                <option value="all">All items</option>
                <option value="paper">Paper</option>
                <option value="pen">Ballpens</option>
              </select>
            </span>
          </label>
          <label className="space-y-1.5 text-xs font-bold uppercase tracking-wider text-slate-400">
            Outcome
            <select value={statusFilter} onChange={(event) => setStatusFilter(event.target.value)} className="mt-1.5 h-11 w-full rounded-xl border border-slate-200 bg-slate-50 px-3 text-sm font-medium normal-case tracking-normal text-slate-800 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-slate-900 dark:text-white">
              <option value="all">All outcomes</option>
              <option value="completed">Completed</option>
              <option value="partial">Partial success</option>
              <option value="failed">Failed / cancelled</option>
              <option value="change">Change owed</option>
              <option value="refunded">Refunded</option>
              <option value="other">Other / pending</option>
            </select>
          </label>
          <div className="flex items-end">
            <button onClick={generateReport} disabled={loading} className="flex h-11 w-full items-center justify-center gap-2 rounded-xl bg-primary-500 text-sm font-bold text-white shadow-md shadow-primary-500/10 transition-colors hover:bg-primary-600 disabled:opacity-60">
              {loading ? <RefreshCw className="h-4 w-4 animate-spin" /> : <FileText className="h-4 w-4" />}
              {loading ? 'Loading report' : 'Run report'}
            </button>
          </div>
        </div>
      </section>

      {error && <div role="alert" className="flex items-start gap-2.5 rounded-xl border border-red-500/20 bg-red-500/10 p-4 text-sm font-semibold text-red-600 dark:text-red-400"><AlertCircle className="mt-0.5 h-4 w-4 shrink-0" />{error}</div>}

      <section className="grid grid-cols-1 gap-4 sm:grid-cols-2 xl:grid-cols-4">
        <div className={cardClass}>
          <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Transactions</p>
          <p className="mt-2 font-display text-3xl font-extrabold text-slate-800 dark:text-white">{summary.transactions}</p>
          <p className="mt-1 text-xs text-slate-500">{summary.completed} completed · {summary.needsReview} failed or partial</p>
        </div>
        <div className={cardClass}>
          <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Net retained · estimate</p>
          <p className="mt-2 font-display text-2xl font-extrabold text-emerald-600 dark:text-emerald-400">{currency(summary.netRetained)}</p>
          <p className="mt-1 text-xs text-slate-500">Credits received less change paid and refunds recorded</p>
        </div>
        <div className={cardClass}>
          <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Change still owed</p>
          <p className="mt-2 font-display text-2xl font-extrabold text-amber-600 dark:text-amber-400">{currency(summary.changeOwed)}</p>
          <p className="mt-1 text-xs text-slate-500">Unpaid change recorded on matching orders</p>
        </div>
        <div className={cardClass}>
          <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Requested / dispensed</p>
          <p className="mt-2 font-display text-2xl font-extrabold text-slate-800 dark:text-white">{summary.requestedUnits} <span className="text-slate-400">/</span> {summary.dispensedQty}</p>
          <p className="mt-1 text-xs text-slate-500">Units requested / physical output reported</p>
        </div>
      </section>

      <section className="grid grid-cols-1 gap-3 rounded-2xl border border-slate-200 bg-slate-50/80 p-4 sm:grid-cols-3 dark:border-white/[0.06] dark:bg-white/[0.02]">
        <div className="px-2 py-1"><p className="text-[10px] font-extrabold uppercase tracking-wider text-slate-400">Credits received</p><p className="mt-1 text-sm font-bold text-slate-700 dark:text-slate-200">{currency(summary.creditReceived)}</p></div>
        <div className="border-slate-200 px-2 py-1 sm:border-l sm:pl-5 dark:border-white/[0.08]"><p className="text-[10px] font-extrabold uppercase tracking-wider text-slate-400">Change paid</p><p className="mt-1 text-sm font-bold text-slate-700 dark:text-slate-200">{currency(summary.changePaid)}</p></div>
        <div className="border-slate-200 px-2 py-1 sm:border-l sm:pl-5 dark:border-white/[0.08]"><p className="text-[10px] font-extrabold uppercase tracking-wider text-slate-400">Refunds recorded</p><p className="mt-1 text-sm font-bold text-slate-700 dark:text-slate-200">{currency(summary.refundPaid)}</p></div>
      </section>

      <section className={`${cardClass} space-y-5`}>
        <div className="flex flex-col justify-between gap-4 lg:flex-row lg:items-center">
          <div>
            <h2 className="font-display text-lg font-bold text-slate-800 dark:text-white">Transaction detail</h2>
            <p className="mt-1 text-xs text-slate-500">{visibleTransactions.length} matching transactions · currency shown in PHP</p>
          </div>
          <div className="flex flex-col gap-2 sm:flex-row">
            <label className="relative min-w-52">
              <Search className="absolute left-3.5 top-1/2 h-4 w-4 -translate-y-1/2 text-slate-400" />
              <input value={searchQuery} onChange={(event) => setSearchQuery(event.target.value)} placeholder="Search TR, item, issue..." className="h-10 w-full rounded-xl border border-slate-200 bg-slate-50 pl-10 pr-3 text-sm text-slate-800 outline-none focus:border-primary-500 dark:border-white/[0.08] dark:bg-slate-900 dark:text-white" />
            </label>
            <button onClick={exportCSV} disabled={!visibleTransactions.length || loading || exporting !== null} className="flex h-10 items-center justify-center gap-2 rounded-xl border border-slate-200 px-3 text-xs font-bold text-slate-600 transition hover:bg-slate-50 disabled:opacity-50 dark:border-white/[0.08] dark:text-slate-300 dark:hover:bg-white/[0.04]">
              {exporting === 'csv' ? <RefreshCw className="h-4 w-4 animate-spin" /> : <Download className="h-4 w-4" />} CSV
            </button>
            <button onClick={exportPDF} disabled={!visibleTransactions.length || loading || exporting !== null} className="flex h-10 items-center justify-center gap-2 rounded-xl bg-[#0F535C] px-3 text-xs font-bold text-white transition hover:bg-[#0B4148] disabled:opacity-50">
              {exporting === 'pdf' ? <RefreshCw className="h-4 w-4 animate-spin" /> : <FileText className="h-4 w-4" />} PDF
            </button>
          </div>
        </div>

        {itemType !== 'all' && <p className="rounded-lg bg-slate-50 px-3 py-2 text-xs text-slate-500 dark:bg-white/[0.03]">Settlement amounts describe the full transaction when an order contains both paper and pens; item rows are limited to the selected category.</p>}

        <div className="overflow-x-auto rounded-xl border border-slate-100 dark:border-white/[0.06]">
          <table className="w-full min-w-[1180px] border-collapse text-left text-sm">
            <thead className="bg-slate-50 text-[10px] font-extrabold uppercase tracking-wider text-slate-400 dark:bg-white/[0.03]">
              <tr>
                <th className="px-3 py-3">Transaction</th>
                <th className="px-3 py-3">Date & time</th>
                <th className="px-3 py-3">Outcome</th>
                <th className="px-3 py-3">Items</th>
                <th className="px-3 py-3 text-center">Units</th>
                <th className="px-3 py-3 text-center">Output</th>
                <th className="px-3 py-3 text-right">Credit in</th>
                <th className="px-3 py-3 text-right">Change paid / owed</th>
                <th className="px-3 py-3 text-right">Refund</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-100 text-slate-700 dark:divide-white/[0.05] dark:text-slate-200">
              {loading ? (
                <tr><td colSpan="9" className="py-14 text-center text-slate-400"><RefreshCw className="mx-auto mb-2 h-5 w-5 animate-spin" />Loading matching transactions…</td></tr>
              ) : pageTransactions.length === 0 ? (
                <tr><td colSpan="9" className="py-14 text-center font-medium text-slate-400">No transactions match these filters.</td></tr>
              ) : pageTransactions.map((transaction) => {
                const status = getStatusInfo(transaction);
                const StatusIcon = status.icon;
                return (
                  <tr key={transaction.transaction_id || transaction.id} className="align-top transition-colors hover:bg-slate-50/70 dark:hover:bg-white/[0.02]">
                    <td className="whitespace-nowrap px-3 py-3.5"><div className="font-mono text-xs font-bold text-slate-700 dark:text-white">{transaction.tr_number || '—'}</div><div className="mt-1 text-[10px] text-slate-400">ID {transaction.transaction_id || transaction.id}</div></td>
                    <td className="whitespace-nowrap px-3 py-3.5 text-xs text-slate-500">{formatDateTime(transaction.transaction_date, timeZone)}</td>
                    <td className="max-w-48 px-3 py-3.5"><span className={`inline-flex items-center gap-1 rounded-full border px-2 py-1 text-[10px] font-extrabold uppercase ${statusStyles[status.tone]}`}><StatusIcon className="h-3 w-3" />{status.label}</span>{transaction.failure_reason && <p className="mt-1.5 max-w-44 text-[11px] leading-snug text-red-600 dark:text-red-300">{transaction.failure_reason}</p>}</td>
                    <td className="max-w-64 px-3 py-3.5 text-xs font-semibold leading-relaxed">{transaction.itemSummary}</td>
                    <td className="px-3 py-3.5 text-center text-xs font-bold">{transaction.requestedUnits}</td>
                    <td className="px-3 py-3.5 text-center text-xs font-semibold">{transaction.dispensedQty}</td>
                    <td className="whitespace-nowrap px-3 py-3.5 text-right text-xs font-bold">{currency(transaction.creditReceived)}</td>
                    <td className="whitespace-nowrap px-3 py-3.5 text-right text-xs"><span className="font-semibold">{currency(transaction.changePaid)}</span><span className="text-slate-400"> / </span><span className={transaction.changeOwed > 0 ? 'font-bold text-amber-600 dark:text-amber-400' : 'text-slate-500'}>{currency(transaction.changeOwed)}</span></td>
                    <td className="whitespace-nowrap px-3 py-3.5 text-right text-xs font-semibold">{currency(transaction.refundPaid)}</td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>

        <div className="flex flex-col items-center justify-between gap-3 sm:flex-row">
          <p className="text-xs text-slate-500">Page {page} of {totalPages} · {visibleTransactions.length} transactions</p>
          <div className="flex items-center gap-2">
            <button onClick={() => setPage((current) => Math.max(1, current - 1))} disabled={page <= 1} aria-label="Previous page" className="flex h-9 w-9 items-center justify-center rounded-lg border border-slate-200 text-slate-600 hover:bg-slate-50 disabled:opacity-40 dark:border-white/[0.08] dark:text-slate-300 dark:hover:bg-white/[0.04]"><ChevronLeft className="h-4 w-4" /></button>
            <button onClick={() => setPage((current) => Math.min(totalPages, current + 1))} disabled={page >= totalPages} aria-label="Next page" className="flex h-9 w-9 items-center justify-center rounded-lg border border-slate-200 text-slate-600 hover:bg-slate-50 disabled:opacity-40 dark:border-white/[0.08] dark:text-slate-300 dark:hover:bg-white/[0.04]"><ChevronRight className="h-4 w-4" /></button>
          </div>
        </div>
      </section>

      <p className="text-xs leading-relaxed text-slate-400">
        “Net retained” is an estimate calculated from recorded credits received minus recorded change paid and refunds. Matching an item category filters line items, while settlement values remain transaction-level. Transactions without a sales line are not included in this sales report.
      </p>
    </div>
  );
}
