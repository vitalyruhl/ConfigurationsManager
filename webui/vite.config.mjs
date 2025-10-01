import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

export default defineConfig({
  plugins: [vue()],
  server: {
    proxy: {
      // Point dev UI directly at local bridge server (deviceBridge.js)
      '/config.json': { target: 'http://localhost:33000' },
      '/version': { target: 'http://localhost:33000' },
      '/runtime.json': { target: 'http://localhost:33000' },
  '/runtime_meta.json': { target: 'http://localhost:33000' },
  '/user_theme.css': { target: 'http://localhost:33000' }
      // NOTE: No WebSocket mock here. If you add a mock ws server on port 32000 providing /ws upgrade,
      // you can enable: '/ws': { target: 'ws://localhost:32000', ws: true }.
    }
  }
});