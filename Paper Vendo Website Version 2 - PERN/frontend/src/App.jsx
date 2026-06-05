import React, { useState, useEffect, createContext, useContext } from 'react';
import { BrowserRouter as Router, Routes, Route, Navigate, useLocation } from 'react-router-dom';
import axios from 'axios';

// Configure Axios default base URL for production deployments
axios.defaults.baseURL = import.meta.env.VITE_API_URL || '';

// Import Pages
import Login from './pages/Login';
import Dashboard from './pages/Dashboard';
import Inventory from './pages/Inventory';
import Transactions from './pages/Transactions';
import RealTimeStatus from './pages/RealTimeStatus';

// Import Sidebar Layout
import Sidebar from './components/Sidebar';

// Create Global Auth Context
const AuthContext = createContext(null);

export const useAuth = () => useContext(AuthContext);

// Protected Router Wrapper
const ProtectedRoute = ({ children }) => {
  const { token, loading } = useAuth();
  const location = useLocation();

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
      <Sidebar />
      <main className="flex-1 p-6 md:p-10 ml-64 overflow-x-hidden min-h-screen">
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
            path="/status" 
            element={
              <ProtectedRoute>
                <RealTimeStatus />
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
