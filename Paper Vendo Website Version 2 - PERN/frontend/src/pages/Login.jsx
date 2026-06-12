import React, { useState } from 'react';
import axios from 'axios';
import { useAuth } from '../App';
import { KeyRound, ShieldAlert, CheckCircle, Eye, EyeOff, Lock, User, UserPlus } from 'lucide-react';

export default function Login() {
  const { login } = useAuth();
  const [isRegister, setIsRegister] = useState(false);
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [role, setRole] = useState('staff'); // default to 'staff'
  const [showPassword, setShowPassword] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const [loading, setLoading] = useState(false);

  const toggleMode = () => {
    setIsRegister(!isRegister);
    setError('');
    setSuccess('');
    setUsername('');
    setPassword('');
    setConfirmPassword('');
    setRole('staff');
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError('');
    setSuccess('');

    // Sign Up validation
    if (isRegister) {
      if (password !== confirmPassword) {
        setError('Passwords do not match.');
        return;
      }
      if (password.length < 6) {
        setError('Password must be at least 6 characters.');
        return;
      }
    }

    setLoading(true);

    try {
      if (isRegister) {
        // Register API call
        const response = await axios.post('/api/auth/register', { username, password, role });
        setSuccess(response.data.message || 'Registration successful! You can now log in.');
        // Reset forms and toggle to login mode after 2 seconds
        setTimeout(() => {
          setIsRegister(false);
          setSuccess('');
          setPassword('');
          setConfirmPassword('');
        }, 2500);
      } else {
        // Login API call
        const response = await axios.post('/api/auth/login', { username, password });
        login(response.data.token, response.data.user);
      }
    } catch (err) {
      console.error('Authentication error:', err);
      setError(err.response?.data?.message || 'Authentication failed. Please verify connection.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="relative flex min-h-screen w-screen items-center justify-center p-4 bg-slate-50 overflow-hidden font-sans">
      {/* Premium Dynamic Background Elements */}
      <div className="absolute top-[-20%] left-[-20%] w-[600px] h-[600px] rounded-full bg-gradient-to-br from-primary-500/10 to-indigo-500/5 blur-[130px] pointer-events-none animate-pulse duration-5000"></div>
      <div className="absolute bottom-[-20%] right-[-20%] w-[600px] h-[600px] rounded-full bg-gradient-to-br from-emerald-500/10 to-teal-500/5 blur-[130px] pointer-events-none animate-pulse duration-7000"></div>
      <div className="absolute top-[30%] left-[40%] w-[300px] h-[300px] rounded-full bg-primary-500/5 blur-[90px] pointer-events-none"></div>

      {/* Grid Backdrop effect */}
      <div className="absolute inset-0 bg-[linear-gradient(rgba(15,23,42,0.015)_1px,transparent_1px),linear-gradient(90deg,rgba(15,23,42,0.015)_1px,transparent_1px)] bg-[size:32px_32px] pointer-events-none opacity-80"></div>

      {/* Light Glass Card */}
      <div className="w-full max-w-[440px] rounded-[32px] p-8 md:p-10 border border-slate-200/80 bg-white/90 backdrop-blur-md shadow-[0_20px_50px_rgba(15,23,42,0.06)] relative z-10 animate-[fadeIn_0.5s_ease-out]">
        
        {/* Brand Header */}
        <div className="flex flex-col items-center text-center mb-8">
          <div className="flex items-center justify-center w-16 h-16 rounded-2xl bg-gradient-to-br from-primary-400 to-primary-600 text-white shadow-xl shadow-primary-500/20 mb-4 transition-all duration-300 hover:scale-105">
            {isRegister ? <UserPlus className="w-8 h-8" /> : <KeyRound className="w-8 h-8" />}
          </div>
          <h1 className="font-display font-extrabold text-2xl text-slate-800 tracking-tight">
            {isRegister ? 'Create Account' : 'Welcome Back'}
          </h1>
          <p className="text-sm text-slate-500 mt-1">
            {isRegister ? 'Register as superadmin or staff' : 'Sign in to your Cloud Vendo Panel'}
          </p>
        </div>

        {/* Error Alert */}
        {error && (
          <div className="flex items-start gap-2.5 p-3.5 mb-6 rounded-xl border border-red-200 bg-red-50 text-red-600 text-xs font-semibold animate-[slideDown_0.2s_ease-out]">
            <ShieldAlert className="w-4 h-4 shrink-0 mt-0.5" />
            <span>{error}</span>
          </div>
        )}

        {/* Success Alert */}
        {success && (
          <div className="flex items-start gap-2.5 p-3.5 mb-6 rounded-xl border border-emerald-200 bg-emerald-50 text-emerald-600 text-xs font-semibold animate-[slideDown_0.2s_ease-out]">
            <CheckCircle className="w-4 h-4 shrink-0 mt-0.5" />
            <span>{success}</span>
          </div>
        )}

        {/* Auth Form */}
        <form onSubmit={handleSubmit} className="space-y-5">
          {/* Username Input */}
          <div className="space-y-1.5">
            <label className="block text-[11px] font-bold text-slate-500 uppercase tracking-wider">Username</label>
            <div className="relative">
              <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400">
                <User className="w-4.5 h-4.5" />
              </span>
              <input
                type="text"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                placeholder="Enter Username"
                className="w-full h-12 pl-11 pr-4 rounded-xl text-sm bg-slate-50/70 border border-slate-200 text-slate-800 placeholder-slate-400 focus:bg-white focus:border-primary-500 focus:ring-4 focus:ring-primary-500/10 outline-none transition-all duration-200"
                required
              />
            </div>
          </div>

          {/* Role Dropdown - Displayed ONLY in Register mode */}
          {isRegister && (
            <div className="space-y-1.5 animate-[fadeIn_0.2s_ease-out]">
              <label className="block text-[11px] font-bold text-slate-500 uppercase tracking-wider">System Role</label>
              <select
                value={role}
                onChange={(e) => setRole(e.target.value)}
                className="w-full h-12 px-3.5 rounded-xl text-sm bg-slate-50/70 border border-slate-200 text-slate-700 focus:bg-white focus:border-primary-500 focus:ring-4 focus:ring-primary-500/10 outline-none transition-all duration-200"
              >
                <option value="staff">Staff (View inventory & statistics)</option>
                <option value="superadmin">Superadmin (Full settings control)</option>
              </select>
            </div>
          )}

          {/* Password Input */}
          <div className="space-y-1.5">
            <label className="block text-[11px] font-bold text-slate-500 uppercase tracking-wider">Password</label>
            <div className="relative">
              <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400">
                <Lock className="w-4.5 h-4.5" />
              </span>
              <input
                type={showPassword ? 'text' : 'password'}
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="Enter Password"
                className="w-full h-12 pl-11 pr-11 rounded-xl text-sm bg-slate-50/70 border border-slate-200 text-slate-800 placeholder-slate-400 focus:bg-white focus:border-primary-500 focus:ring-4 focus:ring-primary-500/10 outline-none transition-all duration-200"
                required
              />
              <button
                type="button"
                onClick={() => setShowPassword(!showPassword)}
                className="absolute right-3.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-slate-600 transition-colors"
              >
                {showPassword ? <EyeOff className="w-4.5 h-4.5" /> : <Eye className="w-4.5 h-4.5" />}
              </button>
            </div>
          </div>

          {/* Confirm Password Input - Displayed ONLY in Register mode */}
          {isRegister && (
            <div className="space-y-1.5 animate-[fadeIn_0.2s_ease-out]">
              <label className="block text-[11px] font-bold text-slate-500 uppercase tracking-wider">Confirm Password</label>
              <div className="relative">
                <span className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-400">
                  <Lock className="w-4.5 h-4.5" />
                </span>
                <input
                  type={showPassword ? 'text' : 'password'}
                  value={confirmPassword}
                  onChange={(e) => setConfirmPassword(e.target.value)}
                  placeholder="Re-enter Password"
                  className="w-full h-12 pl-11 pr-11 rounded-xl text-sm bg-slate-50/70 border border-slate-200 text-slate-800 placeholder-slate-400 focus:bg-white focus:border-primary-500 focus:ring-4 focus:ring-primary-500/10 outline-none transition-all duration-200"
                  required
                />
              </div>
            </div>
          )}

          {/* Submit Button */}
          <button
            type="submit"
            disabled={loading}
            className="w-full h-12 rounded-xl bg-gradient-to-r from-primary-500 to-indigo-500 hover:from-primary-600 hover:to-indigo-600 disabled:opacity-75 font-semibold text-sm text-white shadow-lg shadow-primary-500/20 flex items-center justify-center gap-2 transition-all duration-200 active:scale-[0.98] mt-8"
          >
            {loading ? (
              <div className="h-5 w-5 animate-spin rounded-full border-2 border-white/30 border-t-white"></div>
            ) : (
              <span>{isRegister ? 'Create Account' : 'Sign In'}</span>
            )}
          </button>
        </form>

        {/* Mode Toggle Link */}
        <div className="mt-8 text-center border-t border-slate-100 pt-6">
          <button
            type="button"
            onClick={toggleMode}
            className="text-xs text-slate-500 hover:text-primary-600 transition-colors font-medium outline-none"
          >
            {isRegister 
              ? 'Already have an account? Sign In' 
              : "Don't have an account? Sign Up"}
          </button>
        </div>
      </div>
    </div>
  );
}
