import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

export default defineConfig({
  plugins: [vue()],
  server: {
    proxy: {
      '/config.json': {
        target: 'http://localhost:32000',
        rewrite: path => path.replace(/^\/config\.json$/, '/config')
      },
      '/version': {
        target: 'http://localhost:32000',
        rewrite: path => path.replace(/^\/version$/, '/version')
      },
      // New: proxy mock runtime endpoints to json-server keys
      '/runtime.json': {
        target: 'http://localhost:32000',
        // Allow cache-busting query strings (e.g. /runtime.json?ts=123) by not anchoring to end
        rewrite: path => path.replace(/^\/runtime\.json/, '/runtime')
      },
      '/runtime_meta.json': {
        target: 'http://localhost:32000',
        // Same relaxed pattern for metadata endpoint
        rewrite: path => path.replace(/^\/runtime_meta\.json/, '/runtime_meta')
      }
      // NOTE: No WebSocket mock here. If you add a mock ws server on port 32000 providing /ws upgrade,
      // you can enable: '/ws': { target: 'ws://localhost:32000', ws: true }.
    }
  }
});