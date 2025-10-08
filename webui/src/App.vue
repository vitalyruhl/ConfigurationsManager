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
    <!-- Toast notifications -->
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
    <RuntimeDashboard
      v-if="activeTab === 'live'"
      ref="runtimeDashboard"
      :config="config"
      @can-flash-change="handleCanFlashChange"
    />

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

const config = ref({});
const refreshKey = ref(0); // forces re-render of settings tree
const version = ref("");
const refreshing = ref(false);
const activeTab = ref("live");

const runtimeDashboard = ref(null);
const canFlash = ref(false);

// Toast notifications
const toasts = ref([]); // {id, message, type, sticky, ts}
let toastCounter = 0;
const opBusy = ref({}); // per-setting busy map

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
  } else {
    toasts.value.push({ id, message, type, sticky, ts: Date.now() });
  }
  if (!sticky) {
    setTimeout(() => dismissToast(id), duration);
  }
  return id;
}

function dismissToast(id) {
  toasts.value = toasts.value.filter((t) => t.id !== id);
}

function updateToast(id, message, type = "info", duration = 2500) {
  const t = toasts.value.find((toast) => toast.id === id);
  if (!t) return notify(message, type, duration);
  t.message = message;
  t.type = type;
  t.sticky = false;
  setTimeout(() => dismissToast(id), duration);
}

provide("notify", notify);
provide("updateToast", updateToast);
provide("dismissToast", dismissToast);

const rURIComp = encodeURIComponent;

async function loadSettings() {
  try {
    const response = await fetch("/config.json");
    if (!response.ok) throw new Error("HTTP Error");
    const data = await response.json();
    config.value = data.config || data;
    refreshKey.value++;
  } catch (error) {
    notify("Fehler: " + error.message, "error");
  }
}

function switchToSettings() {
  activeTab.value = "settings";
  loadSettings();
}

async function injectVersion() {
  try {
    const response = await fetch("/version");
    if (!response.ok) throw new Error("Version fetch failed");
    const data = await response.text();
    version.value = String(data);
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
  } catch (e) {
    /* ignore theme errors */
  }
}

async function applySingle(category, key, value) {
  const opKey = `${category}.${key}`;
  opBusy.value[opKey] = true;
  const toastId = notify(`Applying: ${opKey} ...`, "info", 7000, true);
  try {
    const response = await fetch(
      `/config/apply?category=${rURIComp(category)}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value }),
      }
    );
    const json = await response.json().catch(() => ({}));
    if (!response.ok || json.status !== "ok") {
      throw new Error(json.reason || "Failed");
    }
    await loadSettings();
    updateToast(toastId, `Applied: ${opKey}`, "success");
  } catch (e) {
    updateToast(toastId, `Apply failed ${opKey}: ${e.message}`, "error");
  } finally {
    delete opBusy.value[opKey];
  }
}

async function saveSingle(category, key, value) {
  const opKey = `${category}.${key}`;
  opBusy.value[opKey] = true;
  const toastId = notify(`Saving: ${opKey} ...`, "info", 7000, true);
  try {
    const response = await fetch(
      `/config/save?category=${rURIComp(category)}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value }),
      }
    );
    const json = await response.json().catch(() => ({}));
    if (!response.ok || json.status !== "ok") {
      throw new Error(json.reason || "Failed");
    }
    await loadSettings();
    updateToast(toastId, `Saved: ${opKey}`, "success");
  } catch (e) {
    updateToast(toastId, `Save failed ${opKey}: ${e.message}`, "error");
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
    const [category, key] = input.name.split(".");
    if (!all[category]) all[category] = {};
    if (
      input.dataset.isPassword === "1" &&
      (!input.value || input.value === "***")
    ) {
      return;
    }
    all[category][key] =
      input.type === "checkbox" ? input.checked : input.value;
  });
  refreshing.value = true;
  try {
    const response = await fetch("/config/save_all", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(all),
    });
    const json = await response.json().catch(() => ({}));
    if (response.ok && json.status === "ok") {
      notify("All settings saved", "success");
      await loadSettings();
    } else {
      notify("Failed to save all settings", "error");
    }
  } catch (error) {
    notify("Error: " + error.message, "error");
  } finally {
    refreshing.value = false;
  }
}

