<template>
  <div>
    <h1 id="mainHeader" class="ta-center">{{ version }}</h1>
    <div class="tabs">
      <button
        v-if="hasLiveContent"
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
            {{ showSettingsPassword ? 'Hide' : 'Show' }}
          </button>
        </div>
        <div class="modal-buttons">
          <button @click="cancelSettingsAuth" class="cancel-btn">Cancel</button>
          <button @click="confirmSettingsAuth" class="confirm-btn">Authenticate</button>
        </div>
      </div>
    </div>

    <RuntimeDashboard
      v-if="activeTab === 'live' && hasLiveContent"
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
import { ref, onMounted, provide, nextTick } from "vue";
import Category from "./components/Category.vue";
import RuntimeDashboard from "./components/RuntimeDashboard.vue";

// Encryption salt placeholder (replaced during WebUI packaging for firmware)
// This is unique per project and makes simple sniffing difficult
const ENCRYPTION_SALT = "__ENCRYPTION_SALT__";

// Simple XOR-based encryption with salt
// Not cryptographically strong, but makes WiFi sniffing difficult for casual attackers
function encryptPassword(password, salt) {
  if (!password || !salt) return password;
  
  const saltBytes = new TextEncoder().encode(salt);
  const pwdBytes = new TextEncoder().encode(password);
  const encrypted = new Uint8Array(pwdBytes.length);
  
  for (let i = 0; i < pwdBytes.length; i++) {
    // XOR with salt bytes (repeating)
    encrypted[i] = pwdBytes[i] ^ saltBytes[i % saltBytes.length];
  }
  
  // Convert to hex
  return Array.from(encrypted)
    .map(b => b.toString(16).padStart(2, '0'))
    .join('');
}

function decryptPassword(encrypted, salt) {
  if (!encrypted || !salt || encrypted.length % 2 !== 0) return encrypted;
  
  try {
    const saltBytes = new TextEncoder().encode(salt);
    const encryptedBytes = new Uint8Array(encrypted.length / 2);
    
    // Convert hex to bytes
    for (let i = 0; i < encrypted.length; i += 2) {
      encryptedBytes[i / 2] = parseInt(encrypted.substr(i, 2), 16);
    }
    
    // XOR decrypt
    const decrypted = new Uint8Array(encryptedBytes.length);
    for (let i = 0; i < encryptedBytes.length; i++) {
      decrypted[i] = encryptedBytes[i] ^ saltBytes[i % saltBytes.length];
    }
    
    return new TextDecoder().decode(decrypted);
  } catch (e) {
    console.warn('[Security] Decryption failed:', e);
    return encrypted; // Return as-is if decryption fails
  }
}

