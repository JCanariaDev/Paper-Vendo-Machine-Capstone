import React from 'react';
import { Link } from 'react-router-dom';
import {
  AlertTriangle, BookOpen, Boxes, CircleHelp, Coins,
  FileText, PackagePlus, ShieldCheck, ShoppingCart
} from 'lucide-react';

const sections = [
  {
    title: 'Customer: make a purchase',
    icon: ShoppingCart,
    tone: 'emerald',
    steps: [
      'Check the screen for Wi-Fi and machine-ready indicators before beginning.',
      'Insert coins and wait for the displayed credit to update.',
      'Choose paper or a ballpen, select the product, and add it to the cart.',
      'Review the order summary, then press Checkout once and wait for dispensing and change release to finish.',
      'Collect the items and any change from the output area. If the screen reports a failure, keep the transaction number for the administrator.'
    ]
  },
  {
    title: 'Admin: paper pads and bays',
    icon: Boxes,
    tone: 'amber',
    steps: [
      'Use Inventory to check the product size, storage count, assigned bay, presence state, and recorded sheet count.',
      'Bay 1 accepts 1_whole or crosswise paper. Bay 2 accepts lengthwise or 1/4 paper.',
      'Each bay is designed for one installed pad. Remove or replace the existing pad before loading another.',
      'When installing a pad, count the sheets physically and enter that actual starting count. Do not assume the catalog capacity is the physical count.',
      'Use Correct sheet count only after a physical recount or confirmed failed dispense. Add a reason so the audit trail explains the change.',
      'A removed or replaced partial pad is not automatically returned to storage. Discard it or keep it separate and reconcile storage deliberately.'
    ]
  },
  {
    title: 'Admin: ballpen stock',
    icon: PackagePlus,
    tone: 'blue',
    steps: [
      'Check master storage and the pen bay count on Inventory.',
      'Use the pen bay refill control to transfer the physical pieces being loaded from storage into the bay.',
      'Confirm that the selected pen type matches the physical stock. Avoid editing counts unless the stock has been physically checked.'
    ]
  },
  {
    title: 'Admin: coin hopper and change',
    icon: Coins,
    tone: 'cyan',
    steps: [
      'Use Machine Monitor and transaction history to verify whether change was due, released, or left owed.',
      'Restock the hopper only using the approved physical procedure for your machine and confirm the coin count through the machine’s supported status process.',
      'Never handle the hopper’s mains-powered section while energized. Isolate power first; electrical servicing should be done by a qualified person.',
      'If the machine reports change failure, record the transaction number and compare the transaction record with the related machine logs before changing inventory counts.'
    ]
  },
  {
    title: 'When something goes wrong',
    icon: AlertTriangle,
    tone: 'rose',
    steps: [
      'Write down the transaction number and the exact screen message before restarting the machine.',
      'Check Machine Monitor for online state, current activity, and recent transactions.',
      'Open System Logs and search the transaction number. Look for the first ERROR/WARNING and its cause metadata.',
      'For a paper issue, verify the assigned bay, physical pad presence, actual remaining sheets, and exit-IR results. Correct counts only after checking the physical pad.',
      'If a transaction is still in progress or credits are held, do not repeatedly reboot or create another order until its status is understood.'
    ]
  }
];

const toneClasses = {
  emerald: 'bg-emerald-100 text-emerald-700 dark:bg-emerald-950/40 dark:text-emerald-300',
  amber: 'bg-amber-100 text-amber-700 dark:bg-amber-950/40 dark:text-amber-300',
  blue: 'bg-blue-100 text-blue-700 dark:bg-blue-950/40 dark:text-blue-300',
  cyan: 'bg-cyan-100 text-cyan-700 dark:bg-cyan-950/40 dark:text-cyan-300',
  rose: 'bg-rose-100 text-rose-700 dark:bg-rose-950/40 dark:text-rose-300'
};