async function applyAll() {
  if (!confirm("Apply all settings?")) return;
  const all = {};
  collectSettingsInputs().forEach((input) => {
    const [category, key] = input.name.split(".");
    if (!all[category]) all[category] = {};
    all[category][key] =
      input.type === "checkbox" ? input.checked : input.value;
  });
  refreshing.value = true;
  try {
    const response = await fetch("/config/apply_all", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(all),
    });
    const json = await response.json().catch(() => ({}));
    if (response.ok && json.status === "ok") {
      notify("All settings applied", "success");
      await loadSettings();
    } else {
      notify("Failed to apply all settings", "error");
    }
  } catch (error) {
    notify("Error: " + error.message, "error");
  } finally {
    refreshing.value = false;
  }
}

async function resetDefaults() {
  if (!confirm("Reset to defaults?")) return;
  try {
    const response = await fetch("/config/reset", { method: "POST" });
    if (response.ok) {
      notify("Reset to defaults", "success");
      setTimeout(() => location.reload(), 1000);
    }
  } catch (error) {
    notify("Error: " + error.message, "error");
  }
}

function rebootDevice() {
  if (!confirm("Reboot device?")) return;
  fetch("/config/reboot", { method: "POST" })
    .then((response) => (response.ok ? notify("Rebooting...", "info") : null))
    .catch((error) => notify("Error: " + error.message, "error"));
}

function startFlash() {
  if (!canFlash.value) {
    notify("OTA is disabled", "error");
    return;
  }
  runtimeDashboard.value?.startFlash();
}

function handleCanFlashChange(val) {
  canFlash.value = !!val;
}

onMounted(() => {
  loadUserTheme();
  loadSettings();
  injectVersion();
});
</script>

<style scoped>
body {
  font-family: Arial, sans-serif;
  margin: 1rem;
  background-color: #f0f0f0;
}
h3 {
  color: #000;
  margin: 0.5rem auto;
}
.tabs {
  display: flex;
  gap: 0.5rem;
  justify-content: center;
  margin-bottom: 1rem;
}
.tabs button {
  background: #ddd;
  color: #222;
  width: auto;
  padding: 0.5rem 1rem;
  color: whitesmoke;
  background-color: slategray;
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
  background-color: gainsboro;
}
.hidden-file-input {
  display: none;
}
.live-cards {
  display: flex;
  flex-wrap: wrap;
  gap: 1rem;
  justify-content: center;
}
.live-controls {
  display: flex;
  justify-content: flex-end;
  align-items: center;
  margin: 0 0 0.5rem;
}
.bool-toggle {
  display: flex;
  align-items: center;
  gap: 0.4rem;
  font-size: 0.85rem;
  color: #555;
  cursor: pointer;
}
.bool-toggle input {
  cursor: pointer;
  accent-color: #3498db;
}
.card {
  background: #fff;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.15);
  /* padding: 0.75rem 1rem; */
  padding: 0 1rem 1rem 1rem;
  border-radius: 8px;
  min-width: 15rem;
}
.tbl {
  display: flex;
  flex-direction: column;
  gap: 0.15rem;
}
.live-cards .card .rw {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto auto;
  align-items: center;
  gap: 0.35rem;
  margin: 0.2rem 0;
  font-size: 0.9rem;
}
.lab {
  font-weight: 600;
  text-align: left;
  display: flex;
  align-items: center;
  gap: 0.35rem;
}
.val {
  text-align: right;
  font-variant-numeric: tabular-nums;
}
.un {
  text-align: left;
  font-size: 0.8rem;
  color: #000;
  min-width: 2.5ch;
  white-space: nowrap;
  font-weight: 600;
}
.rw.sev-warn {
  color: #b58900;
  font-weight: 600;
}
.rw.sev-alarm {
  color: #d00000;
  font-weight: 700;
  animation: blink 1.6s linear infinite;
}

