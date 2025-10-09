import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

/** @param {string|undefined|null} v */
const bool = (v) => v === '1' || v === 'true';

export default defineConfig({
  plugins: [vue()],
  define: {
    __ENABLE_WS_PUSH__: bool(process.env.VITE_ENABLE_WS_PUSH ?? '1'),
    // Combined analog sliders flag with back-compat fallbacks
  __ENABLE_RUNTIME_ANALOG_SLIDERS__: bool(process.env.VITE_ENABLE_RUNTIME_ANALOG_SLIDERS ?? '1'),
    __ENABLE_RUNTIME_STATE_BUTTONS__: bool(process.env.VITE_ENABLE_RUNTIME_STATE_BUTTONS ?? '1'),
    __ENABLE_SYSTEM_PROVIDER__: bool(process.env.VITE_ENABLE_SYSTEM_PROVIDER ?? '1'),
  },
  build: {
    cssCodeSplit: false,
    sourcemap: false,
    target: 'es2019',
    minify: 'esbuild'
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