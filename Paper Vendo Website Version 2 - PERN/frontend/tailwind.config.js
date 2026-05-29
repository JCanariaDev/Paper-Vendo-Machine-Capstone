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
          50: '#f0f9ff',
          100: '#e0f2fe',
          200: '#bae6fd',
          300: '#7dd3fc',
          400: '#38bdf8',
          500: '#0ea5e9', // Sky blue primary
          600: '#0284c7',
          700: '#0369a1',
          800: '#075985',
          900: '#0c4a6e',
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
