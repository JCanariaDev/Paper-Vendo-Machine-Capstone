import express from 'express';
import cors from 'cors';
import dotenv from 'dotenv';
import { createClient } from '@supabase/supabase-js';
import { createAuthRouter } from './routes/auth.js';
import { createMachineRouter } from './routes/machine.js';

// Load environmental variables
dotenv.config();

const app = express();
const PORT = process.env.PORT || 5000;

// Enable CORS
app.use(cors({
  origin: '*', // Allow all origins for easy development, can restrict to Vercel domain later
  methods: ['GET', 'POST', 'PUT', 'DELETE', 'PATCH', 'OPTIONS'],
  allowedHeaders: ['Content-Type', 'Authorization', 'apikey']
}));

// Body parser
app.use(express.json());

// Initialize Supabase Client
const supabaseUrl = process.env.SUPABASE_URL;
const supabaseKey = process.env.SUPABASE_KEY;

if (!supabaseUrl || !supabaseKey) {
  console.error('CRITICAL: Supabase environmental credentials are missing in the .env configuration!');
  process.exit(1);
}

const supabase = createClient(supabaseUrl, supabaseKey);
console.log('>>> Connected securely to Supabase Database Client');

// Health Check Endpoints (Supports Render default and custom checks)
app.get(['/', '/health', '/api/health'], (req, res) => {
  res.status(200).json({ status: 'healthy', timestamp: new Date() });
});

// Setup Routers
app.use('/api/auth', createAuthRouter(supabase));
app.use('/api/machine', createMachineRouter(supabase));

// Global Error Handler
app.use((err, req, res, next) => {
  console.error('Express Server Error Catch:', err);
  res.status(500).json({ message: 'A server error occurred.' });
});

// Bind to 0.0.0.0 for Render / containerized deployment
app.listen(PORT, '0.0.0.0', () => {
  console.log(`========================================`);
  console.log(` PAPER VENDO SERVER ONLINE ON PORT ${PORT}`);
  console.log(` Listening on host: 0.0.0.0:${PORT}`);
  console.log(`========================================`);
});
