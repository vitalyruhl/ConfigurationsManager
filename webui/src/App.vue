<template>
  <div>
    <h1 id="mainHeader" class="ta-center">{{ version }}</h1>
    <div class="tabs">
      <button
        :class="{ active: activeTab === 'live' }"
        @click="activeTab = 'live'"
      >
        Live
      </button>
      <button
        :class="{ active: activeTab === 'settings' }"
        @click="switchToSettings()"
      >
        Settings
      </button>
      <button class="flash-btn" @click="startFlash" :disabled="!canFlash">
        Flash
      </button>
    </div>
    <div class="toast-wrapper">
      <div v-for="t in toasts" :key="t.id" class="toast" :class="t.type">
        <span class="toast-msg">{{ t.message }}</span>
        <button
          class="toast-close"
          @click="dismissToast(t.id)"
          aria-label="Close"
        >
          ✕
        </button>
      </div>
    </div>
    <!-- Settings Authentication Modal -->
    <div v-if="showSettingsAuth" class="modal-overlay" @click="cancelSettingsAuth">
      <div class="modal-content" @click.stop>
        <h3>{{ pendingFlashStart ? 'OTA Flash Access Required' : 'Settings Access Required' }}</h3>
        <p v-if="pendingFlashStart" style="margin: 0 0 15px 0; color: #666; font-size: 14px;">
          Password required to start firmware flash/OTA update
        </p>
        <div class="password-input-container">
          <input
            ref="settingsPasswordInput"
            v-model="settingsPassword"
            :type="showSettingsPassword ? 'text' : 'password'"
            placeholder="Enter settings password"
            @keyup.enter="confirmSettingsAuth"
            @keyup.escape="cancelSettingsAuth"
            autocomplete="off"
          />
          <button
            class="password-toggle"
            @click="showSettingsPassword = !showSettingsPassword"
            :title="showSettingsPassword ? 'Hide password' : 'Show password'"
          >
            {{ showSettingsPassword ? '🙈' : '👁️' }}
          </button>
        </div>
        <div class="modal-buttons">
          <button @click="cancelSettingsAuth" class="cancel-btn">Cancel</button>
          <button @click="confirmSettingsAuth" class="confirm-btn">Authenticate</button>
        </div>
      </div>
    </div>

    <RuntimeDashboard
      v-if="activeTab === 'live'"
      ref="runtimeDashboard"
      :config="config"
      @can-flash-change="handleCanFlashChange"
    />
    <section v-else-if="activeTab === 'settings' && settingsAuthenticated" class="settings-view">
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
        <button @click="applyAll" class="apply-btn" :disabled="refreshing">
          {{ refreshing ? "..." : "Apply All" }}
        </button>
        <button @click="saveAll" class="save-btn" :disabled="refreshing">
          {{ refreshing ? "..." : "Save All" }}
        </button>
        <button @click="resetDefaults" class="reset-btn" :disabled="refreshing">
          Reset Defaults
        </button>
        <button @click="rebootDevice" class="reset-btn" :disabled="refreshing">
          Reboot
        </button>
      </div>
    </section>
  </div>
</template>
<script setup>
import { ref, onMounted, provide } from "vue";
import Category from "./components/Category.vue";
import RuntimeDashboard from "./components/RuntimeDashboard.vue";

// Simple SHA-256 implementation for password hashing
async function sha256(message) {
  const msgBuffer = new TextEncoder().encode(message);
  const hashBuffer = await crypto.subtle.digest('SHA-256', msgBuffer);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  const hashHex = hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
  return hashHex;
}

// Check if a value is a password field that should be hashed
function isPasswordField(category, key, settingData) {
  return settingData && settingData.isPassword === true;
}

