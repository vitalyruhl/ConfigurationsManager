<template>
  <div>
    <h1 id="mainHeader">Device {{ version }}</h1>
    <div class="tabs">
      <button :class="{active: activeTab==='live'}" @click="activeTab='live'">Live</button>
      <button :class="{active: activeTab==='settings'}" @click="switchToSettings()">Settings</button>
    </div>
    <div id="status" v-if="statusMessage" :style="{backgroundColor: statusColor}">{{ statusMessage }}</div>

    <section v-if="activeTab==='live'" class="live-view">
      <div class="live-cards">
        <div class="card" v-for="group in runtimeGroups" :key="group.name">
          <h3>{{ group.title }}</h3>
          <template v-for="f in group.fields" :key="f.key">
            <p v-if="runtime[group.name] && runtime[group.name][f.key] !== undefined">
              {{ f.label }}: {{ formatValue(runtime[group.name][f.key], f) }}<span v-if="f.unit"> {{ f.unit }}</span>
            </p>
          </template>
          <p v-if="group.name==='system' && runtime.uptime !== undefined">Uptime: {{ Math.floor((runtime.uptime||0)/1000) }} s</p>
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
import { ref, onMounted, onBeforeUnmount } from 'vue';
import Category from './components/Category.vue';

const config = ref({});
const refreshKey = ref(0); // forces re-render of settings tree
const version = ref('');
const statusMessage = ref('');
const statusColor = ref('');
const refreshing = ref(false); // new state
const activeTab = ref('live');

// Runtime values state
const runtime = ref({});
let pollTimer = null;
let ws = null;
const wsConnected = ref(false);
// Runtime metadata state for dynamic grouping
const runtimeMeta = ref([]); // raw metadata array from /runtime_meta.json
const runtimeGroups = ref([]); // transformed groups -> [{ name, title, fields:[{key,label,unit,precision}]}]

async function loadSettings() {
  try {
    const response = await fetch('/config.json');
    if (!response.ok) throw new Error('HTTP Error');
    const data = await response.json();
    console.log('Fetched config:', data);
    config.value = data.config || data;
    // bump key to force child component re-mount (ensures stale visibility cache cleared)
    refreshKey.value++;
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
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
    console.log('Fetched Version:', data);
    version.value = 'V' + data;
  } catch (e) {
    version.value = '';
  }
}

function showStatus(message, color) {
  statusMessage.value = message;
  statusColor.value = color;
  setTimeout(() => (statusMessage.value = ''), 3000);
}

async function applySingle(category, key, value) {
  try {
    refreshing.value = true;
    const response = await fetch(`/config/apply?category=${encodeURIComponent(category)}&key=${encodeURIComponent(key)}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ value })
      });
    if (!response.ok) throw new Error('Apply failed');
    await loadSettings();
    showStatus('Applied', 'green');
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
  } finally {
    refreshing.value = false;
  }
}

async function saveSingle(category, key, value) {
  try {
    refreshing.value = true;
    const response = await fetch(`/config/save?category=${encodeURIComponent(category)}&key=${encodeURIComponent(key)}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ value })
      });
    if (!response.ok) throw new Error('Save failed');
    await loadSettings();
    showStatus('Saved', 'green');
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
  } finally {
    refreshing.value = false;
  }
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
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(all)
    });
    showStatus(response.ok ? 'All settings saved!' : 'Error!', response.ok ? 'green' : 'red');
    if (response.ok) await loadSettings();
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
  }
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
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(all)
    });
    showStatus(response.ok ? 'All settings Applied!' : 'Error!', response.ok ? 'green' : 'red');
    if (response.ok) await loadSettings();
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
  }
}

async function resetDefaults() {
  if (!confirm('Reset to defaults?')) return;
  try {
    const response = await fetch('/config/reset', { method: 'POST' });
    if (response.ok) {
      showStatus('Reset successful!', 'green');
      setTimeout(() => location.reload(), 1000);
    }
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
  }
}

function rebootDevice() {
  if (!confirm('Reboot device?')) return;
  fetch('/config/reboot', { method: 'POST' })
    .then(response => response.ok ? showStatus('Rebooting...', 'blue') : null)
    .catch(error => showStatus('Error: ' + error.message, 'red'));
}

onMounted(() => {
  loadSettings();
  injectVersion();
  initLive();
  // Immediate initial fetch so user sees data even before WS connects or first poll tick
  fetchRuntime();
  fetchRuntimeMeta();
});

