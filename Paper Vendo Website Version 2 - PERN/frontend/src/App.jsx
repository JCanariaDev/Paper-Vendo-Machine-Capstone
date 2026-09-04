import React, { useState, useEffect, createContext, useContext } from 'react';
import { BrowserRouter as Router, Routes, Route, Navigate, useLocation } from 'react-router-dom';
import axios from 'axios';

// Configure Axios default base URL for production deployments
axios.defaults.baseURL = import.meta.env.VITE_API_URL || '';

// Import Pages
import Login from './pages/Login';
import Dashboard from './pages/Dashboard';
import MachineMonitor from './pages/MachineMonitor';
import Inventory from './pages/Inventory';
import Transactions from './pages/Transactions';
import Analytics from './pages/Analytics';
import Reports from './pages/Reports';

// Import Sidebar Layout
import Sidebar from './components/Sidebar';

// Create Global Auth Context
const AuthContext = createContext(null);

export const useAuth = () => useContext(AuthContext);

// Protected Router Wrapper
const ProtectedRoute = ({ children }) => {
  const { token, loading } = useAuth();
  const location = useLocation();
  const [sidebarOpen, setSidebarOpen] = useState(false);

  if (loading) {
    return (
      <div className="flex h-screen w-screen items-center justify-center bg-lightBg-base dark:bg-darkBg-base">
        <div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500"></div>
      </div>
    );
  }

  if (!token) {
    // Redirect them to login but save the current location they tried to access
    return <Navigate to="/login" state={{ from: location }} replace />;
  }

  return (
    <div className="flex min-h-screen bg-lightBg-base dark:bg-darkBg-base text-slate-800 dark:text-slate-100 transition-colors duration-300">
      {/* Mobile Top Navbar */}
      <div className="fixed top-0 left-0 right-0 h-16 flex items-center justify-between px-4 bg-white border-b border-slate-200 dark:bg-[#0D1526] dark:border-white/[0.06] z-30 md:hidden">
        <div className="flex items-center gap-3">
          <button
            onClick={() => setSidebarOpen(true)}
            className="p-2 rounded-xl text-slate-600 dark:text-slate-300 hover:bg-slate-100 dark:hover:bg-white/[0.04] transition-colors"
          >
            <svg className="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M4 6h16M4 12h16M4 18h16" />
            </svg>
          </button>
          <div className="flex items-center gap-2.5">
            <div className="flex items-center justify-center w-9 h-9 rounded-xl bg-white border border-slate-200 dark:border-white/[0.08] p-1.5 shadow-sm">
              <img src="/logo.png" alt="P&B V Machine Logo" className="w-full h-full object-contain" />
            </div>
            <div>
              <h1 className="font-display font-bold text-sm text-slate-800 dark:text-white leading-none">Paper Vendo</h1>
              <span className="text-[9px] font-semibold tracking-wider text-primary-500 uppercase">Cloud Panel</span>
            </div>
          </div>
        </div>
      </div>

      <Sidebar isOpen={sidebarOpen} onClose={() => setSidebarOpen(false)} />
      <main className="flex-1 p-4 md:p-10 pt-20 md:pt-10 ml-0 md:ml-64 overflow-x-hidden min-h-screen">
        {children}
      </main>
    </div>
  );
};

export default function App() {
  const [token, setToken] = useState(localStorage.getItem('vendo_token'));
  const [user, setUser] = useState(JSON.parse(localStorage.getItem('vendo_user')));
  const [loading, setLoading] = useState(true);
  const [theme, setTheme] = useState(localStorage.getItem('vendo_theme') || 'dark');
    

  // Sync state and Axios authorization headers
  useEffect(() => {
    if (token) {
      axios.defaults.headers.common['Authorization'] = `Bearer ${token}`;
      
      // Background verification of token
      axios.get('/api/auth/verify')
        .then(res => {
          if (res.data.valid) {
            setUser(res.data.user);
            localStorage.setItem('vendo_user', JSON.stringify(res.data.user));
          }
          setLoading(false);
        })
        .catch(err => {
          console.warn('Session expired. Logging out.');
          logout();
          setLoading(false);
        });
    } else {
      delete axios.defaults.headers.common['Authorization'];
      setLoading(false);
    }
  }, [token]);

  // Sync theme changes
  useEffect(() => {
    const root = window.document.documentElement;
    if (theme === 'dark') {
      root.classList.add('dark');
      root.setAttribute('data-theme', 'dark');
    } else {
      root.classList.remove('dark');
      root.setAttribute('data-theme', 'light');
    }
    localStorage.setItem('vendo_theme', theme);
  }, [theme]);

  const login = (newToken, newUser) => {
    localStorage.setItem('vendo_token', newToken);
    localStorage.setItem('vendo_user', JSON.stringify(newUser));
    setToken(newToken);
    setUser(newUser);
  };

  const logout = () => {
    localStorage.removeItem('vendo_token');
    localStorage.removeItem('vendo_user');
    setToken(null);
    setUser(null);
  };

  const toggleTheme = () => {
    setTheme(prev => (prev === 'dark' ? 'light' : 'dark'));
  };

  // Admin Router Wrapper (Superadmin only)
  const AdminRoute = ({ children }) => {
    const { user } = useAuth();
    if (user && user.role !== 'superadmin') {
      return <Navigate to="/dashboard" replace />;
    }
    return children;
  };

  return (
    <AuthContext.Provider value={{ token, user, loading, login, logout, theme, toggleTheme }}>
      <Router>
        <Routes>
          {/* Public Authentication Gate */}
          <Route 
            path="/login" 
            element={token ? <Navigate to="/dashboard" replace /> : <Login />} 
          />

          {/* Secure Admin Pages */}
          <Route 
            path="/dashboard" 
            element={
              <ProtectedRoute>
                <Dashboard />
              </ProtectedRoute>
            } 
          />
          <Route 
            path="/monitor" 
            element={
              <ProtectedRoute>
                <MachineMonitor />
              </ProtectedRoute>
            } 
          />
          <Route 
            path="/inventory" 
            element={
              <ProtectedRoute>
                <Inventory />
              </ProtectedRoute>
            } 
          />
          <Route 
            path="/transactions" 
            element={
              <ProtectedRoute>
                <Transactions />
              </ProtectedRoute>
            } 
          />
          <Route 
            path="/analytics" 
            element={
              <ProtectedRoute>
                <AdminRoute>
                  <Analytics />
                </AdminRoute>
              </ProtectedRoute>
            } 
          />
          <Route 
            path="/reports" 
            element={
              <ProtectedRoute>
                <AdminRoute>
                  <Reports />
                </AdminRoute>
              </ProtectedRoute>
            } 
          />

          {/* Root Wildcard Redirection */}
          <Route 
            path="*" 
            element={<Navigate to={token ? "/dashboard" : "/login"} replace />} 
          />
        </Routes>
      </Router>
    </AuthContext.Provider>
  );
}