export default function Help() {
  return (
    <main className="mx-auto max-w-7xl space-y-8 pb-12 font-sans">
      <header className="relative overflow-hidden rounded-3xl border border-slate-200 bg-white p-7 shadow-sm dark:border-white/[0.06] dark:bg-[#161F30] md:p-9">
        <div className="pointer-events-none absolute -right-12 -top-16 h-64 w-64 rounded-full bg-emerald-400/10 blur-3xl" />
        <div className="relative flex flex-col gap-5 md:flex-row md:items-center md:justify-between">
          <div className="max-w-2xl">
            <div className="mb-3 inline-flex items-center gap-2 rounded-full border border-primary-200 bg-primary-50 px-3 py-1 text-xs font-extrabold uppercase tracking-wider text-primary-700 dark:border-primary-800/50 dark:bg-primary-950/30 dark:text-primary-300">
              <BookOpen className="h-3.5 w-3.5" /> Operator guide
            </div>
            <h1 className="font-display text-3xl font-extrabold leading-tight text-slate-800 dark:text-white md:text-4xl">How to run the machine</h1>
            <p className="mt-3 text-sm leading-relaxed text-slate-500 dark:text-slate-400">A practical guide for customers and operators. Use it alongside the physical labels on the kiosk; the labels and on-screen prompts take priority if a procedure differs.</p>
          </div>
          <div className="flex flex-wrap gap-2">
            <Link to="/inventory" title="Open product storage and bay management" className="inline-flex items-center gap-2 rounded-xl bg-primary-500 px-4 py-2.5 text-sm font-bold text-white transition hover:bg-primary-600"><Boxes className="h-4 w-4" />Inventory</Link>
            <Link to="/logs" title="Search machine and system event records" className="inline-flex items-center gap-2 rounded-xl border border-slate-200 bg-white px-4 py-2.5 text-sm font-bold text-slate-700 transition hover:bg-slate-50 dark:border-white/[0.08] dark:bg-white/[0.03] dark:text-slate-200"><FileText className="h-4 w-4" />System Logs</Link>
          </div>
        </div>
      </header>

      <section className="grid gap-4 lg:grid-cols-2">
        {sections.map(({ title, icon: Icon, tone, steps }) => (
          <article key={title} className="rounded-2xl border border-slate-200 bg-white p-5 shadow-sm dark:border-white/[0.06] dark:bg-[#161F30] sm:p-6">
            <div className="flex items-center gap-3 border-b border-slate-100 pb-4 dark:border-white/[0.06]">
              <span className={`flex h-10 w-10 items-center justify-center rounded-xl ${toneClasses[tone]}`}><Icon className="h-5 w-5" /></span>
              <h2 className="font-display text-lg font-bold text-slate-800 dark:text-white">{title}</h2>
            </div>
            <ol className="mt-4 space-y-3">
              {steps.map((step, index) => (
                <li key={step} className="flex gap-3 text-sm leading-relaxed text-slate-600 dark:text-slate-300">
                  <span className="mt-0.5 flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-slate-100 text-[11px] font-extrabold text-slate-500 dark:bg-white/[0.06] dark:text-slate-300">{index + 1}</span>
                  <span>{step}</span>
                </li>
              ))}
            </ol>
          </article>
        ))}
      </section>

      <aside className="flex gap-3 rounded-2xl border border-amber-200 bg-amber-50 p-4 text-sm text-amber-900 dark:border-amber-800/40 dark:bg-amber-950/20 dark:text-amber-200">
        <ShieldCheck className="mt-0.5 h-5 w-5 shrink-0" />
        <p><strong>Keep a clear audit trail.</strong> Before changing a count or resolving a failed transaction, record the transaction number, check the physical stock, and enter a short reason in the relevant admin action.</p>
      </aside>

      <div className="flex items-start gap-3 rounded-2xl border border-slate-200 bg-slate-50 p-4 text-xs leading-relaxed text-slate-500 dark:border-white/[0.06] dark:bg-white/[0.02] dark:text-slate-400">
        <CircleHelp className="mt-0.5 h-4 w-4 shrink-0" />
        <p>For software and hardware faults that are not covered here, preserve the logs and ask the project administrator before changing wiring, database records, or machine configuration.</p>
      </div>
    </main>
  );
}
