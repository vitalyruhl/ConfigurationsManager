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
    <input
      ref="otaFileInput"
      type="file"
      accept=".bin,.bin.gz"
      class="hidden-file-input"
      @change="onFlashFileSelected"
    />

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

    <section v-if="activeTab === 'live'" class="live-view">
      <div class="live-cards">
        <div class="card" v-for="group in displayRuntimeGroups" :key="group.name">
          <h3>{{ group.title }}</h3>

          <div class="tbl">
            <template v-for="f in group.fields" :key="f.key">
              <!-- Divider rendering -->
              <hr v-if="f.isDivider" class="dv" :data-label="f.label" />
              <!-- Interactive Button -->
              <div v-else-if="f.isButton" class="rw act" :data-group="group.name" :data-key="f.key">
                <span class="lab">{{ f.label }}</span>
                <span class="val">
                  <button class="btn" @click="triggerRuntimeButton(group.name, f.key)">{{ f.label }}</button>
                </span>
                <span class="un"></span>
              </div>
              <!-- Stateful Button -->
              <div v-else-if="f.isStateButton" class="rw act" :data-group="group.name" :data-key="f.key">
                <span class="lab">{{ f.label }}</span>
                <span class="val">
                  <button class="btn" :class="{'on': runtime[group.name] && runtime[group.name][f.key]}" @click="onStateButton(group.name,f)">{{ f.label }}: {{ runtime[group.name] && runtime[group.name][f.key] ? 'ON' : 'OFF' }}</button>
                </span>
                <span class="un"></span>
              </div>
              <!-- Int Slider (explicit set) -->
              <div v-else-if="f.isIntSlider" class="rw sl" :data-group="group.name" :data-key="f.key">
                <span class="lab">{{ f.label }}</span>
                <span class="val sw">
                  <input type="range" :min="f.min" :max="f.max" step="1" :value="tempSliderValue(group.name,f)" @input="onIntSliderLocal(group.name,f,$event)" />
                  <span class="sv">{{ tempSliderValue(group.name,f) }}</span>
                  <button class="sb" @click="commitIntSlider(group.name,f)">Set</button>
                </span>
                <span class="un"></span>
              </div>
              <!-- Float Slider (explicit set) -->
              <div v-else-if="f.isFloatSlider" class="rw sl" :data-group="group.name" :data-key="f.key">
                <span class="lab">{{ f.label }}</span>
                <span class="val sw">
                  <input type="range" :min="f.min" :max="f.max" :step="floatSliderStep(f)" :value="tempSliderValue(group.name,f)" @input="onFloatSliderLocal(group.name,f,$event)" />
                  <span class="sv">{{ formatTempFloat(group.name,f) }}</span>
                  <button class="sb" @click="commitFloatSlider(group.name,f)">Set</button>
                </span>
                <span class="un"></span>
              </div>
              <!-- Int Input -->
              <div v-else-if="f.isIntInput" class="rw sl" :data-group="group.name" :data-key="f.key">
                <span class="lab">{{ f.label }}</span>
                <span class="val sw">
                  <input type="number" :min="f.min" :max="f.max" step="1" :value="tempInputValue(group.name,f)" @input="onIntInputLocal(group.name,f,$event)" class="num-input" />
                  <button class="sb" @click="commitIntInput(group.name,f)">Set</button>
                </span>
                <span class="un"></span>
              </div>
              <!-- Float Input -->
              <div v-else-if="f.isFloatInput" class="rw sl" :data-group="group.name" :data-key="f.key">
                <span class="lab">{{ f.label }}</span>
                <span class="val sw">
                  <input type="number" :min="f.min" :max="f.max" :step="floatSliderStep(f)" :value="tempInputValue(group.name,f)" @input="onFloatInputLocal(group.name,f,$event)" class="num-input" />
                  <button class="sb" @click="commitFloatInput(group.name,f)">Set</button>
                </span>
                <span class="un"></span>
              </div>
              <!-- Interactive Checkbox (toggle) -->
              <div v-else-if="f.isCheckbox" class="rw tg" :data-group="group.name" :data-key="f.key">
                <span class="lab">{{ f.label }}</span>
                <span class="val">
                  <label class="switch">
                    <input type="checkbox" :checked="runtime[group.name] && runtime[group.name][f.key]" @change="onRuntimeCheckbox(group.name, f.key, $event.target.checked)" />
                    <span class="slider round"></span>
                  </label>
                </span>
                <span class="un"></span>
              </div>
              <!-- Pure string (static or dynamic) -->
              <div v-else-if="f.isString" class="rw str">
                <span class="lab">{{ f.label }}</span>
                <span class="val">
                  <template v-if="f.staticValue">{{ f.staticValue }}</template>
                  <template
                    v-else-if="
                      runtime[group.name] &&
                      runtime[group.name][f.key] !== undefined
                    "
                    >{{ runtime[group.name][f.key] }}</template
                  >
                  <template v-else>—</template>
                </span>
                  <span class="un"></span>
              </div>
              <!-- Boolean / Numeric -->
              <div
                v-else-if="
                  runtime[group.name] &&
                  runtime[group.name][f.key] !== undefined
                "
                  :class='["rw", valueClasses(runtime[group.name][f.key], f)]'
                :data-group="group.name"
                :data-key="f.key"
                  :data-type="f.isBool ? 'bool' : f.isString ? 'string' : 'numeric'"
                :data-state="
                  f.isBool ? boolState(runtime[group.name][f.key], f) : null
                "
              >
                <template v-if="f.isBool">
                  <span
                      class="lab bl"
                    v-if="fieldVisible(f, 'label')"
                    :style="fieldCss(f, 'label')"
                  >
                    <span
                        class="bd"
                      v-if="boolDotVisible(runtime[group.name][f.key], f)"
                      :style="boolDotStyle(runtime[group.name][f.key], f)"
                    ></span>
                    {{ f.label }}
                  </span>
                    <span class="lab bl" v-else></span>
                  <span
                      class="val"
                    v-if="showBoolStateText && fieldVisible(f, 'state')"
                    :style="fieldCss(f, 'state')"
                    >{{ formatBool(runtime[group.name][f.key], f) }}</span
                  >
                    <span class="val" v-else></span>
                  <span
                      class="un"
                    v-if="fieldVisible(f, 'unit', false)"
                    :style="fieldCss(f, 'unit')"
                  ></span>
                    <span class="un" v-else></span>
                </template>
                <template v-else>
                  <span
                      class="lab"
                    v-if="fieldVisible(f, 'label')"
                    :style="fieldCss(f, 'label')"
                    >{{ f.label }}</span
                  >
                    <span class="lab" v-else></span>
                  <span
                      class="val"
                    v-if="fieldVisible(f, 'values')"
                    :style="fieldCss(f, 'values')"
                    >{{ formatValue(runtime[group.name][f.key], f) }}</span
                  >
                    <span class="val" v-else></span>
                  <span
                      class="un"
                    v-if="fieldVisible(f, 'unit', !!f.unit)"
                    :style="fieldCss(f, 'unit')"
                    >{{ f.unit }}</span
                  >
                    <span class="un" v-else></span>
                </template>
              </div>
            </template>
          </div>
          <hr
              v-if="group.name === 'system' && runtime.uptime !== undefined"
              class="dv"
          />
          <p
            v-if="group.name === 'system' && runtime.uptime !== undefined"
            class="uptime"
          >
            Uptime: {{ Math.floor((runtime.uptime || 0) / 1000) }} s
          </p>
          <p
            v-if="group.name === 'system' && runtime.system && runtime.system.loopAvg !== undefined"
            class="uptime"
          >
            Loop Avg: {{ typeof runtime.system.loopAvg === 'number' ? runtime.system.loopAvg.toFixed(2) : runtime.system.loopAvg }} ms
          </p>
          <div v-if="group.name === 'system' && runtime.uptime !== undefined" class="tbl">
            <div class="rw cr">
              <span class="lab">Show state text</span>
              <label class="switch val">
                <input type="checkbox" v-model="showBoolStateText" class="switch" />
                <span class="slider round"></span>
              </label>
            </div>
          </div>
        </div>
      </div>
      <div class="live-status">
        Mode: {{ wsConnected ? "WebSocket" : "Polling" }}
      </div>
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
import { ref, onMounted, onBeforeUnmount, computed, watch } from "vue";
import Category from "./components/Category.vue";

