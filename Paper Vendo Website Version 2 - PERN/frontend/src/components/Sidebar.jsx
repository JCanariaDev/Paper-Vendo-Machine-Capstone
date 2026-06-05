import React from 'react';
import { NavLink } from 'react-router-dom';
import { useAuth } from '../App';
import { 
  LayoutDashboard, 
  Boxes, 
  History, 
  LogOut, 
  Sun, 
  Moon, 
  User,
  Activity
} from 'lucide-react';

export default function Sidebar() {
  const { user, logout, theme, toggleTheme } = useAuth();

  const navItems = [
    { to: '/dashboard', label: 'Dashboard', icon: LayoutDashboard },
    { to: '/status', label: 'Real Time Status', icon: Activity },
    { to: '/inventory', label: 'Inventory', icon: Boxes },
    { to: '/transactions', label: 'Sales History', icon: History },
  ];


  return (
    <aside className="fixed top-0 left-0 h-screen w-64 flex flex-col z-20 transition-all duration-300 bg-white border-r border-slate-200 dark:bg-[#0D1526] dark:border-white/[0.06]">
      {/* Brand Header */}
      <div className="flex items-center gap-3 p-6 border-b border-slate-200 dark:border-white/[0.06]">
        <div className="flex items-center justify-center w-10 h-10 rounded-xl bg-primary-500 text-white font-display font-extrabold text-xl shadow-md shadow-primary-500/20">
          V
        </div>
        <div>
          <h1 className="font-display font-bold text-lg text-slate-800 dark:text-white leading-none">Paper Vendo</h1>
          <span className="text-[11px] font-semibold tracking-wider text-primary-500 uppercase">Cloud Panel</span>
        </div>
      </div>

      {/* Navigation Links */}
      <nav className="flex-1 px-4 py-6 space-y-2">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            className={({ isActive }) => `
              flex items-center gap-3.5 px-4 py-3 rounded-xl font-medium text-sm transition-all duration-200
              ${isActive 
                ? 'bg-primary-500 text-white shadow-lg shadow-primary-500/10' 
                : 'text-slate-600 dark:text-slate-400 hover:bg-slate-100 dark:hover:bg-white/[0.03] hover:text-slate-800 dark:hover:text-white'
              }
            `}
          >
            <item.icon className="w-5 h-5" />
            <span>{item.label}</span>
          </NavLink>
        ))}
      </nav>

      {/* Footer Controls */}
      <div className="p-4 border-t border-slate-200 dark:border-white/[0.06] space-y-4">
        {/* User Card */}
        <div className="flex items-center gap-3 p-3 rounded-xl bg-slate-50 dark:bg-white/[0.02] border border-slate-150 dark:border-white/[0.04]">
          <div className="flex items-center justify-center w-9 h-9 rounded-lg bg-slate-200 dark:bg-white/10 text-slate-600 dark:text-slate-300">
            <User className="w-5 h-5" />
          </div>
          <div className="overflow-hidden">
            <h4 className="text-xs font-bold text-slate-800 dark:text-white truncate">@{user?.username}</h4>
            <span className="text-[10px] font-semibold text-slate-400 uppercase tracking-wide">{user?.role}</span>
          </div>
        </div>

        {/* Action Controls */}
        <div className="flex items-center justify-between gap-2 px-1">
          {/* Light/Dark Toggle */}
          <button
            onClick={toggleTheme}
            className="flex items-center justify-center w-10 h-10 rounded-xl text-slate-600 dark:text-slate-400 hover:bg-slate-100 dark:hover:bg-white/[0.04] transition-colors"
            title={theme === 'dark' ? 'Switch to Light Mode' : 'Switch to Dark Mode'}
          >
            {theme === 'dark' ? <Sun className="w-5 h-5 text-amber-400" /> : <Moon className="w-5 h-5" />}
          </button>

          {/* Logout */}
          <button
            onClick={logout}
            className="flex items-center gap-2 px-4 py-2.5 rounded-xl font-semibold text-sm text-red-500 hover:bg-red-50 dark:hover:bg-red-500/10 transition-colors"
          >
            <LogOut className="w-4 h-4" />
            <span>Log Out</span>
          </button>
        </div>
      </div>
    </aside>
  );
}
