import express from 'express';
import jwt from 'jsonwebtoken';
import bcrypt from 'bcryptjs';

export function createAuthRouter(supabase) {
  const router = express.Router();

  // LOGIN ENDPOINT
  router.post('/login', async (req, res) => {
    const { username, password } = req.body;

    if (!username || !password) {
      return res.status(400).json({ message: 'Username and password are required.' });
    }

    try {
      // Query admins table in Supabase
      const { data: admins, error } = await supabase
        .from('admins')
        .select('*')
        .eq('username', username);

      if (error) {
        throw error;
      }

      if (!admins || admins.length === 0) {
        return res.status(401).json({ message: 'Account not found.' });
      }

      const user = admins[0];

      // Password Verification
      let isMatch = false;
      
      // Try to compare as bcrypt hashed password first
      try {
        isMatch = await bcrypt.compare(password, user.password);
      } catch (err) {
        // Not a hashed password or error in hashing
        isMatch = false;
      }

      // Fallback: check plain text password for legacy seeds (e.g. admin123, staff123)
      if (!isMatch && password === user.password) {
        isMatch = true;
      }

      if (!isMatch) {
        return res.status(401).json({ message: 'Incorrect password.' });
      }

      // Generate JWT Access Token
      const token = jwt.sign(
        { id: user.id, username: user.username, role: user.role },
        process.env.JWT_SECRET || 'fallback_secret_key',
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

  // VERIFY CURRENT SESSION MIDDLEWARE / ROUTE
  router.get('/verify', (req, res) => {
    const authHeader = req.headers.authorization;
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
      return res.status(401).json({ valid: false, message: 'No session token provided.' });
    }

    const token = authHeader.split(' ')[1];

    try {
      const decoded = jwt.verify(token, process.env.JWT_SECRET || 'fallback_secret_key');
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