onBeforeUnmount(()=>{
  if (pollTimer) clearInterval(pollTimer);
  if (ws) ws.close();
});

function initLive(){
  // Try WebSocket first
  const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
  const url = proto + location.host + '/ws';
  try {
    ws = new WebSocket(url);
    ws.onopen = ()=>{ 
      wsConnected.value = true; 
      // Safety fetch if first push hasn't arrived quickly
      setTimeout(()=>{ if(!runtime.value.uptime) fetchRuntime(); }, 300);
    };
    ws.onclose = ()=>{ wsConnected.value = false; fallbackPolling(); };
    ws.onerror = ()=>{ wsConnected.value = false; fallbackPolling(); };
    ws.onmessage = ev => {
      try { runtime.value = JSON.parse(ev.data); } catch(e){}
    };
    // If not connected within 1.5s -> fallback
    setTimeout(()=>{ if(!wsConnected.value) { try{ ws.close(); }catch(e){} fallbackPolling(); } }, 1500);
  } catch(e){
    fallbackPolling();
  }
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
      grouped[m.group].fields.push({ key: m.key, label: m.label, unit: m.unit, precision: m.precision });
    }
    runtimeGroups.value = Object.values(grouped);
    return;
  }
  // Fallback (legacy firmware without metadata endpoint)
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

function formatValue(val, meta){
  if(val == null) return '';
  if(typeof val === 'number'){
    if(meta && typeof meta.precision === 'number' && !Number.isInteger(val)){
      try { return val.toFixed(meta.precision); } catch(e){ return val; }
    }
    return val;
  }
  return val;
}

function fallbackPolling(){
  if (pollTimer) clearInterval(pollTimer);
  fetchRuntime();
  pollTimer = setInterval(fetchRuntime, 2000);
}
</script>

<style scoped>
body {
  font-family: Arial, sans-serif;
  margin: 1rem;
  background-color: #f0f0f0;
}
.logo-bar {
  display: flex;
  align-items: flex-start;
  justify-content: flex-start;
  height: 80px;
  margin-bottom: 0.5rem;
}
.app-logo {
  max-width: 80px;
  max-height: 80px;
  width: auto;
  height: auto;
  object-fit: contain;
  display: block;
}
h1 {
  color: #2c3e50;
  text-align: center;
  font-size: 1.5rem;
}
.tabs { display:flex; gap:.5rem; justify-content:center; margin-bottom:1rem; }
.tabs button { background:#ddd; color:#222; width:auto; padding:.5rem 1rem; }
.tabs button.active { background:#3498db; color:#fff; }
.live-cards { display:flex; flex-wrap:wrap; gap:1rem; justify-content:center; }
.card { background:#fff; box-shadow:0 1px 3px rgba(0,0,0,.15); padding:0.75rem 1rem; border-radius:8px; min-width:140px; }
 .live-cards .card p { margin:0.25rem 0; }
 .live-cards { gap:1.25rem; }
 body { padding:0.5rem; }
.live-status { text-align:center; margin-top:1rem; font-size:.85rem; color:#555; }
h2 {
  color: #3498db;
  margin-top: 0;
  border-bottom: 2px solid #3498db;
  padding-bottom: 10px;
  font-size: 1.2rem;
}
label {
  font-weight: bold;
  min-width: 120px;
}
input,
select {
  width: 100%;
  padding: 8px;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 14px;
  box-sizing: border-box;
}
button {
  padding: 8px 16px;
  font-size: 14px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: 0.3s;
  width: 100%;
  margin-bottom: 5px;
}
.apply-btn {
  background-color: #3498db;
  color: white;
}
.save-btn {
  background-color: #27ae60;
  color: white;
}
.reset-btn {
  background-color: #e74c3c;
  color: white;
}
#status {
  position: fixed;
  top: 10px;
  right: 10px;
  left: 10px;
  padding: 10px;
  border-radius: 5px;
  display: none;
  max-width: none;
  z-index: 1000;
}
.action-buttons {
  text-align: center;
  margin: 20px 0;
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.actions {
  display: flex;
  flex-direction: column;
  gap: 5px;
  width: 100%;
}
@media (min-width: 768px) {
  body {
    margin: 2rem;
  }
  .action-buttons {
    flex-direction: row;
  }
}
</style>

