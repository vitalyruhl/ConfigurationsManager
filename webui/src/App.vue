<template>
  <div>
    <h1 id="mainHeader" class="ta-center">{{ version }}</h1>
    <div class="tabs">
      <button :class="{active: activeTab==='live'}" @click="activeTab='live'">Live</button>
      <button :class="{active: activeTab==='settings'}" @click="switchToSettings()">Settings</button>
      <button class="flash-btn" @click="startFlash" :disabled="!canFlash">Flash</button>
    </div>
    <input ref="otaFileInput" type="file" accept=".bin,.bin.gz" class="hidden-file-input" @change="onFlashFileSelected" />

    <!-- Toast notifications -->
    <div class="toast-wrapper">
      <div v-for="t in toasts" :key="t.id" class="toast" :class="t.type">
        <span class="toast-msg">{{ t.message }}</span>
        <button class="toast-close" @click="dismissToast(t.id)" aria-label="Close">✕</button>
      </div>
    </div>

    <section v-if="activeTab==='live'" class="live-view">
      <div class="live-cards">
        <div class="card" v-for="group in runtimeGroups" :key="group.name">
          <h3>{{ group.title }}</h3>

          <template v-for="f in group.fields" :key="f.key">
            <!-- Divider rendering -->
            <hr v-if="f.isDivider" class="rt-divider" :data-label="f.label" />
            <!-- Pure string (static or dynamic) -->
            <p v-else-if="f.isString" class="rt-string">
              <strong>{{ f.label }}</strong>
              <span v-if="f.staticValue">: {{ f.staticValue }}</span>
              <span v-else-if="runtime[group.name] && runtime[group.name][f.key] !== undefined">: {{ runtime[group.name][f.key] }}</span>
            </p>
            <!-- Boolean / Numeric -->
            <p v-else-if="runtime[group.name] && runtime[group.name][f.key] !== undefined"
               :class="valueClasses(runtime[group.name][f.key], f)">
              <template v-if="f.isBool">
                <span class="bool-dot" :class="boolDotClass(runtime[group.name][f.key], f)"></span>
                {{ f.label }}
              </template>
              <template v-else>
                {{ f.label }}: {{ formatValue(runtime[group.name][f.key], f) }}<span v-if="f.unit"> {{ f.unit }}</span>
              </template>
            </p>
          </template>
          <hr v-if="group.name==='system' && runtime.uptime !== undefined" class="rt-divider,uptime" />
          <p v-if="group.name==='system' && runtime.uptime !== undefined" class="uptime">Uptime: {{ Math.floor((runtime.uptime||0)/1000) }} s</p>
        </div>
      </div>
      <div class="live-status">Mode: {{ wsConnected? 'WebSocket' : 'Polling' }}</div>
    </section>

    <section v-else class="settings-view">
      <div id="settingsContainer" :key="refreshKey">
        <Category
          v-for="(settings, category) in config"
          :key="category + '_' + refreshKey"
          :category="category"
          :settings="settings"
          :busy-map="opBusy"
          @apply-single="applySingle"
          @save-single="saveSingle"
        />
      </div>
      <div class="action-buttons">
        <button @click="applyAll" class="apply-btn" :disabled="refreshing">{{ refreshing ? '...' : 'Apply All' }}</button>
        <button @click="saveAll" class="save-btn" :disabled="refreshing">{{ refreshing ? '...' : 'Save All' }}</button>
        <button @click="resetDefaults" class="reset-btn" :disabled="refreshing">Reset Defaults</button>
        <button @click="rebootDevice" class="reset-btn" :disabled="refreshing">Reboot</button>
      </div>
    </section>
  </div>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount, computed } from 'vue';
import Category from './components/Category.vue';

const config = ref({});
const refreshKey = ref(0); // forces re-render of settings tree
const version = ref('');
const refreshing = ref(false);
const activeTab = ref('live');
const flashing = ref(false);
const otaFileInput = ref(null);

// Toast notifications
const toasts = ref([]); // {id, message, type, sticky, ts}
let toastCounter = 0;
const opBusy = ref({}); // per-setting busy map

// Runtime values state
const runtime = ref({});
let pollTimer = null;
let ws = null;
const wsConnected = ref(false);
// Runtime metadata state for dynamic grouping
const runtimeMeta = ref([]); // raw metadata array from /runtime_meta.json
const runtimeGroups = ref([]); // transformed groups -> [{ name, title, fields:[{key,label,unit,precision}]}]