const config = ref({});
const refreshKey = ref(0); // forces re-render of settings tree
const version = ref("");
const refreshing = ref(false);
const activeTab = ref("live");
const flashing = ref(false);
const showBoolStateText = ref(false);
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
const rURIComp = encodeURIComponent; //reduce encodeURIComponent -> rURIComp - we save lot of bytes.
// Runtime metadata state for dynamic grouping
const runtimeMeta = ref([]); // raw metadata array from /runtime_meta.json
const runtimeGroups = ref([]); // transformed groups -> [{ name, title, fields:[{key,label,unit,precision}]}]
const displayRuntimeGroups = computed(() =>
  runtimeGroups.value.filter((group) => groupHasVisibleContent(group))
);

function normalizeStyle(style) {
  if (!style || typeof style !== "object") return null;
  const normalized = {};
  Object.entries(style).forEach(([ruleKey, ruleValue]) => {
    if (!ruleValue || typeof ruleValue !== "object") return;
    const { visible, ...rest } = ruleValue;
    const cssProps = { ...rest };
    normalized[ruleKey] = {
      css: cssProps,
      visible: Object.prototype.hasOwnProperty.call(ruleValue, "visible")
        ? !!visible
        : undefined,
    };
  });
  return Object.keys(normalized).length ? normalized : null;
}