/* Slider specific styling */
.rw.sl {
  border: 1px solid #e0e0e0;
  padding: 0.35rem 0.5rem;
  border-radius: 6px;
  background: #fafafa;
  box-shadow: inset 0 0 0 1px #ffffff, 0 1px 2px rgba(0, 0, 0, 0.05);
  position: relative;
}
.rw.sl:hover {
  border-color: #d0d0d0;
  background: #f5f5f5;
}
.rw.sl .sw {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  min-width: 10rem;
}
.rw.sl input[type="range"] {
  width: 8rem;
  accent-color: #ff9800;
  cursor: pointer;
}
.rw.sl .sv {
  font-weight: 600;
  min-width: 3.2ch;
  text-align: right;
  font-variant-numeric: tabular-nums;
}
.rw.sl .sb {
  background: #1976d2;
  color: #fff;
  border: none;
  padding: 0.25rem 0.6rem;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.72rem;
  font-weight: 600;
  letter-spacing: 0.5px;
  text-transform: uppercase;
}
.rw.sl .sb:hover {
  background: #125a9f;
}
.rw.sl .num-input {
  width: 5.5rem;
  padding: 0.2rem 0.3rem;
  border: 1px solid #bbb;
  border-radius: 4px;
  font-size: 0.75rem;
  background: #fff;
}
.rw.sl .num-input:focus {
  outline: 1px solid #1976d2;
}
.rw.act .btn.on {
  background: #2e7d32;
  color: #fff;
}
.rw.act .btn.on:hover {
  background: #256628;
}
@keyframes blink {
  50% {
    opacity: 0.3;
  }
}
.rw.br .lab {
  display: flex;
  align-items: center;
  gap: 0.4rem;
}
.rw.br[data-state="on"] .bd {
  background: #2ecc71;
  border: none;
  box-shadow: 0 0 2px rgba(0, 0, 0, 0.4);
}
.rw.br[data-state="off"] .bd {
  background: #888;
  border: 1px solid #000;
  box-shadow: 0 0 2px rgba(0, 0, 0, 0.4);
}
.rw.br[data-state="alarm"] .bd {
  background: #d00000;
  border: none;
  box-shadow: 0 0 4px rgba(208, 0, 0, 0.7);
  animation: blink 1.6s linear infinite;
}
.rw.cr {
  grid-template-columns: minmax(0, 1fr) auto !important;
}
.rw.cr .val {
  justify-self: end;
}
.bd {
  width: 0.85rem;
  height: 0.85rem;
  border-radius: 50%;
  display: inline-block;
}
.dv {
  border: 0;
  border-top: 1px solid #ccc;
  margin: 0.6rem 0 0.4rem;
  position: relative;
}
.dv:before {
  content: attr(data-label);
  position: absolute;
  top: -0.7rem;
  left: 0.4rem;
  background: #fff;
  padding: 0 0.3rem;
  font-size: 0.65rem;
  color: #999;
  letter-spacing: 0.05em;
}
.str {
  color: #333;
}
.str .val {
  text-align: left;
  font-weight: 400;
}
.ta-center {
  text-align: center;
}
.uptime {
  font-size: 0.85rem;
  color: #555;
  margin: 0.3rem 0 0.15rem;
  padding: 0.25rem 0.4rem 0.15rem;
  text-align: center;
}
/* Slow down blink for readability */
.live-status {
  text-align: center;
  margin-top: 1rem;
  font-size: 0.85rem;
  color: #555;
}
h1 {
  color: #3498db;
  margin-top: 0.2rem;
  border-bottom: 2px solid #3498db;
  padding-bottom: 0.2rem;
  font-size: 1.8rem;
  font-weight: bold;
  min-width: 120px;
}
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
  .action-buttons {
    flex-direction: row;
  }
  button {
    width: auto;
  }
}
/* Toast notifications */
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
.top-margin {
  margin-top: 2rem !important;
}
/* Android-Style Toggle Switch */
.switch {
  display: inline-block;
  height: 34px;
  position: relative;
  width: 30px;
}

.switch input {
  display: none;
}

.slider {
  background-color: #ccc;
  bottom: 0;
  cursor: pointer;
  left: 1.2rem;
  position: absolute;
  right: 0;
  top: 0;
  transition: 0.4s;
  max-width: 4em;
}

.slider:before {
  background-color: #fff;
  bottom: 4px;
  content: "";
  height: 26px;
  left: 4px;
  position: absolute;
  transition: 0.4s;
  width: 26px;
}

input:checked + .slider {
  background-color: #66bb6a;
}

input:checked + .slider:before {
  transform: translateX(26px);
}

.slider.round {
  border-radius: 34px;
}

.slider.round:before {
  border-radius: 50%;
}
/* end of toggle switch */
</style>