const canFlash = computed(() => {
  const systemConfig = config.value?.System;
  let allow = false;
  if (systemConfig && systemConfig.OTAEn && typeof systemConfig.OTAEn.value !== 'undefined') {
    allow = !!systemConfig.OTAEn.value;
  } else if (runtime.value.system && typeof runtime.value.system.allowOTA !== 'undefined') {
    allow = !!runtime.value.system.allowOTA;
  }
  return allow && !flashing.value;
});

function notify(message, type='info', duration=3000, sticky=false, id=null){
  if(id==null) id = ++toastCounter;
  const existing = toasts.value.find(t=>t.id===id);
  if(existing){ existing.message = message; existing.type = type; existing.sticky = sticky; existing.ts = Date.now(); }
  else toasts.value.push({ id, message, type, sticky, ts: Date.now() });
  if(!sticky){ setTimeout(()=>dismissToast(id), duration); }
  return id;
}
function dismissToast(id){ toasts.value = toasts.value.filter(t=>t.id!==id); }
function updateToast(id, message, type='info', duration=2500){
  const t = toasts.value.find(t=>t.id===id);
  if(!t) return notify(message, type, duration);
  t.message = message; t.type = type; t.sticky = false; setTimeout(()=>dismissToast(id), duration);
}

async function loadSettings() {
  try {
    const response = await fetch('/config.json');
    if (!response.ok) throw new Error('HTTP Error');
    const data = await response.json();
    config.value = data.config || data;
    refreshKey.value++; // force re-mount
  } catch (error) {
    notify('Fehler: ' + error.message, 'error');
  }
}

function switchToSettings(){
  activeTab.value = 'settings';
  loadSettings();
}

async function injectVersion() {
  try {
    const response = await fetch('/version');
    if (!response.ok) throw new Error('Version fetch failed');
    const data = await response.text();
    version.value = '' + data;
  } catch (e) {
    version.value = '';
  }
}

async function applySingle(category, key, value) {
  const opKey = `${category}.${key}`;
  opBusy.value[opKey] = true;
  const toastId = notify(`Applying: ${opKey} ...`, 'info', 7000, true);
  try {
    const response = await fetch(`/config/apply?category=${encodeURIComponent(category)}&key=${encodeURIComponent(key)}`,{ method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ value }) });
    const json = await response.json().catch(()=>({}));
    if(!response.ok || json.status!=='ok') throw new Error(json.reason || 'Failed');
    await loadSettings();
    updateToast(toastId, `Applied: ${opKey}`, 'success');
  } catch(e){
    updateToast(toastId, `Apply failed ${opKey}: ${e.message}`, 'error');
  } finally { delete opBusy.value[opKey]; }
}

async function saveSingle(category, key, value) {
  const opKey = `${category}.${key}`;
  opBusy.value[opKey] = true;
  const toastId = notify(`Saving: ${opKey} ...`, 'info', 7000, true);
  try {
    const response = await fetch(`/config/save?category=${encodeURIComponent(category)}&key=${encodeURIComponent(key)}`,{ method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ value }) });
    const json = await response.json().catch(()=>({}));
    if(!response.ok || json.status!=='ok') throw new Error(json.reason || 'Failed');
    await loadSettings();
    updateToast(toastId, `Saved: ${opKey}`, 'success');
  } catch(e){
    updateToast(toastId, `Save failed ${opKey}: ${e.message}`, 'error');
  } finally { delete opBusy.value[opKey]; }
}

async function saveAll() {
  if (!confirm('Save all settings?')) return;
  const all = {};
  document.querySelectorAll('input').forEach(input => {
    const [category, key] = input.name.split('.');
    if (!all[category]) all[category] = {};
    if (input.dataset.isPassword === '1' && (!input.value || input.value === '***')) return;
    all[category][key] = input.type === 'checkbox' ? input.checked : input.value;
  });
  try {
    const response = await fetch('/config/save_all', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(all)
    });
    const json = await response.json().catch(()=>({}));
    if(response.ok && json.status==='ok'){
      notify('All settings saved', 'success');
      await loadSettings();
    } else {
      notify('Failed to save all settings', 'error');
    }
  } catch (error) { notify('Error: ' + error.message, 'error'); }
}

