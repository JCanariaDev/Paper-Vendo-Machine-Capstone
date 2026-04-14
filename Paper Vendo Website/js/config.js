/* 
   config.js - The Cloud Connection
   Project: Paper Vendo Cloud
*/


//Change url and key
const NEW_SUPABASE_URL = "https://jowpzdynbdeznuvohrpx.supabase.co";
const NEW_SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";


// Helper to handle all API requests
async function supabaseRequest(table, method = 'GET', data = null, query = '') {
    const url = `${NEW_SUPABASE_URL}/rest/v1/${table}${query}`;
    const options = {
        method: method,
        headers: {
            "apikey": NEW_SUPABASE_KEY,
            "Authorization": `Bearer ${NEW_SUPABASE_KEY}`,
            "Content-Type": "application/json",
            "Prefer": "return=representation"
        }
    };

    if (data) options.body = JSON.stringify(data);

    try {
        const response = await fetch(url, options);
        if (!response.ok) {
            const errData = await response.json();
            throw new Error(errData.message || "Request failed");
        }
        return await response.json();
    } catch (error) {
        console.error("Cloud Error:", error);
        return { error: true, message: error.message };
    }
}

// Session Management (Replaces PHP Sessions)
const auth = {
    login: (userData) => {
        localStorage.setItem('vendo_user', JSON.stringify(userData));
        window.location.href = 'dashboard.html';
    },
    logout: () => {
        localStorage.removeItem('vendo_user');
        window.location.href = 'index.html';
    },
    check: () => {
        const user = localStorage.getItem('vendo_user');
        if (!user && !window.location.pathname.includes('index.html')) {
            window.location.href = 'index.html';
            return null;
        }

        const parsedUser = user ? JSON.parse(user) : null;

        // Background Security Check: Verify this session hasn't been spoofed
        if (parsedUser && !window.location.pathname.includes('index.html')) {
            supabaseRequest('admins', 'GET', null, `?username=eq.${parsedUser.username}&password=eq.${parsedUser.password}&select=id`)
                .then(result => {
                    // If the database returns 0 matches for this username/password combo, or an error happens
                    if (!result || result.error || result.length === 0) {
                        console.warn("Spoofed or invalid session detected. Logging out automatically.");
                        auth.logout();
                    }
                })
                .catch(err => console.error("Session verification failed", err));
        }

        return parsedUser;
    }
};

// Theme Management
const theme = {
    toggle: () => {
        const current = document.documentElement.getAttribute('data-theme') || 'light';
        const target = current === 'light' ? 'dark' : 'light';
        document.documentElement.setAttribute('data-theme', target);
        localStorage.setItem('vendo_theme', target);
    },
    init: () => {
        const saved = localStorage.getItem('vendo_theme') || 'light';
        document.documentElement.setAttribute('data-theme', saved);
    }
};

// Run on every page load
theme.init();
