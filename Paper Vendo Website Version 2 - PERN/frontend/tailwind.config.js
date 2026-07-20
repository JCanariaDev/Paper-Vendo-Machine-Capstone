/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  darkMode: 'class', // support class-based dark mode
  theme: {
    extend: {
      colors: {
        primary: {
          50: '#f3faf7',
          100: '#e2f4ed',
          200: '#c1e7db',
          300: '#96d4c1',
          400: '#62baa0',
          500: '#3d997a', // Matcha / Teal primary
          600: '#2e7b61',
          700: '#266350',
          800: '#205041',
          900: '#1b4337',
        },
        darkBg: {
          base: '#0B0F19', // Deep dark slate background
          card: '#161F30', // Inner card slate
          border: 'rgba(255, 255, 255, 0.08)',
          input: '#1F293D',
        },
        lightBg: {
          base: '#F8FAFC', // Crisp clean gray-white
          card: '#FFFFFF',
          border: 'rgba(0, 0, 0, 0.08)',
          input: '#F1F5F9',
        }
      },
      fontFamily: {
        sans: ['Inter', 'sans-serif'],
        display: ['Outfit', 'sans-serif'],
      },
      boxShadow: {
        glass: '0 8px 32px 0 rgba(0, 0, 0, 0.37)',
        glassLight: '0 8px 32px 0 rgba(15, 23, 42, 0.05)',
      }
    },
  },
  plugins: [],
}