function ensureStyleRules(meta) {
  if (!meta) return null;
  if (meta.styleRules) return meta.styleRules;
  if (meta.style) {
    const normalized = normalizeStyle(meta.style);
    if (normalized) {
      meta.styleRules = normalized;
      return meta.styleRules;
    }
  }
  return null;
}

watch(showBoolStateText, (val) => {
  try {
    if (typeof window !== "undefined" && window.localStorage) {
      window.localStorage.setItem("cm_showBoolStateText", val ? "1" : "0");
    }
  } catch (e) {
    /* ignore */
  }
});

const canFlash = computed(() => {
  const systemConfig = config.value?.System;
  let allow = false;
  if (
    systemConfig &&
    systemConfig.OTAEn &&
    typeof systemConfig.OTAEn.value !== "undefined"
  ) {
    allow = !!systemConfig.OTAEn.value;
  } else if (
    runtime.value.system &&
    typeof runtime.value.system.allowOTA !== "undefined"
  ) {
    allow = !!runtime.value.system.allowOTA;
  }
  return allow && !flashing.value;
});

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
  if (!sticky) {
    setTimeout(() => dismissToast(id), duration);
  }
  return id;
}
function dismissToast(id) {
  toasts.value = toasts.value.filter((t) => t.id !== id);
}
function updateToast(id, message, type = "info", duration = 2500) {
  const t = toasts.value.find((t) => t.id === id);
  if (!t) return notify(message, type, duration);
  t.message = message;
  t.type = type;
  t.sticky = false;
  setTimeout(() => dismissToast(id), duration);
}

async function loadSettings() {
  try {
    const response = await fetch("/config.json");
    if (!response.ok) throw new Error("HTTP Error");
    const data = await response.json();
    config.value = data.config || data;
    refreshKey.value++; // force re-mount
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
    version.value = "" + data;
  } catch (e) {
    version.value = "";
  }
}

async function applySingle(category, key, value) {
  const opKey = `${category}.${key}`;
  opBusy.value[opKey] = true;
  const toastId = notify(`Applying: ${opKey} ...`, "info", 7000, true);
  try {
    const response = await fetch(
      `/config/apply?category=${rURIComp(
        category
      )}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value }),
      }
    );
    const json = await response.json().catch(() => ({}));
    if (!response.ok || json.status !== "ok")
      throw new Error(json.reason || "Failed");
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
      `/config/save?category=${rURIComp(
        category
      )}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value }),
      }
    );
    const json = await response.json().catch(() => ({}));
    if (!response.ok || json.status !== "ok")
      throw new Error(json.reason || "Failed");
    await loadSettings();
    updateToast(toastId, `Saved: ${opKey}`, "success");
  } catch (e) {
    updateToast(toastId, `Save failed ${opKey}: ${e.message}`, "error");
  } finally {
    delete opBusy.value[opKey];
  }
}

async function saveAll() {
  if (!confirm("Save all settings?")) return;
  const all = {};
  document.querySelectorAll("input").forEach((input) => {
    const [category, key] = input.name.split(".");
    if (!all[category]) all[category] = {};
    if (
      input.dataset.isPassword === "1" &&
      (!input.value || input.value === "***")
    )
      return;
    all[category][key] =
      input.type === "checkbox" ? input.checked : input.value;
  });
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
  }
}

