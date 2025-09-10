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
      }
    }
  }
});