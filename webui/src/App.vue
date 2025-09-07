<template>
  <div>
    <h1 id="mainHeader">Device Configuration V{{ version }}</h1>
    <div id="status" v-if="statusMessage" :style="{backgroundColor: statusColor}">{{ statusMessage }}</div>
    <div id="settingsContainer">
      <Category
        v-for="(settings, category) in config"
        :key="category"
        :category="category"
        :settings="settings"
        @apply-single="applySingle"
        @save-single="saveSingle"
      />
    </div>
    <div class="action-buttons">
      <button @click="applyAll" class="apply-btn">Apply All</button>
      <button @click="saveAll" class="save-btn">Save All</button>
      <button @click="resetDefaults" class="reset-btn">Reset Defaults</button>
      <button @click="rebootDevice" class="reset-btn">Reboot</button>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import Category from './components/Category.vue';

const config = ref({});
const version = ref('x.x.x');
const statusMessage = ref('');
const statusColor = ref('');

async function loadSettings() {
  try {
    const response = await fetch('/config.json');
    if (!response.ok) throw new Error('HTTP Error');
    const data = await response.json();
    config.value = data.config || data; // for json-server, config is nested
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
  }
}

async function injectVersion() {
  try {
    const response = await fetch('/version');
    if (!response.ok) throw new Error('Version fetch failed');
    const data = await response.json();
    version.value = data.value || data;
  } catch (e) {
    version.value = 'x.x.x';
  }
}

function showStatus(message, color) {
  statusMessage.value = message;
  statusColor.value = color;
  setTimeout(() => (statusMessage.value = ''), 3000);
}

async function applySingle(category, key, value) {
  try {
    const response = await fetch(`/config/apply?category=${encodeURIComponent(category)}&key=${encodeURIComponent(key)}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ value })
      });
    if (!response.ok) throw new Error('Apply failed');
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
  }
}

async function saveSingle(category, key, value) {
  try {
    const response = await fetch(`/config/save?category=${encodeURIComponent(category)}&key=${encodeURIComponent(key)}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ value })
      });
    if (!response.ok) throw new Error('Save failed');
  } catch (error) {
    showStatus('Error: ' + error.message, 'red');
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
  injectVersion();
  loadSettings();
});
</script>

<style scoped>
body {
  font-family: Arial, sans-serif;
  margin: 1rem;
  background-color: #f0f0f0;
}
.category {
  margin: 10px 0;
  padding: 15px;
  background: white;
  border-radius: 10px;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
}
h1 {
  color: #2c3e50;
  text-align: center;
  font-size: 1.5rem;
}
h2 {
  color: #3498db;
  margin-top: 0;
  border-bottom: 2px solid #3498db;
  padding-bottom: 10px;
  font-size: 1.2rem;
}
.setting {
  margin: 15px 0;
  padding: 15px;
  background: #f8f9fa;
  border-radius: 5px;
  display: flex;
  flex-direction: column;
  gap: 10px;
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
  .setting {
    flex-direction: row;
    align-items: center;
  }
  .actions {
    flex-direction: row;
    width: auto;
  }
  button {
    width: auto;
  }
  .action-buttons {
    flex-direction: row;
  }
  input,
  select {
    width: auto;
    flex: 2;
  }
}
</style>