async function applyAll() {
  if (!confirm("Apply all settings?")) return;
  const all = {};
  document.querySelectorAll("input").forEach((input) => {
    const [category, key] = input.name.split(".");
    if (!all[category]) all[category] = {};
    all[category][key] =
      input.type === "checkbox" ? input.checked : input.value;
  });
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
  if (!otaFileInput.value) {
    notify("Browser file input not ready.", "error");
    return;
  }
  otaFileInput.value.value = "";
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
    notify("OTA is disabled", "error");
    return;
  }
  if (file.size === 0) {
    notify("Selected file is empty.", "error");
    otaFileInput.value.value = "";
    return;
  }

  let password = window.prompt("Enter OTA password", "");
  if (password === null) {
    otaFileInput.value.value = "";
    return; // user cancelled
  }
  password = password.trim();

  const headers = new Headers();
  if (password.length) {
    headers.append("X-OTA-PASSWORD", password);
  }

  const form = new FormData();
  form.append("firmware", file, file.name);

  flashing.value = true;
  const toastId = notify(`Uploading ${file.name}...`, "info", 15000, true);
  try {
    const response = await fetch("/ota_update", {
      method: "POST",
      headers,
      body: form,
    });

    let payload = {};
    const text = await response.text();
    try {
      payload = text ? JSON.parse(text) : {};
    } catch (e) {
      payload = { status: "error", reason: text || "invalid_response" };
    }

    if (response.status === 401) {
      updateToast(toastId, "Unauthorized: wrong OTA password.", "error", 6000);
    } else if (response.status === 403 || payload.reason === "ota_disabled") {
      updateToast(toastId, "OTA disabled", "error", 6000);
    } else if (!response.ok || payload.status !== "ok") {
      const reason = payload.reason || response.statusText || "Upload failed";
      updateToast(toastId, `Flash failed: ${reason}`, "error", 6000);
    } else {
      updateToast(toastId, "Flash done!", "success", 9000);
      notify("Flash done!", "success", 6000);
      notify("Waiting for reboot...", "info", 8000);
    }
  } catch (error) {
    updateToast(toastId, `Flash failed: ${error.message}`, "error", 6000);
  } finally {
    flashing.value = false;
    if (otaFileInput.value) otaFileInput.value.value = "";
  }
}

onMounted(() => {
  try {
    if (typeof window !== "undefined" && window.localStorage) {
      const stored = window.localStorage.getItem("cm_showBoolStateText");
      if (stored === "1") {
        showBoolStateText.value = true;
      } else if (stored === "0") {
        showBoolStateText.value = false;
      }
    }
  } catch (e) {
    /* ignore */
  }

  // Load optional user theme CSS (global override)
  try {
    fetch("/user_theme.css?ts=" + Date.now())
      .then((r) => (r.status === 200 ? r.text() : ""))
      .then((css) => {
        if (css && css.trim().length) {
          const existing = document.getElementById("user-theme-css");
          if (existing) existing.remove();
          const el = document.createElement("style");
          el.id = "user-theme-css";
          el.textContent = css;
          document.head.appendChild(el);
        }
      })
      .catch(() => {});
  } catch (e) {
    /* ignore */
  }

  loadSettings();
  injectVersion();
  initLive();
  fetchRuntime();
  fetchRuntimeMeta();
});

onBeforeUnmount(() => {
  if (pollTimer) clearInterval(pollTimer);
  if (ws) ws.close();
});

function initLive() {
  const proto = location.protocol === "https:" ? "wss://" : "ws://";
  const url = proto + location.host + "/ws";
  startWebSocket(url);
}
let wsRetry = 0;
function startWebSocket(url){
  try {
    if(ws){ try { ws.close(); } catch(e){} }
    ws = new WebSocket(url);
    ws.onopen = () => {
      wsConnected.value = true;
      wsRetry = 0;
      setTimeout(()=>{ if(!runtime.value.uptime) fetchRuntime(); }, 300);
    };
    ws.onclose = () => { wsConnected.value = false; scheduleWsReconnect(url); };
    ws.onerror = () => { wsConnected.value = false; scheduleWsReconnect(url); };
    ws.onmessage = (ev) => { try { runtime.value = JSON.parse(ev.data); } catch(e){} };
  } catch(e){ scheduleWsReconnect(url); }
}
function scheduleWsReconnect(url){
  const delay = Math.min(5000, 300 + wsRetry * wsRetry * 200);
  wsRetry++;
  setTimeout(()=>{ startWebSocket(url); }, delay);
  if(!pollTimer){ fallbackPolling(); }
}

