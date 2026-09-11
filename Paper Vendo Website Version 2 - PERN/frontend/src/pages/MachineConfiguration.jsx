import React, { useEffect, useState } from 'react';
import axios from 'axios';
import { AlertTriangle, CheckCircle2, Eye, EyeOff, LoaderCircle, LockKeyhole, Save, ShieldCheck, Wifi } from 'lucide-react';

export default function MachineConfiguration() {
  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [config, setConfig] = useState(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState('');
  const [error, setError] = useState('');

  const loadConfiguration = async () => {
    try {
      const response = await axios.get('/api/machine/network-config');
      setConfig(response.data.config);
      setSsid(response.data.config?.ssid || '');
    } catch (err) {
      setError(err.response?.data?.message || 'Could not retrieve the network configuration.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadConfiguration();
  }, []);

  const saveConfiguration = async (event) => {
    event.preventDefault();
    setMessage('');
    setError('');
    setSaving(true);

    try {
      const response = await axios.put('/api/machine/network-config', { ssid, password });
      setConfig(response.data.config);
      setPassword('');
      setMessage(response.data.message);
    } catch (err) {
      setError(err.response?.data?.message || 'Could not save the network configuration.');
    } finally {
      setSaving(false);
    }
  };

  if (loading) {
    return <div className="flex h-[70vh] items-center justify-center"><LoaderCircle className="w-9 h-9 animate-spin text-primary-500" /></div>;
  }

  return (
    <div className="max-w-4xl mx-auto space-y-8 font-sans">
      <div>
        <div className="flex items-center gap-3">
          <div className="flex h-12 w-12 items-center justify-center rounded-2xl bg-primary-500/10 text-primary-500">
            <Wifi className="w-6 h-6" />
          </div>
          <div>
            <h1 className="font-display text-3xl md:text-4xl font-extrabold text-slate-800 dark:text-white">Machine Configuration</h1>
            <p className="mt-1 text-sm text-slate-500 dark:text-slate-400">Stage the Wi-Fi network that the machine should use.</p>
          </div>
        </div>
      </div>

      <div className="flex gap-3 rounded-2xl border border-amber-500/30 bg-amber-500/10 p-4 text-amber-800 dark:text-amber-200">
        <AlertTriangle className="mt-0.5 h-5 w-5 shrink-0" />
        <div className="text-sm leading-relaxed">
          <p className="font-bold">Staging only — not yet sent to the machine</p>
          <p className="mt-1 opacity-90">Credentials are encrypted on the server. The ESP32 checks for a new configuration and applies it with a rollback safeguard if the new network cannot be reached.</p>
        </div>
      </div>

      {error && <div className="flex gap-3 rounded-2xl border border-red-500/30 bg-red-500/10 p-4 text-sm font-semibold text-red-700 dark:text-red-300"><AlertTriangle className="h-5 w-5 shrink-0" />{error}</div>}
      {message && <div className="flex gap-3 rounded-2xl border border-emerald-500/30 bg-emerald-500/10 p-4 text-sm font-semibold text-emerald-700 dark:text-emerald-300"><CheckCircle2 className="h-5 w-5 shrink-0" />{message}</div>}

      <div className="rounded-2xl border border-slate-200 bg-white p-6 shadow-sm dark:border-white/[0.08] dark:bg-[#161F30]">
        <div className="mb-6 flex items-start gap-3">
          <LockKeyhole className="mt-0.5 h-5 w-5 text-primary-500" />
          <div>
            <h2 className="font-display text-lg font-bold text-slate-800 dark:text-white">Wi-Fi credentials</h2>
            <p className="mt-1 text-xs leading-relaxed text-slate-500 dark:text-slate-400">Only superadmins can stage a network. The password is encrypted on the server and is never shown again in this dashboard.</p>
          </div>
        </div>

        <form onSubmit={saveConfiguration} className="space-y-5">
          <label className="block">
            <span className="mb-1.5 block text-xs font-bold uppercase tracking-wider text-slate-500">Network name (SSID)</span>
            <input value={ssid} onChange={(event) => setSsid(event.target.value)} maxLength={32} required placeholder="e.g. Campus-WiFi" className="h-12 w-full rounded-xl border border-slate-200 bg-slate-50 px-4 text-sm text-slate-800 outline-none transition focus:border-primary-500 focus:ring-4 focus:ring-primary-500/10 dark:border-white/[0.08] dark:bg-white/[0.03] dark:text-white" />
          </label>
          <label className="block">
            <span className="mb-1.5 block text-xs font-bold uppercase tracking-wider text-slate-500">Wi-Fi password</span>
            <div className="relative">
              <input type={showPassword ? 'text' : 'password'} value={password} onChange={(event) => setPassword(event.target.value)} minLength={8} maxLength={63} required placeholder="Enter the Wi-Fi password" className="h-12 w-full rounded-xl border border-slate-200 bg-slate-50 px-4 pr-12 text-sm text-slate-800 outline-none transition focus:border-primary-500 focus:ring-4 focus:ring-primary-500/10 dark:border-white/[0.08] dark:bg-white/[0.03] dark:text-white" />
              <button type="button" onClick={() => setShowPassword((visible) => !visible)} className="absolute inset-y-0 right-0 flex w-12 items-center justify-center text-slate-400 hover:text-slate-700 dark:hover:text-white" aria-label={showPassword ? 'Hide password' : 'Show password'}>
                {showPassword ? <EyeOff className="h-4 w-4" /> : <Eye className="h-4 w-4" />}
              </button>
            </div>
          </label>
          <button disabled={saving} className="inline-flex items-center gap-2 rounded-xl bg-primary-500 px-5 py-3 text-sm font-bold text-white shadow-lg shadow-primary-500/20 transition hover:bg-primary-600 disabled:cursor-not-allowed disabled:opacity-60">
            {saving ? <LoaderCircle className="h-4 w-4 animate-spin" /> : <Save className="h-4 w-4" />}
            {saving ? 'Staging configuration…' : 'Stage network configuration'}
          </button>
        </form>
      </div>

      <div className="rounded-2xl border border-slate-200 bg-white p-5 dark:border-white/[0.08] dark:bg-[#161F30]">
        <div className="flex items-center gap-2 text-slate-800 dark:text-white"><ShieldCheck className="h-5 w-5 text-emerald-500" /><h2 className="font-display font-bold">Current staged configuration</h2></div>
        {config ? (
          <dl className="mt-4 grid grid-cols-1 gap-4 text-sm sm:grid-cols-3">
            <div><dt className="text-xs font-bold uppercase tracking-wider text-slate-400">SSID</dt><dd className="mt-1 font-bold text-slate-800 dark:text-white">{config.ssid}</dd></div>
            <div><dt className="text-xs font-bold uppercase tracking-wider text-slate-400">Status</dt><dd className="mt-1 font-bold text-amber-600 dark:text-amber-300">Pending device apply</dd></div>
            <div><dt className="text-xs font-bold uppercase tracking-wider text-slate-400">Last staged</dt><dd className="mt-1 font-semibold text-slate-700 dark:text-slate-300">{new Date(config.updated_at || config.configured_at).toLocaleString()}</dd></div>
          </dl>
        ) : <p className="mt-3 text-sm text-slate-500 dark:text-slate-400">No network configuration has been staged.</p>}
      </div>
    </div>
  );
}