// Hash password if it's a password field, otherwise return original value
async function processValueForTransmission(category, key, value, settingData) {
  if (isPasswordField(category, key, settingData) && value && value.trim() !== '') {
    // Only hash non-empty passwords
    const hashed = await sha256(value);
    console.log(`[Security] Hashing password for ${category}.${key}`);
    return `hashed:${hashed}`; // Prefix to indicate this is a hashed password
  }
  return value; // Return original value for non-passwords
}

const config = ref({});
const refreshKey = ref(0);
const version = ref("");
const refreshing = ref(false);
const activeTab = ref("live");
const runtimeDashboard = ref(null);
const canFlash = ref(false);
const toasts = ref([]); // {id,message,type,sticky,ts}
let toastCounter = 0;

// Settings authentication
const showSettingsAuth = ref(false);
const settingsAuthenticated = ref(false);
const settingsPassword = ref("");
const showSettingsPassword = ref(false);
const settingsPasswordInput = ref(null);
const pendingFlashStart = ref(false); // Track if we need to start flash after auth

// Configurable settings password - loaded from backend
const SETTINGS_PASSWORD = ref("cf"); // Default fallback

async function loadSettingsPassword() {
  try {
    const response = await fetch("/config/settings_password");
    if (response.ok) {
      const data = await response.json();
      if (data.status === "ok" && data.password) {
        SETTINGS_PASSWORD.value = data.password;
        console.log("[Frontend] Settings password loaded from backend");
      }
    }
  } catch (e) {
    console.warn("[Frontend] Could not load settings password from backend, using default:", e);
  }
}
const opBusy = ref({});
const rURIComp = encodeURIComponent;

function notify(
  message,
  type = "info",
  duration = 3000,
  sticky = false,
  id = null
) {
  if (id == null) id = ++toastCounter;
  const existing = toasts.value.find((t) => t.id === id);
  if (existing) {
    existing.message = message;
    existing.type = type;
    existing.sticky = sticky;
    existing.ts = Date.now();
  } else toasts.value.push({ id, message, type, sticky, ts: Date.now() });
  if (!sticky) setTimeout(() => dismissToast(id), duration);
  return id;
}
function dismissToast(id) {
  toasts.value = toasts.value.filter((t) => t.id !== id);
}
function updateToast(id, message, type = "info", duration = 2500) {
  const t = toasts.value.find((x) => x.id === id);
  if (!t) return notify(message, type, duration);
  t.message = message;
  t.type = type;
  t.sticky = false;
  setTimeout(() => dismissToast(id), duration);
}
provide("notify", notify);
provide("updateToast", updateToast);
provide("dismissToast", dismissToast);

async function loadSettings() {
  try {
    console.log("[Frontend] Starting loadSettings...");
    
    // Firefox-compatible timeout implementation
    const fetchPromise = fetch("/config.json");
    const timeoutPromise = new Promise((_, reject) => 
      setTimeout(() => reject(new Error('Request timeout')), 10000)
    );
    
    const r = await Promise.race([fetchPromise, timeoutPromise]);
    
    console.log(`[Frontend] Response status: ${r.status} ${r.statusText}`);
    console.log(`[Frontend] Response headers:`, Object.fromEntries(r.headers.entries()));
    
    if (!r.ok) {
      throw new Error(`HTTP ${r.status}: ${r.statusText}`);
    }
    
    // Check content-length header vs actual response
    const contentLength = r.headers.get('content-length');
    console.log(`[Frontend] config.json - Content-Length: ${contentLength}`);
    
    const text = await r.text();
    console.log(`[Frontend] config.json - Actual response length: ${text.length}`);
    
    if (contentLength && parseInt(contentLength) !== text.length) {
      console.warn(`[Frontend] Content-Length mismatch! Header: ${contentLength}, Actual: ${text.length}`);
    }
    
    const data = JSON.parse(text);
    config.value = data.config || data;
    refreshKey.value++;
    
    console.log("[Frontend] config.json loaded successfully");
  } catch (e) {
    console.error("[Frontend] loadSettings error:", e);
    if (e.name === 'AbortError') {
      notify("Connection timeout: Could not load settings", "error");
    } else if (e.message.includes('Content-Length')) {
      notify("Server response error: Data transmission interrupted", "error");
    } else {
      notify("Fehler: " + e.message, "error");
    }
  }
}
function switchToSettings() {
  if (settingsAuthenticated.value) {
    activeTab.value = "settings";
    loadSettings();
  } else {
    showSettingsAuth.value = true;
    // Focus password input after modal appears
    setTimeout(() => {
      settingsPasswordInput.value?.focus();
    }, 100);
  }
}