async function fetchRuntime() {
  try {
    const r = await fetch("/runtime.json?ts=" + Date.now());
    if (!r.ok) return;
    runtime.value = await r.json();
    buildRuntimeGroups();
  } catch (e) {
    /* ignore */
  }
}

async function fetchRuntimeMeta() {
  try {
    const r = await fetch("/runtime_meta.json?ts=" + Date.now());
    if (!r.ok) return;
    runtimeMeta.value = await r.json();
    buildRuntimeGroups();
  } catch (e) {
    /* ignore */
  }
}

function buildRuntimeGroups() {
  if (runtimeMeta.value.length) {
    const grouped = {};
    for (const m of runtimeMeta.value) {
      if (!grouped[m.group])
        grouped[m.group] = {
          name: m.group,
          title: capitalize(m.group),
          fields: [],
        };
      grouped[m.group].fields.push({
        key: m.key,
        label: m.label,
        unit: m.unit,
        precision: m.precision,
        warnMin: m.warnMin,
        warnMax: m.warnMax,
        alarmMin: m.alarmMin,
        alarmMax: m.alarmMax,
        isBool: m.isBool,
    isButton: m.isButton || false,
    isStateButton: m.isStateButton || false,
    isIntSlider: m.isIntSlider || false,
    isFloatSlider: m.isFloatSlider || false,
    isCheckbox: m.isCheckbox || false,
    card: m.card || null,
    min: m.min,
    max: m.max,
    init: m.init,
  isIntInput: m.isIntInput || false,
  isFloatInput: m.isFloatInput || false,
        boolAlarmValue:
          typeof m.boolAlarmValue === "boolean"
            ? !!m.boolAlarmValue
            : undefined,
        isString: m.isString || false,
        isDivider: m.isDivider || false,
        staticValue: m.staticValue || "",
        order: m.order !== undefined ? m.order : 100,
        style: m.style || null,
        styleRules: normalizeStyle(m.style || null),
      });
    }

    // Augment: add any runtime keys that appeared after meta snapshot (e.g., new firmware fields like loopAvg)
    try {
      const sys = runtime.value.system || {};
      if (grouped.system) {
        const existingKeys = new Set(grouped.system.fields.map(f => f.key));
        Object.keys(sys).forEach(k => {
          if (!existingKeys.has(k)) {
            grouped.system.fields.push({
              key: k,
              label: capitalize(k),
              unit: k === 'freeHeap' ? 'KB' : (k === 'loopAvg' ? 'ms' : ''),
              precision: (k === 'loopAvg') ? 2 : 0,
              order: 999, // appended at end when no meta info
              isBool: typeof sys[k] === 'boolean',
              isString: typeof sys[k] === 'string',
              isButton:false,
              isCheckbox:false,
              isDivider: false,
              staticValue: '',
              style: null,
              styleRules: null
            });
          }
        });
      } else if (Object.keys(sys).length) {
        grouped.system = { name:'system', title:'System', fields: Object.keys(sys).map(k=>({
          key:k,
          label:capitalize(k),
          unit: k === 'freeHeap' ? 'KB' : (k === 'loopAvg' ? 'ms' : ''),
          precision: (k === 'loopAvg') ? 2 : 0,
          order: 999,
          isBool: typeof sys[k] === 'boolean',
          isString: typeof sys[k] === 'string',
          isButton:false,
          isCheckbox:false,
          isDivider: false,
          staticValue: '',
          style:null,
          styleRules:null
        })) };
      }
    } catch(e) { /* ignore augmentation errors */ }

    runtimeGroups.value = Object.values(grouped).map((g) => {
      g.fields.sort((a, b) => {
        if (a.isDivider && b.isDivider) return a.order - b.order;
        if (a.order === b.order) return a.label.localeCompare(b.label);
        return a.order - b.order;
      });
      return g;
    });

    // Extract custom control cards (fields with card property) into standalone groups
    const extraCards = {};
    for (const g of runtimeGroups.value) {
      g.fields = g.fields.filter(f => {
        if (f.card) {
          if(!extraCards[f.card]) extraCards[f.card] = { name: f.card, title: capitalize(f.card), fields: [] };
          extraCards[f.card].fields.push(f);
          return false; // remove from original group
        }
        return true;
      });
    }
    for (const cardName in extraCards) {
      // sort inside card
      extraCards[cardName].fields.sort((a,b)=>{ if(a.order===b.order) return a.label.localeCompare(b.label); return a.order-b.order; });
      runtimeGroups.value.push(extraCards[cardName]);
    }
    return;
  }
  const fallback = [];
  if (runtime.value.sensors) {
    fallback.push({
      name: "sensors",
      title: "Sensors",
      fields: Object.keys(runtime.value.sensors).map((k) => ({
        key: k,
        label: capitalize(k),
        unit: "",
        precision:
          k.toLowerCase().includes("temp") || k.toLowerCase().includes("dew")
            ? 1
            : 0,
      })),
    });
  }
  if (runtime.value.system) {
    fallback.push({
      name: "system",
      title: "System",
      fields: Object.keys(runtime.value.system).map((k) => ({
        key: k,
        label: capitalize(k),
        unit: "",
        precision: 0,
      })),
    });
  }
  runtimeGroups.value = fallback;
}