// Simple SHA-256 implementation (kept for potential future use)
// Fallback for non-secure contexts (HTTP) using a pure JS implementation
async function sha256(message) {
  // Check if crypto.subtle is available (HTTPS or localhost only)
  if (typeof crypto !== 'undefined' && crypto.subtle) {
    try {
      const msgBuffer = new TextEncoder().encode(message);
      const hashBuffer = await crypto.subtle.digest('SHA-256', msgBuffer);
      const hashArray = Array.from(new Uint8Array(hashBuffer));
      const hashHex = hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
      return hashHex;
    } catch (e) {
      console.warn('[Security] crypto.subtle failed, using fallback:', e);
    }
  }
  
  // Fallback: Pure JavaScript SHA-256 implementation for HTTP contexts
  // Based on https://geraintluff.github.io/sha256/ (public domain)
  function sha256Fallback(ascii) {
    function rightRotate(value, amount) {
      return (value >>> amount) | (value << (32 - amount));
    }
    
    const mathPow = Math.pow;
    const maxWord = mathPow(2, 32);
    const lengthProperty = 'length';
    let i, j;
    let result = '';
    
    const words = [];
    const asciiBitLength = ascii[lengthProperty] * 8;
    
    let hash = sha256Fallback.h = sha256Fallback.h || [];
    const k = sha256Fallback.k = sha256Fallback.k || [];
    let primeCounter = k[lengthProperty];
    
    const isComposite = {};
    for (let candidate = 2; primeCounter < 64; candidate++) {
      if (!isComposite[candidate]) {
        for (i = 0; i < 313; i += candidate) {
          isComposite[i] = candidate;
        }
        hash[primeCounter] = (mathPow(candidate, .5) * maxWord) | 0;
        k[primeCounter++] = (mathPow(candidate, 1 / 3) * maxWord) | 0;
      }
    }
    
    ascii += '\x80';
    while (ascii[lengthProperty] % 64 - 56) ascii += '\x00';
    for (i = 0; i < ascii[lengthProperty]; i++) {
      j = ascii.charCodeAt(i);
      if (j >> 8) return;
      words[i >> 2] |= j << ((3 - i) % 4) * 8;
    }
    words[words[lengthProperty]] = ((asciiBitLength / maxWord) | 0);
    words[words[lengthProperty]] = (asciiBitLength);
    
    for (j = 0; j < words[lengthProperty];) {
      const w = words.slice(j, j += 16);
      const oldHash = hash;
      hash = hash.slice(0, 8);
      
      for (i = 0; i < 64; i++) {
        const w15 = w[i - 15], w2 = w[i - 2];
        
        const a = hash[0], e = hash[4];
        const temp1 = hash[7]
          + (rightRotate(e, 6) ^ rightRotate(e, 11) ^ rightRotate(e, 25))
          + ((e & hash[5]) ^ ((~e) & hash[6]))
          + k[i]
          + (w[i] = (i < 16) ? w[i] : (
              w[i - 16]
              + (rightRotate(w15, 7) ^ rightRotate(w15, 18) ^ (w15 >>> 3))
              + w[i - 7]
              + (rightRotate(w2, 17) ^ rightRotate(w2, 19) ^ (w2 >>> 10))
            ) | 0
          );
        const temp2 = (rightRotate(a, 2) ^ rightRotate(a, 13) ^ rightRotate(a, 22))
          + ((a & hash[1]) ^ (a & hash[2]) ^ (hash[1] & hash[2]));
        
        hash = [(temp1 + temp2) | 0].concat(hash);
        hash[4] = (hash[4] + temp1) | 0;
      }
      
      for (i = 0; i < 8; i++) {
        hash[i] = (hash[i] + oldHash[i]) | 0;
      }
    }
    
    for (i = 0; i < 8; i++) {
      for (j = 3; j + 1; j--) {
        const b = (hash[i] >> (j * 8)) & 255;
        result += ((b < 16) ? 0 : '') + b.toString(16);
      }
    }
    return result;
  }
  
  return sha256Fallback(message);
}

// Check if a value is a password field that should be encrypted
// Prefer explicit metadata; fall back to heuristic on key/display name
function isPasswordField(category, key, settingData) {
  if (settingData && settingData.isPassword === true) return true;
  const k = (key || '').toString().toLowerCase();
  const dn = ((settingData && (settingData.displayName || settingData.name)) || '').toString().toLowerCase();
  return k.includes('pass') || dn.includes('pass');
}

