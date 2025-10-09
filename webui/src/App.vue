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
const refreshKey = ref(0);
const version = ref("");
const refreshing = ref(false);
const activeTab = ref("live");
const runtimeDashboard = ref(null);
const canFlash = ref(false);
const toasts = ref([]); // {id,message,type,sticky,ts}
let toastCounter = 0;
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
    const r = await fetch("/config.json");
    if (!r.ok) throw new Error("HTTP Error");
    const data = await r.json();
    config.value = data.config || data;
    refreshKey.value++;
  } catch (e) {
    notify("Fehler: " + e.message, "error");
  }
}
function switchToSettings() {
  activeTab.value = "settings";
  loadSettings();
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
    const r = await fetch(
      `/config/apply?category=${rURIComp(category)}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value }),
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
    const r = await fetch(
      `/config/save?category=${rURIComp(category)}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value }),
      }
    );
    const json = await r.json().catch(() => ({}));
    if (!r.ok || json.status !== "ok") throw new Error(json.reason || "Failed");
    await loadSettings();
    updateToast(tid, `Saved: ${opKey}`, "success");
  } catch (e) {
    updateToast(tid, `Save failed ${opKey}: ${e.message}`, "error");
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
  fetch("/config/reboot", { method: "POST" })
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
  runtimeDashboard.value?.startFlash();
}
function handleCanFlashChange(v) {
  canFlash.value = !!v;
}
onMounted(() => {
  loadUserTheme();
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
</style>