async function applyAll() {
  if (!confirm('Apply all settings?')) return;
  const all = {};
  document.querySelectorAll('input').forEach(input => {
    const [category, key] = input.name.split('.');
    if (!all[category]) all[category] = {};
    all[category][key] = input.type === 'checkbox' ? input.checked : input.value;
  });
  try {
    const response = await fetch('/config/apply_all', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(all)
    });
    const json = await response.json().catch(()=>({}));
    if(response.ok && json.status==='ok'){
      notify('All settings applied', 'success');
      await loadSettings();
    } else {
      notify('Failed to apply all settings', 'error');
    }
  } catch (error) { notify('Error: ' + error.message, 'error'); }
}

async function resetDefaults() {
  if (!confirm('Reset to defaults?')) return;
  try {
    const response = await fetch('/config/reset', { method: 'POST' });
    if (response.ok) {
      notify('Reset to defaults', 'success');
      setTimeout(() => location.reload(), 1000);
    }
  } catch (error) { notify('Error: ' + error.message, 'error'); }
}

function rebootDevice() {
  if (!confirm('Reboot device?')) return;
  fetch('/config/reboot', { method: 'POST' })
    .then(response => response.ok ? notify('Rebooting...', 'info') : null)
    .catch(error => notify('Error: ' + error.message, 'error'));
}

function startFlash() {
  if (!canFlash.value) {
    notify('OTA updates are disabled in device settings.', 'error');
    return;
  }
  if (!otaFileInput.value) {
    notify('Browser file input not ready.', 'error');
    return;
  }
  otaFileInput.value.value = '';
  otaFileInput.value.click();
}

async function onFlashFileSelected(event) {
  const files = event.target?.files;
  if (!files || !files.length) {
    return;
  }
  const file = files[0];
  if (!file) {
    return;
  }
  if (!canFlash.value) {
    notify('OTA updates are disabled in device settings.', 'error');
    return;
  }
  if (file.size === 0) {
    notify('Selected file is empty.', 'error');
    otaFileInput.value.value = '';
    return;
  }

  let password = window.prompt('Enter OTA password', '');
  if (password === null) {
    otaFileInput.value.value = '';
    return; // user cancelled
  }
  password = password.trim();

  const headers = new Headers();
  if (password.length) {
    headers.append('X-OTA-PASSWORD', password);
  }

  const form = new FormData();
  form.append('firmware', file, file.name);

  flashing.value = true;
  const toastId = notify(`Uploading ${file.name}...`, 'info', 15000, true);
  try {
    const response = await fetch('/ota_update', {
      method: 'POST',
      headers,
      body: form
    });

    let payload = {};
    const text = await response.text();
    try { payload = text ? JSON.parse(text) : {}; } catch (e) { payload = { status: 'error', reason: text || 'invalid_response' }; }

    if (response.status === 401) {
      updateToast(toastId, 'Unauthorized: wrong OTA password.', 'error', 6000);
    } else if (response.status === 403 || payload.reason === 'ota_disabled') {
      updateToast(toastId, 'OTA updates are disabled on this device.', 'error', 6000);
    } else if (!response.ok || payload.status !== 'ok') {
      const reason = payload.reason || response.statusText || 'Upload failed';
      updateToast(toastId, `Flash failed: ${reason}`, 'error', 6000);
    } else {
      updateToast(toastId, 'Flash uploaded. Device will reboot shortly.', 'success', 12000);
      notify('Flash successful! Device is rebooting...', 'success', 6000);
      notify('Waiting for device to reboot...', 'info', 8000);
    }
  } catch (error) {
    updateToast(toastId, `Flash failed: ${error.message}`, 'error', 6000);
  } finally {
    flashing.value = false;
    if (otaFileInput.value) otaFileInput.value.value = '';
  }
}

onMounted(() => {
  loadSettings();
  injectVersion();
  initLive();
  fetchRuntime();
  fetchRuntimeMeta();
});

onBeforeUnmount(()=>{
  if (pollTimer) clearInterval(pollTimer);
  if (ws) ws.close();
});

function initLive(){
  const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
  const url = proto + location.host + '/ws';
  try {
    ws = new WebSocket(url);
    ws.onopen = ()=>{
      wsConnected.value = true;
      setTimeout(()=>{ if(!runtime.value.uptime) fetchRuntime(); }, 300);
    };
    ws.onclose = ()=>{ wsConnected.value = false; fallbackPolling(); };
    ws.onerror = ()=>{ wsConnected.value = false; fallbackPolling(); };
    ws.onmessage = ev => { try { runtime.value = JSON.parse(ev.data); } catch(e){} };
    setTimeout(()=>{ if(!wsConnected.value) { try{ ws.close(); }catch(e){} fallbackPolling(); } }, 1500);
  } catch(e){ fallbackPolling(); }
}

