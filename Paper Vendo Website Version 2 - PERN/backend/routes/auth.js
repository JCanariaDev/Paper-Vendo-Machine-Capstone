import express from 'express';
import jwt from 'jsonwebtoken';
import bcrypt from 'bcryptjs';
import { authenticateToken, authorizeRoles, getJwtSecret } from '../middleware/auth.js';

export function createAuthRouter(supabase) {
  const router = express.Router();

  // LOGIN ENDPOINT
  router.post('/login', async (req, res) => {
    const { username, password } = req.body;

    if (typeof username !== 'string' || typeof password !== 'string' || !username.trim() || !password) {
      return res.status(400).json({ message: 'Username and password are required.' });
    }

    try {
      // Query admins table in Supabase
      const { data: admins, error } = await supabase
        .from('admins')
        .select('id, username, password, role')
        .eq('username', username);

      if (error) {
        throw error;
      }

      if (!admins || admins.length === 0) {
        return res.status(401).json({ message: 'Account not found.' });
      }

      const user = admins[0];

      // Every password, including the seed accounts in Cloud_Paper_Vendo.sql,
      // is bcrypt-hashed.  Do not retain a plaintext fallback.
      const isMatch = await bcrypt.compare(password, user.password);

      if (!isMatch) {
        return res.status(401).json({ message: 'Incorrect password.' });
      }

      // Generate JWT Access Token
      const token = jwt.sign(
        { id: user.id, username: user.username, role: user.role },
        getJwtSecret(),
        { expiresIn: '24h' }
      );

      // Return user data and token
      return res.status(200).json({
        message: 'Login successful',
        token,
        user: {
          id: user.id,
          username: user.username,
          role: user.role
        }
      });
    } catch (error) {
      console.error('Auth Server Error:', error);
      return res.status(500).json({ message: 'Internal server error occurred.' });
    }
  });

  // Account creation is restricted to superadmins. Public self-registration
  // would grant staff access to transaction adjustment actions.
  router.post('/register', authenticateToken, authorizeRoles('superadmin'), async (req, res) => {
    const { username, password, role } = req.body;

    if (typeof username !== 'string' || typeof password !== 'string' || !username.trim() || !password) {
      return res.status(400).json({ message: 'Username and password are required.' });
    }

    const normalizedUsername = String(username).trim();
    const userRole = role || 'staff';
    if (!['staff', 'viewer'].includes(userRole)) {
      return res.status(400).json({ message: 'New accounts may only be staff or viewer.' });
    }
    if (normalizedUsername.length < 3 || normalizedUsername.length > 64) {
      return res.status(400).json({ message: 'Username must be between 3 and 64 characters.' });
    }
    if (password.length < 12) {
      return res.status(400).json({ message: 'Password must be at least 12 characters.' });
    }

    try {
      // Check if username already exists in Supabase
      const { data: existingUser, error: checkError } = await supabase
        .from('admins')
        .select('username')
        .eq('username', normalizedUsername);

      if (checkError) {
        throw checkError;
      }

      if (existingUser && existingUser.length > 0) {
        return res.status(400).json({ message: 'Username is already taken.' });
      }

      // Hash password
      const hashedPassword = await bcrypt.hash(password, 10);

      // Insert new user
      const { data: newUser, error: insertError } = await supabase
        .from('admins')
        .insert([{ username: normalizedUsername, password: hashedPassword, role: userRole }])
        .select('id, username, role');

      if (insertError) {
        throw insertError;
      }

      return res.status(201).json({
        message: 'Account registered successfully.',
        user: {
          id: newUser[0].id,
          username: newUser[0].username,
          role: newUser[0].role
        }
      });
    } catch (error) {
      console.error('Registration Error:', error);
      return res.status(500).json({ message: 'Failed to register account.' });
    }
  });

  // VERIFY CURRENT SESSION MIDDLEWARE / ROUTE
  router.get('/verify', (req, res) => {
    const authHeader = req.headers.authorization;
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
      return res.status(401).json({ valid: false, message: 'No session token provided.' });
    }

    const token = authHeader.split(' ')[1];

    try {
      const decoded = jwt.verify(token, getJwtSecret());
      return res.status(200).json({
        valid: true,
        user: {
          id: decoded.id,
          username: decoded.username,
          role: decoded.role
        }
      });
    } catch (err) {
      return res.status(401).json({ valid: false, message: 'Invalid or expired session token.' });
    }
  });

  return router;
}