// Process value for transmission
// Encrypt passwords using project-specific salt to prevent WiFi sniffing
// IMPORTANT: Always keep the encryption branch present at bundle time.
// Do NOT guard by comparing to the placeholder string here, otherwise
// the bundler may optimize it away before our post-build salt injection.
// The placeholder will be replaced in the built JS by the header generator.
async function processValueForTransmission(category, key, value, settingData) {
  if (isPasswordField(category, key, settingData) && value && value.trim() !== '') {
    try {
      const encrypted = encryptPassword(value, ENCRYPTION_SALT);
      return encrypted;
    } catch (e) {
      console.warn('[Security] Password encryption failed, sending plaintext:', e);
      // Fall through to return plaintext if encryption fails
    }
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

// Detect if there is any live UI content available
const hasLiveContent = ref(true);

// Settings authentication
const showSettingsAuth = ref(false);
const settingsAuthenticated = ref(false);
const settingsPassword = ref("");
const showSettingsPassword = ref(false);
const settingsPasswordInput = ref(null);
const pendingFlashStart = ref(false); // Track if we need to start flash after auth

// Configurable settings password - loaded from backend
const SETTINGS_PASSWORD = ref(""); // Empty by default

async function loadSettingsPassword() {
  try {
    const response = await fetch("/config/settings_password");
    if (response.ok) {
      const data = await response.json();
      if (data.status === "ok" && data.password !== undefined) {
        SETTINGS_PASSWORD.value = data.password;
        // //console.log("[Frontend] Settings password loaded from backend:", data.password === "" ? "(none)" : "(set)");
      }
    }
  } catch (e) {
    console.warn("[Frontend] Could not load settings password from backend:", e);
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
  // Increase duration for errors to make them more readable
  if (type === "error" && duration < 8000) {
    duration = 8000; // 8 seconds for error messages
  }
  
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
    //console.log("[Frontend] Starting loadSettings...");
    
    // Firefox-compatible timeout implementation
    const fetchPromise = fetch("/config.json");
    const timeoutPromise = new Promise((_, reject) => 
      setTimeout(() => reject(new Error('Request timeout')), 10000)
    );
    
    const r = await Promise.race([fetchPromise, timeoutPromise]);
    
    //console.log(`[Frontend] Response status: ${r.status} ${r.statusText}`);
    //console.log(`[Frontend] Response headers:`, Object.fromEntries(r.headers.entries()));
    
    if (!r.ok) {
      throw new Error(`HTTP ${r.status}: ${r.statusText}`);
    }
    
    // Check content-length header vs actual response
    const contentLength = r.headers.get('content-length');
    //console.log(`[Frontend] config.json - Content-Length: ${contentLength}`);
    
    const text = await r.text();
    //console.log(`[Frontend] config.json - Actual response length: ${text.length}`);
    
    if (contentLength && parseInt(contentLength) !== text.length) {
      console.warn(`[Frontend] Content-Length mismatch! Header: ${contentLength}, Actual: ${text.length}`);
    }
    
    // Clean the response text from potential control characters
    const cleanedText = text.replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '');
    
    const data = JSON.parse(cleanedText);
    config.value = data.config || data;
    refreshKey.value++;
    
    //console.log("[Frontend] config.json loaded successfully");
    // Re-evaluate live content availability after config loads
    checkLiveContent();
  } catch (e) {
    console.error("[Frontend] loadSettings error:", e);
    if (e.name === 'AbortError') {
      notify("Connection timeout: Could not load settings", "error", 8000);
    } else if (e.message.includes('Content-Length')) {
      notify("Server response error: Data transmission interrupted", "error", 8000);
    } else if (e instanceof SyntaxError && e.message.includes('JSON')) {
      // JSON parsing error - provide more helpful message
      notify("JSON parsing error. Please check if password contains special characters.", "error", 10000);
      console.error("[Frontend] JSON parsing failed. Response preview:", text.substring(0, 500));
    } else {
      notify("Fehler: " + e.message, "error", 8000);
    }
  }
}

// Helper: fetch with timeout (Firefox-friendly)
async function fetchWithTimeout(resource, options = {}, timeoutMs = 5000) {
  if (typeof AbortController !== 'undefined') {
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), timeoutMs);
    try {
      const res = await fetch(resource, { ...options, signal: controller.signal });
      return res;
    } finally {
      clearTimeout(id);
    }
  }
  return Promise.race([
    fetch(resource, options),
    new Promise((_, reject) => setTimeout(() => reject(new Error('Request timeout')), timeoutMs))
  ]);
}