function confirmSettingsAuth() {
  if (settingsPassword.value === SETTINGS_PASSWORD.value) {
    settingsAuthenticated.value = true;
    showSettingsAuth.value = false;
    settingsPassword.value = ""; // Clear password for security
    
    // Check if we need to start flash after authentication
    if (pendingFlashStart.value) {
      pendingFlashStart.value = false;
      notify("Access granted - Starting OTA flash...", "success");
      runtimeDashboard.value?.startFlash();
    } else {
      // Normal settings access
      activeTab.value = "settings";
      loadSettings();
      notify("Settings access granted", "success");
    }
  } else {
    notify("Invalid password", "error");
    settingsPassword.value = ""; // Clear wrong password
    settingsPasswordInput.value?.focus();
  }
}

function cancelSettingsAuth() {
  showSettingsAuth.value = false;
  settingsPassword.value = "";
  showSettingsPassword.value = false;
  pendingFlashStart.value = false; // Clear any pending flash start
  activeTab.value = "live"; // Stay on live tab
}
async function injectVersion() {
  try {
    const r = await fetch("/version");
    if (!r.ok) throw new Error("Version fetch failed");
    version.value = String(await r.text());
  } catch (e) {
    version.value = "";
  }
}
async function loadUserTheme() {
  try {
    const css = await fetch(`/user_theme.css?ts=${Date.now()}`)
      .then((r) => (r.status === 200 ? r.text() : ""))
      .catch(() => "");
    if (css && css.trim().length) {
      const existing = document.getElementById("user-theme-css");
      if (existing) existing.remove();
      const el = document.createElement("style");
      el.id = "user-theme-css";
      el.textContent = css;
      document.head.appendChild(el);
    }
  } catch (e) {}
}
async function applySingle(category, key, value) {
  const opKey = `${category}.${key}`;
  opBusy.value[opKey] = true;
  const tid = notify(`Applying: ${opKey} ...`, "info", 7000, true);
  try {
    // Get setting metadata to check if this is a password field
    const settingData = config.value[category] && config.value[category][key];
    
    // Process value (hash if password, otherwise use as-is)
    const processedValue = await processValueForTransmission(category, key, value, settingData);
    
    const r = await fetch(
      `/config/apply?category=${rURIComp(category)}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value: processedValue }),
      }
    );
    const json = await r.json().catch(() => ({}));
    if (!r.ok || json.status !== "ok") throw new Error(json.reason || "Failed");
    await loadSettings();
    updateToast(tid, `Applied: ${opKey}`, "success");
  } catch (e) {
    updateToast(tid, `Apply failed ${opKey}: ${e.message}`, "error");
  } finally {
    delete opBusy.value[opKey];
  }
}
async function saveSingle(category, key, value) {
  const opKey = `${category}.${key}`;
  opBusy.value[opKey] = true;
  const tid = notify(`Saving: ${opKey} ...`, "info", 7000, true);
  try {
    // Add timeout to HTTP request
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 10000); // 10 second timeout
    
    // Get setting metadata to check if this is a password field
    const settingData = config.value[category] && config.value[category][key];
    
    // Process value (hash if password, otherwise use as-is)
    const processedValue = await processValueForTransmission(category, key, value, settingData);
    
    const r = await fetch(
      `/config/save?category=${rURIComp(category)}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value: processedValue }),
        signal: controller.signal
      }
    );
    clearTimeout(timeoutId);
    
    const json = await r.json().catch(() => ({}));
    if (!r.ok || json.status !== "ok") throw new Error(json.reason || "Failed");
    await loadSettings();
    updateToast(tid, `Saved: ${opKey}`, "success");
  } catch (e) {
    if (e.name === 'AbortError') {
      updateToast(tid, `Save timeout ${opKey}: Connection lost`, "error");
    } else {
      updateToast(tid, `Save failed ${opKey}: ${e.message}`, "error");
    }
  } finally {
    delete opBusy.value[opKey];
  }
}
function collectSettingsInputs() {
  return Array.from(
    document.querySelectorAll("#settingsContainer input[name]")
  );
}
async function saveAll() {
  if (!confirm("Save all settings?")) return;
  const all = {};
  collectSettingsInputs().forEach((input) => {
    const [cat, key] = input.name.split(".");
    if (!all[cat]) all[cat] = {};
    if (
      input.dataset.isPassword === "1" &&
      (!input.value || input.value === "***")
    )
      return;
    all[cat][key] = input.type === "checkbox" ? input.checked : input.value;
  });
  refreshing.value = true;
  try {
    const r = await fetch("/config/save_all", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(all),
    });
    const json = await r.json().catch(() => ({}));
    if (r.ok && json.status === "ok") {
      notify("All settings saved", "success");
      await loadSettings();
    } else notify("Failed to save all settings", "error");
  } catch (e) {
    notify("Error: " + e.message, "error");
  } finally {
    refreshing.value = false;
  }
}
async function applyAll() {
  if (!confirm("Apply all settings?")) return;
  const all = {};
  collectSettingsInputs().forEach((input) => {
    const [cat, key] = input.name.split(".");
    if (!all[cat]) all[cat] = {};
    all[cat][key] = input.type === "checkbox" ? input.checked : input.value;
  });
  refreshing.value = true;
  try {
    const r = await fetch("/config/apply_all", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(all),
    });
    const json = await r.json().catch(() => ({}));
    if (r.ok && json.status === "ok") {
      notify("All settings applied", "success");
      await loadSettings();
    } else notify("Failed to apply all settings", "error");
  } catch (e) {
    notify("Error: " + e.message, "error");
  } finally {
    refreshing.value = false;
  }
}
async function resetDefaults() {
  if (!confirm("Reset to defaults?")) return;
  try {
    const r = await fetch("/config/reset", { method: "POST" });
    if (r.ok) {
      notify("Reset to defaults", "success");
      setTimeout(() => location.reload(), 1000);
    }
  } catch (e) {
    notify("Error: " + e.message, "error");
  }
}
function rebootDevice() {
  if (!confirm("Reboot device?")) return;
  fetch("/reboot", { method: "POST" })
    .then((r) => {
      if (r.ok) notify("Rebooting...", "info");
    })
    .catch((e) => notify("Error: " + e.message, "error"));
}
function startFlash() {
  if (!canFlash.value) {
    notify("OTA is disabled", "error");
    return;
  }
  
  // Check if already authenticated for settings (reuse authentication)
  if (!settingsAuthenticated.value) {
    // Show authentication modal for Flash/OTA access
    showSettingsAuth.value = true;
    // Store that we want to start flash after authentication
    pendingFlashStart.value = true;
    return;
  }
  
  runtimeDashboard.value?.startFlash();
}
function handleCanFlashChange(v) {
  canFlash.value = !!v;
}
onMounted(() => {
  loadUserTheme();
  loadSettingsPassword();
  loadSettings();
  injectVersion();
});
</script>
<style scoped>
#mainHeader {
  color: #3498db;
  margin: 0.2rem 0 0;
  border-bottom: 2px solid #3498db;
  padding-bottom: 0.2rem;
  font-size: 1.8rem;
  font-weight: 700;
}
.ta-center {
  text-align: center;
}
.tabs {
  display: flex;
  gap: 0.5rem;
  justify-content: center;
  margin: 1rem 0;
}
.tabs button {
  background: slategray;
  color: #f5f5f5;
  padding: 0.5rem 1.1rem;
  border-radius: 6px;
  border: none;
  cursor: pointer;
  font-weight: 600;
  transition: background 0.2s;
}
.tabs button.active {
  background: darkorange;
  color: #fff;
}
.flash-btn {
  background: #8e44ad;
  color: #fff;
}
.flash-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  background: gainsboro;
}
.settings-view {
  padding: 1rem 0 2rem;
}
.action-buttons {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  margin: 1.5rem 0;
  align-items: stretch;
}
.action-buttons button {
  padding: 0.6rem 1.4rem;
  border-radius: 6px;
  border: none;
  font-size: 0.95rem;
  font-weight: 600;
  cursor: pointer;
  transition: transform 0.15s, box-shadow 0.15s;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.12);
}
.action-buttons button:hover {
  transform: translateY(-1px);
  box-shadow: 0 4px 10px rgba(0, 0, 0, 0.18);
}
.apply-btn {
  background: #3498db;
  color: #fff;
}
.save-btn {
  background: #27ae60;
  color: #fff;
}
.reset-btn {
  background: #e74c3c;
  color: #fff;
}
@media (min-width: 768px) {
  .action-buttons {
    flex-direction: row;
    justify-content: center;
  }
  .action-buttons button {
    width: auto;
  }
}
.toast-wrapper {
  position: fixed;
  bottom: 16px;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  flex-direction: column;
  gap: 8px;
  z-index: 2000;
  align-items: center;
}
.toast {
  position: relative;
  background: #303030;
  color: #fff;
  padding: 10px 14px;
  border-radius: 6px;
  min-width: 200px;
  max-width: 360px;
  box-shadow: 0 4px 10px rgba(0, 0, 0, 0.4);
  font-size: 0.85rem;
  line-height: 1.3;
  display: flex;
  align-items: center;
  gap: 10px;
}
.toast.info {
  background: #455a64;
}
.toast.success {
  background: #2e7d32;
}
.toast.error {
  background: #c62828;
}
.toast-close {
  background: transparent;
  border: none;
  color: #fff;
  cursor: pointer;
  font-size: 0.9rem;
  padding: 0 4px;
}