function groupHasVisibleContent(group) {
  if (!group || !Array.isArray(group.fields) || group.fields.length === 0) {
    return false;
  }
  return group.fields.some((field) => fieldHasVisibleContent(group.name, field));
}

function fieldHasVisibleContent(groupName, field) {
  if (!field || field.isDivider) {
    return false;
  }
  if (isInteractiveField(field)) {
    return true;
  }
  if (field.isString) {
    if (field.staticValue && String(field.staticValue).trim().length) {
      return true;
    }
    return runtimeHasValue(groupName, field.key);
  }
  return runtimeHasValue(groupName, field.key);
}

function isInteractiveField(field) {
  return !!(
    field.isButton ||
    field.isStateButton ||
    field.isIntSlider ||
    field.isFloatSlider ||
    field.isCheckbox ||
    field.isIntInput ||
    field.isFloatInput
  );
}

function runtimeHasValue(groupName, key) {
  const groupData = runtime.value[groupName];
  if (groupData && groupData[key] !== undefined && groupData[key] !== null) {
    return true;
  }
  return false;
}

async function triggerRuntimeButton(group, key){
  try {
  const res = await fetch(`/runtime_action/button?group=${rURIComp(group)}&key=${rURIComp(key)}`, { method:'POST'});
    if(!res.ok){ notify(`Button failed: ${group}/${key}`,'error'); return; }
    notify(`Button: ${key}`,'success',1500);
    // refresh runtime quickly to reflect any side effects
    fetchRuntime();
  } catch(e){ notify(`Button error: ${e.message}`,'error'); }
}
async function onStateButton(group,f){
  const cur = runtime.value[group] && runtime.value[group][f.key];
  const next = !cur;
  try {
  const res = await fetch(`/runtime_action/state_button?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${next?'true':'false'}`, {method:'POST'});
    if(!res.ok){ notify(`State btn failed: ${f.key}`,'error'); return; }
    if(!runtime.value[group]) runtime.value[group] = {};
    runtime.value[group][f.key] = next;
    notify(`${f.key}: ${next?'ON':'OFF'}`,'info',1200);
  } catch(e){ notify(`State btn error: ${e.message}`,'error'); }
}
let intSliderDebounce = {};
function onIntSlider(group,f,ev){
  const val = parseInt(ev.target.value,10);
  if(!runtime.value[group]) runtime.value[group] = {};
  runtime.value[group][f.key] = val;
  if(intSliderDebounce[f.key]) clearTimeout(intSliderDebounce[f.key]);
  intSliderDebounce[f.key] = setTimeout(async ()=>{
  try { await fetch(`/runtime_action/int_slider?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${val}`, {method:'POST'}); }
    catch(e){ notify(`Int slider error: ${e.message}`,'error'); }
  },150);
}
let floatSliderDebounce = {};
function onFloatSlider(group,f,ev){
  const val = parseFloat(ev.target.value);
  if(!runtime.value[group]) runtime.value[group] = {};
  runtime.value[group][f.key] = val;
  if(floatSliderDebounce[f.key]) clearTimeout(floatSliderDebounce[f.key]);
  floatSliderDebounce[f.key] = setTimeout(async ()=>{
  try { await fetch(`/runtime_action/float_slider?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${val}`, {method:'POST'}); }
    catch(e){ notify(`Float slider error: ${e.message}`,'error'); }
  },180);
}
// --- New explicit-control helper state ---
const tempControlCache = {}; // keyed by group.key -> value
function keyFor(group,f){ return group+"::"+f.key; }
function currentRuntimeValue(group,f){
  if(runtime.value[group] && runtime.value[group][f.key] !== undefined) return runtime.value[group][f.key];
  return f.init !== undefined ? f.init : 0;
}
function tempSliderValue(group,f){
  const k = keyFor(group,f);
  if(tempControlCache[k] !== undefined) return tempControlCache[k];
  return currentRuntimeValue(group,f);
}
function onIntSliderLocal(group,f,ev){ tempControlCache[keyFor(group,f)] = parseInt(ev.target.value,10); }
function onFloatSliderLocal(group,f,ev){ tempControlCache[keyFor(group,f)] = parseFloat(ev.target.value); }
function formatTempFloat(group,f){ const v = parseFloat(tempSliderValue(group,f)); const p = f.precision!==undefined?f.precision:2; return isNaN(v)?'-':v.toFixed(p); }
async function commitIntSlider(group,f){ const val = tempSliderValue(group,f); await sendInt(group,f,val); }
async function commitFloatSlider(group,f){ const val = tempSliderValue(group,f); await sendFloat(group,f,val); }