async function checkLiveContent() {
  try {
    // 1) Prefer explicit runtime meta definition
    const m = await fetchWithTimeout(`/runtime_meta.json?ts=${Date.now()}`, {}, 4000);
    if (m && m.ok) {
      const meta = await m.json().catch(() => []);
      if (Array.isArray(meta) && meta.length > 0) {
        hasLiveContent.value = true;
        return;
      }
    }
  } catch (_) { /* ignore */ }

  // 2) Fallback: check runtime data has any groups with values
  try {
    const r = await fetchWithTimeout(`/runtime.json?ts=${Date.now()}`, {}, 4000);
    if (r && r.ok) {
      const rt = await r.json().catch(() => ({}));
      // Consider there is live content only if there is at least one non-empty group
      const groups = Object.keys(rt || {});
      const hasContent = groups.some(g => {
        const o = rt[g];
        if (!o || typeof o !== 'object') return false;
        return Object.keys(o).length > 0;
      });
      hasLiveContent.value = !!hasContent;
    } else {
      hasLiveContent.value = false;
    }
  } catch (_) {
    hasLiveContent.value = false;
  }

  // If no live content, switch to settings automatically
  if (!hasLiveContent.value) {
    if (activeTab.value === 'live') {
      // Auto-switch to settings; auto-auth if no password
      if (SETTINGS_PASSWORD.value === "" || settingsAuthenticated.value) {
        settingsAuthenticated.value = true;
        activeTab.value = 'settings';
        loadSettings();
      } else {
        // prompt for settings auth
        switchToSettings();
      }
      notify("No live dashboard defined – opening Settings", "info");
    }
  }
}
function switchToSettings() {
  // If no password required, allow immediate access
  if (SETTINGS_PASSWORD.value === "" || settingsAuthenticated.value) {
    settingsAuthenticated.value = true; // Mark as authenticated if no password
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
  // If no password is set on backend, allow immediate access
  if (SETTINGS_PASSWORD.value === "" || settingsPassword.value === SETTINGS_PASSWORD.value) {
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
      notify(SETTINGS_PASSWORD.value === "" ? "Settings unlocked" : "Settings access granted", "success");
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
    ) {
      // Skip placeholder or empty password fields
      return;
    }
    let v = input.type === "checkbox" ? input.checked : input.value;
    // Encrypt password fields in bulk actions as well
    if (input.dataset.isPassword === "1" && typeof v === 'string' && v.trim() !== '') {
      try {
        v = encryptPassword(v, ENCRYPTION_SALT);
      } catch (e) {
        console.warn('[Security] Password encryption (saveAll) failed, sending plaintext:', e);
      }
    }
    all[cat][key] = v;
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
    let v = input.type === "checkbox" ? input.checked : input.value;
    if (input.dataset.isPassword === "1" && typeof v === 'string' && v.trim() !== '') {
      try {
        v = encryptPassword(v, ENCRYPTION_SALT);
      } catch (e) {
        console.warn('[Security] Password encryption (applyAll) failed, sending plaintext:', e);
      }
    }
    all[cat][key] = v;
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
// Wait until the runtime dashboard is mounted and has reported canFlash (or timeout)
async function waitForFlashReady(timeoutMs = 2000) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (runtimeDashboard.value && canFlash.value) return true;
    await new Promise((r) => setTimeout(r, 120));
  }
  return runtimeDashboard.value && canFlash.value;
}

async function startFlash() {
  // Ensure RuntimeDashboard is mounted (only on 'live' tab)
  if (activeTab.value !== "live") {
    activeTab.value = "live";
    await nextTick();
  }
  // Wait briefly for canFlash to resolve after mount
  const ready = await waitForFlashReady(2000);
  if (!ready) {
    notify("Preparing Flash UI… please try again in a moment", "info");
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
  checkLiveContent();
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