async function fetchRuntime(){
  try {
    const r = await fetch('/runtime.json?ts=' + Date.now());
    if(!r.ok) return;
    runtime.value = await r.json();
    if(runtimeMeta.value.length === 0) buildRuntimeGroups();
  } catch(e) { /* ignore */ }
}

async function fetchRuntimeMeta(){
  try {
    const r = await fetch('/runtime_meta.json?ts=' + Date.now());
    if(!r.ok) return;
    runtimeMeta.value = await r.json();
    buildRuntimeGroups();
  } catch(e){ /* ignore */ }
}

function buildRuntimeGroups(){
  if(runtimeMeta.value.length){
    const grouped = {};
    for(const m of runtimeMeta.value){
      if(!grouped[m.group]) grouped[m.group] = { name: m.group, title: capitalize(m.group), fields: [] };
      grouped[m.group].fields.push({
        key: m.key, label: m.label, unit: m.unit, precision: m.precision,
        warnMin: m.warnMin, warnMax: m.warnMax, alarmMin: m.alarmMin, alarmMax: m.alarmMax,
        isBool: m.isBool, hasAlarm: m.hasAlarm || false, alarmWhenTrue: m.alarmWhenTrue || false,
        isString: m.isString || false, isDivider: m.isDivider || false, staticValue: m.staticValue || '', order: m.order !== undefined ? m.order : 100
      });
    }
    runtimeGroups.value = Object.values(grouped).map(g=>{ g.fields.sort((a,b)=>{ if(a.isDivider && b.isDivider) return a.order - b.order; if(a.order === b.order) return a.label.localeCompare(b.label); return a.order - b.order; }); return g; });
    return;
  }
  const fallback = [];
  if(runtime.value.sensors){
    fallback.push({ name:'sensors', title:'Sensors', fields: Object.keys(runtime.value.sensors).map(k=>({key:k,label:capitalize(k),unit:'',precision:(k.toLowerCase().includes('temp')||k.toLowerCase().includes('dew'))?1:0})) });
  }
  if(runtime.value.system){
    fallback.push({ name:'system', title:'System', fields: Object.keys(runtime.value.system).map(k=>({key:k,label:capitalize(k),unit:'',precision:0})) });
  }
  runtimeGroups.value = fallback;
}

function capitalize(s){ if(!s) return ''; return s.charAt(0).toUpperCase() + s.slice(1); }
function formatValue(val, meta){ if(val == null) return ''; if(typeof val === 'number'){ if(meta && typeof meta.precision === 'number' && !Number.isInteger(val)){ try { return val.toFixed(meta.precision); } catch(e){ return val; } } return val; } return val; }
function severityClass(val, meta){ if(typeof val !== 'number' || !meta) return ''; if(meta.alarmMin!==undefined && meta.alarmMin!==null && meta.alarmMin!=='' && meta.alarmMin!==false && !Number.isNaN(meta.alarmMin) && val < meta.alarmMin) return 'sev-alarm'; if(meta.alarmMax!==undefined && meta.alarmMax!==null && meta.alarmMax!=='' && meta.alarmMax!==false && !Number.isNaN(meta.alarmMax) && val > meta.alarmMax) return 'sev-alarm'; if(meta.warnMin!==undefined && meta.warnMin!==null && !Number.isNaN(meta.warnMin) && val < meta.warnMin) return 'sev-warn'; if(meta.warnMax!==undefined && meta.warnMax!==null && !Number.isNaN(meta.warnMax) && val > meta.warnMax) return 'sev-warn'; return ''; }
function valueClasses(val, meta){
  if(meta && meta.isBool){
    if(meta.hasAlarm){
      const alarm = isBoolAlarm(val, meta);
      return alarm ? 'sev-alarm bool-row' : 'bool-row';
    }
    return 'bool-row';
  }
  return severityClass(val, meta);
}
function isBoolAlarm(val, meta){ if(!meta || !meta.isBool || !meta.hasAlarm) return false; if(meta.alarmWhenTrue) return !!val; return !val; }
function boolDotClass(val, meta){
  if(!meta || !meta.isBool){ return ''; }
  if(meta.hasAlarm){
    const alarm = isBoolAlarm(val, meta);
    if(alarm) return 'alarm';
    // safe state colorization for alarm booleans: show green when non-alarm
    return 'safe';
  }
  // plain boolean without alarm semantics
  return val ? 'on' : 'off';
}
function fallbackPolling(){ if (pollTimer) clearInterval(pollTimer); fetchRuntime(); pollTimer = setInterval(fetchRuntime, 2000); }
</script>

