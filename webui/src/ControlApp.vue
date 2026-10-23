<template>
  <main class="control-shell">
    <h1>{{ title }}</h1>
    <nav>
      <button v-if="LIVE_UI" :disabled="busy || !safetyReady" @click="view = 'live'">Live</button>
      <button v-if="LIVE_UI && guiLogging" :disabled="busy || !safetyReady" @click="view = 'log'">Log</button>
      <button :disabled="!canFlash || busy" @click="startFlash">Flash</button>
      <button @click="toggleTheme">Theme: {{ theme }}</button>
    </nav>
    <p v-for="toast in toasts" :key="toast.id" class="notice" :role="toast.type === 'error' ? 'alert' : 'status'">{{ toast.message }}</p>
    <OtaController ref="ota" @can-flash-change="canFlash = $event" @ota-active-change="busy = $event" @capabilities-change="handleCapabilities" />
    <RuntimeDashboard v-if="LIVE_UI && safetyReady" :view="view" :ota-state="state" />
    <GuiEvents v-if="LIVE_UI && safetyReady" :disabled="busy" />
    <div v-if="busy" class="busy-overlay" role="alertdialog" aria-label="OTA update in progress">
      <p>OTA update in progress. Please keep this page open.</p>
    </div>
    <form v-if="authVisible" class="auth-dialog" @submit.prevent="authorizeFlash">
      <h2>OTA Flash Access Required</h2>
      <input v-model="password" type="password" autocomplete="current-password" placeholder="Enter settings password" aria-label="Settings password" />
      <button type="button" @click="authVisible = false; password = ''">Cancel</button>
      <button type="submit">Authenticate</button>
    </form>
  </main>
</template>
<script setup>
import { ref, computed, provide, onMounted, onBeforeUnmount, watch } from 'vue';
import RuntimeDashboard from '#cm-live-dashboard';
import GuiEvents from '#cm-gui-events';
import OtaController from './components/OtaController.vue';
import { readJson } from './jsonHttp.mjs';
import { createAppInfoRecovery } from './appInfoClient.mjs';
import { readTextResponse } from './startupHttp.mjs';
const LIVE_UI = import.meta.env.VITE_CM_ENABLE_LIVE_UI !== '0';
const ota = ref(null), state = ref({}), canFlash = ref(false), busy = ref(false);
const safetyReady = ref(false);
function handleCapabilities(value) {
  state.value = value;
  if (!safetyReady.value) busy.value = value.otaActive === true;
  safetyReady.value = true;
}
const view = ref('live'), guiLogging = ref(false), name = ref('ESP32 Configuration'), version = ref('');
const title = computed(() => `${name.value}${version.value ? ` ${version.value}` : ''}`);
const toasts = ref([]), authVisible = ref(false), password = ref('');
let toastId = 0, authExpires = 0;
const storedTheme = document.cookie.match(/(?:^|; )cm\.theme\.v1=([^;]*)/)?.[1];
const theme = ref(['dark', 'light'].includes(storedTheme) ? storedTheme : window.matchMedia?.('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
document.documentElement.dataset.theme = theme.value;
function toggleTheme() { theme.value = theme.value === 'dark' ? 'light' : 'dark'; document.documentElement.dataset.theme = theme.value; document.cookie = `cm.theme.v1=${theme.value}; path=/; max-age=31536000`; }
function notify(message, type = 'info', duration = 4000, sticky = false) {
  const id = ++toastId;
  toasts.value.push({ id, message, type, sticky });
  if (!sticky && duration) setTimeout(() => dismissToast(id), type === 'error' ? Math.max(duration, 8000) : duration);
  return id;
}
function dismissToast(id) { toasts.value = toasts.value.filter(toast => toast.id !== id); }
provide('notify', notify);
provide('dismissToast', dismissToast);
provide('onOtaStatus', value => ota.value?.applyOtaStatus(value));
provide('updateToast', (id, message, type = 'info', duration = 2500) => {
  const toast = toasts.value.find(value => value.id === id);
  if (!toast) return notify(message, type, duration);
  Object.assign(toast, { message, type, sticky: false });
  setTimeout(() => dismissToast(id), type === 'error' ? Math.max(duration, 8000) : duration);
});
async function authorizeFlash() {
  try {
    const result = await readJson('/config/auth', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ password: password.value }) });
    if (result.status !== 'ok') throw new Error('unauthorized');
    authExpires = Date.now() + Number(result.ttlSec || 0) * 1000;
    authVisible.value = false;
    password.value = '';
    await ota.value?.startFlash();
  } catch { password.value = ''; notify('Invalid password or access unavailable', 'error'); }
}
async function startFlash() {
  if (busy.value || !canFlash.value) return;
  if (state.value.settingsAuthRequired && Date.now() >= authExpires) { authVisible.value = true; return; }
  await ota.value?.startFlash();
}
function preventNavigation(event) { if (busy.value) { event.preventDefault(); event.returnValue = ''; } }
watch(busy, active => { if (active) window.addEventListener('beforeunload', preventNavigation); else window.removeEventListener('beforeunload', preventNavigation); });
const identity = createAppInfoRecovery({ onUpdate(info) { name.value = info.appTitle || info.appName || name.value; version.value = info.version; guiLogging.value = info.guiLogging; document.title = title.value; } });
onMounted(async () => {
  try {
    const response = await readTextResponse('/user_theme.css');
    if (response.status === 200 && response.body.trim()) {
      const style = document.createElement('style'); style.id = 'user-theme-css'; style.textContent = response.body; document.head.appendChild(style);
    }
  } catch { /* An optional stylesheet must not block device controls. */ }
  void identity.run();
});
onBeforeUnmount(() => { identity.stop(); window.removeEventListener('beforeunload', preventNavigation); });
</script>
<style scoped>
.control-shell { max-width: 1200px; margin: auto; padding: 1rem; font-family: sans-serif; color: var(--cm-fg, #222); }
h1 { text-align: center; font-size: 1.8rem; }
nav { display: flex; justify-content: center; gap: .5rem; }
button, input { padding: .6rem 1rem; border-radius: .35rem; border: 1px solid #777; }
button { cursor: pointer; }
button:disabled { opacity: .5; cursor: default; }
.notice { padding: .7rem; border: 1px solid #777; border-radius: .3rem; }
.busy-overlay { position: fixed; inset: 0; background: #000a; display: grid; place-items: center; z-index: 2100; color: white; }
.auth-dialog { position: fixed; inset: 25% auto auto 50%; transform: translateX(-50%); background: var(--cm-card-bg, #eee); color: var(--cm-fg, #222); padding: 1.5rem; border: 1px solid #777; border-radius: .5rem; z-index: 2300; }
.auth-dialog input { display: block; margin-bottom: 1rem; }
</style>