/* Settings authentication modal */
.modal-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.7);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 3000; /* above toasts */
}

.modal-content {
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 8px;
  padding: 24px;
  max-width: 420px;
  width: 90%;
  color: #f0f6fc;
  box-shadow: 0 10px 30px rgba(0,0,0,0.45);
}

.modal-content h3 {
  margin: 0 0 16px 0;
  color: #f0f6fc;
  font-size: 18px;
}

.password-input-container {
  display: flex;
  gap: 8px;
  margin-bottom: 16px;
}

.password-input-container input {
  flex: 1;
  padding: 8px 12px;
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 4px;
  color: #f0f6fc;
  font-size: 14px;
}

.password-input-container input:focus {
  outline: none;
  border-color: #1f6feb;
  box-shadow: 0 0 0 2px rgba(31, 111, 235, 0.2);
}

.password-toggle {
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 4px;
  padding: 8px 12px;
  cursor: pointer;
  color: #8b949e;
  font-size: 14px;
  transition: all 0.2s;
  white-space: nowrap;
}

.password-toggle:hover {
  background: #30363d;
  color: #f0f6fc;
}

.modal-buttons {
  display: flex;
  gap: 8px;
  justify-content: flex-end;
}

.modal-buttons .cancel-btn {
  background: #6e7681;
  color: #f0f6fc;
}

.modal-buttons .cancel-btn:hover {
  background: #8b949e;
}

.modal-buttons .confirm-btn {
  background: #238636;
  color: #f0f6fc;
}

.modal-buttons .confirm-btn:hover {
  background: #2ea043;
}
</style>
