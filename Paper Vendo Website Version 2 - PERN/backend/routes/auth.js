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

      // Every password, including the seed accounts in Cloud_Paper_Vendo.sql,
      // is bcrypt-hashed.  Do not retain a plaintext fallback.
      const isMatch = await bcrypt.compare(password, user.password);

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

  // REGISTER ENDPOINT
  router.post('/register', async (req, res) => {
    const { username, password, role } = req.body;

    if (!username || !password) {
      return res.status(400).json({ message: 'Username and password are required.' });
    }

    // Role validation
    const allowedRoles = ['superadmin', 'staff'];
    const userRole = role || 'staff';
    if (!allowedRoles.includes(userRole)) {
      return res.status(400).json({ message: 'Invalid role. Must be either superadmin or staff.' });
    }

    try {
      // Check if username already exists in Supabase
      const { data: existingUser, error: checkError } = await supabase
        .from('admins')
        .select('username')
        .eq('username', username);

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
        .insert([{ username, password: hashedPassword, role: userRole }])
        .select('*');

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
