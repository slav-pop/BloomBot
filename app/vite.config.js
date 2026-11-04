import { fileURLToPath, URL } from 'node:url'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// Pflanzendatenbank liegt im Repo-Root (data/plants.json), siehe Integration.md.
const appDir = fileURLToPath(new URL('.', import.meta.url))
const dataDir = fileURLToPath(new URL('../data', import.meta.url))
const siteJsDir = fileURLToPath(new URL('../site/js', import.meta.url)) // nur für die Tests der Homepage

export default defineConfig({
  // Relative Pfade: die App läuft unter jedem Unterpfad (GitHub Pages: /<repo>/app/).
  base: './',
  plugins: [vue()],
  resolve: {
    alias: { '@data': dataDir },
  },
  // Dev-Server darf nur App und Datenbank ausliefern. Fürs Handy im WLAN: `npm run dev -- --host`.
  server: {
    fs: { allow: [appDir, dataDir, siteJsDir] },
  },
  test: {
    environment: 'happy-dom',
    restoreMocks: true,
    // Auch die Simulation der Homepage (site/js) wird hier mitgetestet.
    include: ['src/**/*.test.js', '../site/js/**/*.test.js'],
    coverage: {
      provider: 'v8',
      allowExternal: true,
      include: ['src/**/*.{js,vue}', '../site/js/*.js'],
      exclude: ['src/main.js'],
      thresholds: { lines: 80, functions: 80, branches: 80, statements: 80 },
    },
  },
})