<style scoped>
body { font-family: Arial, sans-serif; margin: 1rem; background-color: #f0f0f0; }
.tabs { display:flex; gap:.5rem; justify-content:center; margin-bottom:1rem; }
.tabs button { background:#ddd; color:#222; width:auto; padding:.5rem 1rem; }
.tabs button.active { background:#3498db; color:#fff; }
.flash-btn { background:#8e44ad; color:#fff; }
.flash-btn:disabled { opacity:0.5; cursor:not-allowed; }
.hidden-file-input { display:none; }
.live-cards { display:flex; flex-wrap:wrap; gap:1rem; justify-content:center; }
.card { background:#fff; box-shadow:0 1px 3px rgba(0,0,0,.15); padding:0.75rem 1rem; border-radius:8px; min-width:140px; }
.live-cards .card p { margin:0.25rem 0; }
.sev-warn { color:#b58900; font-weight:600; }
.sev-alarm { color:#d00000; font-weight:700; animation: blink 1.6s linear infinite; }
@keyframes blink { 50% { opacity:0.30; } }
.bool-row { display:flex; align-items:center; gap:.4rem; }
.bool-dot { width:.85rem; height:.85rem; border-radius:50%; display:inline-block; box-shadow:0 0 2px rgba(0,0,0,.4); }
.bool-dot.on { background:#2ecc71; }
.bool-dot.off { background:#fff; border:1px solid #888; }
.bool-dot.alarm { background:#d00000; animation: blink 1.6s linear infinite; }
.bool-dot.safe { background:#2ecc71; }
.rt-divider { border:0; border-top:1px solid #ccc; margin:.6rem 0 .4rem; position:relative; }
.rt-divider:before { content: attr(data-label); position:absolute; top:-0.7rem; left:.4rem; background:#fff; padding:0 .3rem; font-size:.65rem; color:#999; letter-spacing:.05em; }
.rt-string { font-size:.85rem; margin:.25rem 0; color:#333; }
.ta-center { text-align:center; }
.uptime { font-size:.85rem; color:#555; margin-top:.5rem;padding:.5rem;text-align: center;}
/* Slow down blink for readability */
.live-status { text-align:center; margin-top:1rem; font-size:.85rem; color:#555; }
h1 { color:#3498db; margin-top:.20rem; border-bottom:2px solid #3498db; padding-bottom:.20rem; font-size:1.8rem; font-weight: bold; min-width: 120px;}
h2 { color:#3498db; margin-top:0; border-bottom:2px solid #3498db; padding-bottom:10px; font-size:1.2rem; }
label { font-weight: bold; min-width: 120px; }
input, select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; box-sizing: border-box; }
button { padding: 8px 16px; font-size: 14px; border: none; border-radius: 4px; cursor: pointer; transition: 0.3s; width: 100%; margin-bottom: 5px; }
.apply-btn { background-color: #3498db; color: white; }
.save-btn { background-color: #27ae60; color: white; }
.reset-btn { background-color: #e74c3c; color: white; }
.action-buttons { text-align: center; margin: 20px 0; display: flex; flex-direction: column; gap: 10px; }
.actions { display: flex; flex-direction: column; gap: 5px; width: 100%; }
@media (min-width: 768px) { .action-buttons { flex-direction: row; } button { width:auto; } }
/* Toast notifications */
.toast-wrapper { position:fixed; bottom:16px; left:50%; transform:translateX(-50%); display:flex; flex-direction:column; gap:8px; z-index:2000; align-items:center; }
.toast { position:relative; background:#303030; color:#fff; padding:10px 14px; border-radius:6px; min-width:200px; max-width:360px; box-shadow:0 4px 10px rgba(0,0,0,.4); font-size:.85rem; line-height:1.3; display:flex; align-items:center; gap:10px; }
.toast.info { background:#455a64; }
.toast.success { background:#2e7d32; }
.toast.error { background:#c62828; }
.toast-close { background:transparent; border:none; color:#fff; cursor:pointer; font-size:.9rem; padding:0 4px; }
</style>

