import { defineConfig, loadEnv } from 'vite';
import vue from '@vitejs/plugin-vue';
import { fileURLToPath } from 'node:url';

export default defineConfig(({ mode }) => {
  const env = { ...loadEnv(mode, process.cwd(), ''), ...process.env };
  const live = env.VITE_CM_ENABLE_LIVE_UI !== '0';
  const settings = env.VITE_CM_ENABLE_SETTINGS_UI !== '0';
  const source = path => fileURLToPath(new URL(path, import.meta.url));
  return {
  resolve: { alias: {
    '#cm-app': source(settings ? './src/App.vue' : './src/ControlApp.vue'),
    '#cm-live-dashboard': source(live ? './src/components/RuntimeDashboard.vue' : './src/components/DisabledUi.vue'),
    '#cm-live-client': source(live ? './src/runtimeDataClient.mjs' : './src/disabledLive.mjs'),
    '#cm-controls-styles': source(live || settings ? './src/assets/styles/runtime-controls.css' : './src/assets/styles/disabled-ui.css'),
    '#cm-gui-events': source(live ? './src/components/GuiEvents.vue' : './src/components/DisabledUi.vue'),
  } },
  plugins: [vue()],
  build: {
    cssCodeSplit: false,
    sourcemap: false,
    target: 'esnext',
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
}; });
