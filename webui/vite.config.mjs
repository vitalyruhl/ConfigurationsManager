import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

export default defineConfig({
  plugins: [vue()],
  server: {
    proxy: {
      '/config.json': 'http://localhost:3000/config',
      '/version': 'http://localhost:3000/version'
    }
  }
});