// Inputs
function tempInputValue(group,f){ return tempSliderValue(group,f); }
function onIntInputLocal(group,f,ev){ tempControlCache[keyFor(group,f)] = parseInt(ev.target.value,10); }
function onFloatInputLocal(group,f,ev){ tempControlCache[keyFor(group,f)] = parseFloat(ev.target.value); }
async function commitIntInput(group,f){ const val = tempInputValue(group,f); await sendInt(group,f,val); }
async function commitFloatInput(group,f){ const val = tempInputValue(group,f); await sendFloat(group,f,val); }

async function sendInt(group,f,val){
  try { const r = await fetch(`/runtime_action/int_slider?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${val}`, {method:'POST'}); if(!r.ok) notify(`Set failed: ${f.key}`,'error'); else { if(!runtime.value[group]) runtime.value[group]={}; runtime.value[group][f.key]=val; notify(`${f.key}=${val}`,'success',1200); } }
  catch(e){ notify(`Set error ${f.key}: ${e.message}`,'error'); }
}
async function sendFloat(group,f,val){
  try { const r = await fetch(`/runtime_action/float_slider?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${val}`, {method:'POST'}); if(!r.ok) notify(`Set failed: ${f.key}`,'error'); else { if(!runtime.value[group]) runtime.value[group]={}; runtime.value[group][f.key]=val; notify(`${f.key}=${val}`,'success',1200); } }
  catch(e){ notify(`Set error ${f.key}: ${e.message}`,'error'); }
}
function floatSliderStep(f){
  if(f.precision && f.precision>0){ return 1/Math.pow(10,f.precision); }
  return 0.1;
}
function formatFloatDisplay(group,f){
  const v = runtime.value[group] && runtime.value[group][f.key];
  if(v===undefined || v===null) return f.init !== undefined ? f.init : 0;
  const p = f.precision!==undefined? f.precision:2;
  return typeof v === 'number' ? v.toFixed(p) : v;
}

let checkboxDebounceTimer = null;
async function onRuntimeCheckbox(group, key, value){
  if(checkboxDebounceTimer) clearTimeout(checkboxDebounceTimer);
  checkboxDebounceTimer = setTimeout(async ()=>{
    try {
  const res = await fetch(`/runtime_action/checkbox?group=${rURIComp(group)}&key=${rURIComp(key)}&value=${value?'true':'false'}`, { method:'POST'});
      if(!res.ok){ notify(`Toggle failed: ${group}/${key}`,'error'); return; }
      notify(`${key}: ${value?'ON':'OFF'}`,'info',1200);
      fetchRuntime();
    } catch(e){ notify(`Toggle error ${key}: ${e.message}`,'error'); }
  }, 160); // small debounce
}

