import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

export default defineConfig({
  plugins: [vue()],
  build: {
    cssCodeSplit: false,
    sourcemap: false,
    target: ['es2018', 'firefox78', 'chrome87', 'safari13'], // Better browser compatibility
    minify: 'esbuild',
    rollupOptions: {
      output: {
        manualChunks: undefined, // Avoid chunk splitting issues
      }
    },
    // Use modern module preload polyfill setting
    modulePreload: { polyfill: true },
  },
  server: {
    proxy: {
      '/config.json': { target: 'http://localhost:33000' },
      '/version': { target: 'http://localhost:33000' },
      '/runtime.json': { target: 'http://localhost:33000' },
      '/runtime_meta.json': { target: 'http://localhost:33000' },
      '/user_theme.css': { target: 'http://localhost:33000' }
    }
  }
});