function capitalize(s) {
  if (!s) return "";
  return s.charAt(0).toUpperCase() + s.slice(1);
}
function formatValue(val, meta) {
  if (val == null) return "";
  if (typeof val === "number") {
    if (meta && typeof meta.precision === "number" && !Number.isInteger(val)) {
      try {
        return val.toFixed(meta.precision);
      } catch (e) {
        return val;
      }
    }
    return val;
  }
  return val;
}
function formatBool(val) {
  return val ? "On" : "Off";
}
function severityClass(val, meta) {
  if (typeof val !== "number" || !meta) return "";
  if (
    meta.alarmMin !== undefined &&
    meta.alarmMin !== null &&
    meta.alarmMin !== "" &&
    meta.alarmMin !== false &&
    !Number.isNaN(meta.alarmMin) &&
    val < meta.alarmMin
  )
    return "sev-alarm";
  if (
    meta.alarmMax !== undefined &&
    meta.alarmMax !== null &&
    meta.alarmMax !== "" &&
    meta.alarmMax !== false &&
    !Number.isNaN(meta.alarmMax) &&
    val > meta.alarmMax
  )
    return "sev-alarm";
  if (
    meta.warnMin !== undefined &&
    meta.warnMin !== null &&
    !Number.isNaN(meta.warnMin) &&
    val < meta.warnMin
  )
    return "sev-warn";
  if (
    meta.warnMax !== undefined &&
    meta.warnMax !== null &&
    !Number.isNaN(meta.warnMax) &&
    val > meta.warnMax
  )
    return "sev-warn";
  return "";
}
function valueClasses(val, meta) {
  if (meta && meta.isBool) {
    const classes = ["br"];
    if (
      typeof meta.boolAlarmValue === "boolean" &&
      !!val === meta.boolAlarmValue
    ) {
      classes.push("sev-alarm");
    }
    return classes.join(" ");
  }
  return severityClass(val, meta);
}

function boolState(val, meta) {
  if (!meta || !meta.isBool) return "";
  const v = !!val;
  if (typeof meta.boolAlarmValue === "boolean" && v === meta.boolAlarmValue)
    return "alarm";
  return v ? "on" : "off";
}

function fieldRule(meta, key) {
  if (!meta) return null;
  const rules = ensureStyleRules(meta);
  if (!rules) return null;
  return rules[key] || null;
}

function fieldVisible(meta, key, fallback = true) {
  const rule = fieldRule(meta, key);
  if (!rule || rule.visible === undefined) return fallback;
  return !!rule.visible;
}

function fieldCss(meta, key) {
  const rule = fieldRule(meta, key);
  if (!rule) return {};
  return rule.css || {};
}

function selectBoolDotRule(val, meta) {
  const rules = ensureStyleRules(meta);
  if (!rules) return null;
  const boolVal = !!val;
  if (
    typeof meta.boolAlarmValue === "boolean" &&
    boolVal === meta.boolAlarmValue
  ) {
    if (rules.stateDotOnAlarm) return rules.stateDotOnAlarm;
  }
  if (boolVal && rules.stateDotOnTrue) return rules.stateDotOnTrue;
  if (!boolVal && rules.stateDotOnFalse) return rules.stateDotOnFalse;
  if (boolVal && rules.stateDotOnFalse) return rules.stateDotOnFalse;
  if (!boolVal && rules.stateDotOnTrue) return rules.stateDotOnTrue;
  if (rules.stateDotOnAlarm) return rules.stateDotOnAlarm;
  return null;
}

function boolDotVisible(val, meta) {
  const rule = selectBoolDotRule(val, meta);
  if (!rule) return false;
  if (rule.visible === undefined) return true;
  return !!rule.visible;
}

function boolDotStyle(val, meta) {
  const rule = selectBoolDotRule(val, meta);
  if (!rule) return {};
  return rule.css || {};
}
function fallbackPolling() {
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
h3 {
  color: orange;
  text-decoration: underline;
  margin: .5rem auto;
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
  box-shadow: inset 0 0 0 1px #ffffff, 0 1px 2px rgba(0,0,0,0.05);
  position: relative;
}
.rw.sl:hover {
  border-color: #d0d0d0;
  background: #f5f5f5;
}
.rw.sl .sw {
  display: flex;
  align-items: center;
  gap: .5rem;
  min-width: 10rem;
}
.rw.sl input[type=range] {
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
  background:#1976d2;
  color:#fff;
  border:none;
  padding:0.25rem 0.6rem;
  border-radius:4px;
  cursor:pointer;
  font-size:0.72rem;
  font-weight:600;
  letter-spacing:.5px;
  text-transform:uppercase;
}
.rw.sl .sb:hover { background:#125a9f; }
.rw.sl .num-input {
  width:5.5rem;
  padding:0.2rem 0.3rem;
  border:1px solid #bbb;
  border-radius:4px;
  font-size:0.75rem;
  background:#fff;
}
.rw.sl .num-input:focus { outline:1px solid #1976d2; }
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
.rw.cr { grid-template-columns: minmax(0,1fr) auto !important; }
.rw.cr .val { justify-self: end; }
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
