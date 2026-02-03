<template>
  <div>
    <h1 id="mainHeader" class="ta-center">{{ headerTitle }}</h1>
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
      <button
        v-if="hasLiveContent && guiLoggingEnabled"
        :class="{ active: activeTab === 'log' }"
        @click="activeTab = 'log'"
      >
        Log
      </button>
      <button class="theme-btn" type="button" @click="toggleTheme">
        Theme: {{ themeMode === 'dark' ? 'Dark' : 'Light' }}
      </button>
    </div>
    <div v-if="showHttpOnlyHint" class="http-only-hint" role="note" aria-label="HTTP only hint">
      <div class="http-only-hint__text">
        This device does not support HTTPS. Use
        <a class="http-only-hint__link" :href="httpOnlyHintUrl">{{ httpOnlyHintUrl }}</a>
      </div>
      <button class="http-only-hint__dismiss" type="button" @click="dismissHttpOnlyHint">Dismiss</button>
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
    <div
      v-if="activeGuiPopup"
      class="gui-popup-overlay"
      role="alertdialog"
      aria-modal="true"
      :aria-label="activeGuiPopup?.title || 'Message'"
      @click.self="dismissGuiPopup"
    >
      <div class="gui-popup-card" :class="activeGuiPopup.type">
        <div class="gui-popup-header">
          <span class="gui-popup-badge">{{ badgeLabel(activeGuiPopup.type) }}</span>
          <h3>{{ activeGuiPopup.title }}</h3>
        </div>
        <p class="gui-popup-message">{{ activeGuiPopup.message }}</p>
        <p v-if="activeGuiPopup.details" class="gui-popup-details">
          {{ activeGuiPopup.details }}
        </p>
        <dl v-if="contextEntries.length" class="gui-popup-context">
          <template v-for="entry in contextEntries" :key="entry.label">
            <div>
              <dt>{{ entry.label }}</dt>
              <dd>{{ entry.value }}</dd>
            </div>
          </template>
        </dl>
        <div class="gui-popup-actions">
          <button
            v-for="action in activeGuiPopup.actions"
            :key="action.id"
            :class="['gui-action-btn', action.primary ? 'primary' : 'ghost']"
            :disabled="guiActionBusy"
            @click="handleGuiAction(action.id, action)"
          >
            <span v-if="guiActionBusy && action.primary" class="busy-dot"></span>
            {{ action.label }}
          </button>
        </div>
      </div>
    </div>
    <!-- Settings Authentication Modal -->
    <div v-if="showSettingsAuth" class="modal-overlay" @click="cancelSettingsAuth">
      <div class="modal-content" @click.stop>
        <h3>{{ pendingFlashStart ? 'OTA Flash Access Required' : 'Settings Access Required' }}</h3>
        <p v-if="pendingFlashStart" style="margin: 0 0 15px 0; color: #666; font-size: 14px;">
          Password required to start firmware flash/OTA update
        </p>
        <form @submit.prevent="confirmSettingsAuth">
          <div class="password-input-container">
            <input
              ref="settingsPasswordInput"
              v-model="settingsPassword"
              type="password"
              placeholder="Enter settings password"
              @keyup.escape="cancelSettingsAuth"
              autocomplete="off"
            />
          </div>
          <div class="modal-buttons">
            <button type="button" @click="cancelSettingsAuth" class="cancel-btn">Cancel</button>
            <button type="submit" class="confirm-btn">Authenticate</button>
          </div>
        </form>
      </div>
    </div>

    <RuntimeDashboard
      v-if="activeTab !== 'settings' && hasLiveContent"
      ref="runtimeDashboard"
      :config="config"
      :view="activeTab"
      @can-flash-change="handleCanFlashChange"
    />
    <section v-else-if="activeTab === 'settings' && settingsAuthenticated" class="settings-view">
      <div class="category-tabs" role="tablist" aria-label="Settings categories">
        <button
          v-for="c in settingsCategories"
          :key="c.key"
          class="cat-tab"
          :class="{ active: selectedSettingsCategory === c.key }"
          type="button"
          role="tab"
          :aria-selected="selectedSettingsCategory === c.key"
          @click="selectedSettingsCategory = c.key"
        >
          {{ c.label }}
        </button>
      </div>

      <div id="settingsContainer" :key="refreshKey">
        <template v-for="c in settingsCategories" :key="c.key + '_' + refreshKey">
          <template v-if="c.key === selectedSettingsCategory">
            <template v-for="card in extractSettingsCardsFromCategory(c.key, c.settings)" :key="c.key + '_' + (card.cardKey || card.title) + '_' + refreshKey">
              <Category
                :category="c.key"
                :settings="card.settings"
                :title="card.title"
                :busy-map="opBusy"
                @apply-single="applySingle"
                @save-single="saveSingle"
              />
            </template>
          </template>
        </template>
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
import { ref, onBeforeUnmount, onMounted, provide, nextTick, computed, watch } from "vue";
import Category from "./components/Category.vue";
import RuntimeDashboard from "./components/RuntimeDashboard.vue";

function isUnsetPasswordValue(value) {
  return value === undefined || value === null || value === '' || value === '***';
}

// Check if a value is a password field that should be encrypted
// Prefer explicit metadata; fall back to heuristic on key/display name
function isPasswordField(category, key, settingData) {
  if (settingData && settingData.isPassword === true) return true;
  const k = (key || '').toString().toLowerCase();
  const dn = ((settingData && (settingData.displayName || settingData.name)) || '').toString().toLowerCase();
  return k.includes('pass') || dn.includes('pass');
}

async function processValueForTransmission(category, key, value, settingData) {
  return value;
}

const config = ref({});
const refreshKey = ref(0);
const version = ref("");
const appName = ref("");
const appTitle = ref("");
const refreshing = ref(false);

const headerTitle = computed(() => {
  const base = (appName.value && appName.value.length) ? appName.value : "ConfigManager";
  return (version.value && version.value.length) ? `${base} ${version.value}` : base;
});

function resolveCategoryLabel(categoryKey, settingsObj) {
  if (
    settingsObj &&
    typeof settingsObj === 'object' &&
    Object.prototype.hasOwnProperty.call(settingsObj, 'categoryPretty') &&
    typeof settingsObj.categoryPretty === 'string' &&
    settingsObj.categoryPretty.trim().length
  ) {
    return settingsObj.categoryPretty;
  }
  if (settingsObj && typeof settingsObj === 'object') {
    for (const key in settingsObj) {
      if (key === 'categoryPretty') continue;
      const sd = settingsObj[key];
      if (sd && typeof sd === 'object' && typeof sd.categoryPretty === 'string' && sd.categoryPretty.trim().length) {
        return sd.categoryPretty;
      }
    }
  }
  if (categoryKey === 'IO') return 'I/O';
  return categoryKey;
}

function normalizeCategoryToken(value) {
  return String(value || '')
    .toLowerCase()
    .replace(/[^a-z0-9]/g, '');
}

function resolveCategoryOrder(settingsObj) {
  if (!settingsObj || typeof settingsObj !== 'object') return undefined;
  if (Object.prototype.hasOwnProperty.call(settingsObj, 'categoryOrder') && typeof settingsObj.categoryOrder === 'number') {
    return settingsObj.categoryOrder;
  }
  for (const key in settingsObj) {
    if (key === 'categoryPretty' || key === 'categoryOrder' || key === 'cards') continue;
    const sd = settingsObj[key];
    if (sd && typeof sd === 'object' && typeof sd.categoryOrder === 'number') {
      return sd.categoryOrder;
    }
  }
  return undefined;
}

const orderedSettingsCategories = computed(() => {
  const cfg = config.value;
  const entries = Object.entries(cfg || {});
  entries.sort((a, b) => {
    const aKey = a[0];
    const bKey = b[0];
    const aLabel = resolveCategoryLabel(aKey, a[1]);
    const bLabel = resolveCategoryLabel(bKey, b[1]);

    // Optional override via metadata
    const aOrder = resolveCategoryOrder(a[1]);
    const bOrder = resolveCategoryOrder(b[1]);
    if (typeof aOrder === 'number' && typeof bOrder === 'number' && aOrder !== bOrder) return aOrder - bOrder;
    if (typeof aOrder === 'number' && typeof bOrder !== 'number') return -1;
    if (typeof bOrder === 'number' && typeof aOrder !== 'number') return 1;

    // Prefer a fixed ordering for the most important categories.
    const priorityOrder = {
      wifi: 0,
      buttons: 2,
      ntp: 3,
      system: 1000,
    };
    const aToken = normalizeCategoryToken(aLabel) || normalizeCategoryToken(aKey);
    const bToken = normalizeCategoryToken(bLabel) || normalizeCategoryToken(bKey);

    // Default: push "system" behind everything else unless categoryOrder is explicitly set.
    if (typeof aOrder !== 'number' && typeof bOrder !== 'number') {
      if (aToken === 'system' && bToken !== 'system') return 1;
      if (bToken === 'system' && aToken !== 'system') return -1;
    }

    const aPrio = Object.prototype.hasOwnProperty.call(priorityOrder, aToken) ? priorityOrder[aToken] : Number.POSITIVE_INFINITY;
    const bPrio = Object.prototype.hasOwnProperty.call(priorityOrder, bToken) ? priorityOrder[bToken] : Number.POSITIVE_INFINITY;
    if (aPrio !== bPrio) return aPrio - bPrio;

    const labelCmp = String(aLabel).localeCompare(String(bLabel));
    if (labelCmp !== 0) return labelCmp;
    return String(aKey).localeCompare(String(bKey));
  });
  return entries;
});

function extractSettingsCardsFromCategory(categoryKey, settingsObj) {
  if (!settingsObj || typeof settingsObj !== 'object') {
    return [{
      category: categoryKey,
      title: resolveCategoryLabel(categoryKey, settingsObj),
      order: undefined,
      settings: settingsObj || {},
    }];
  }

  const cardsContainer = settingsObj.cards;
  if (!cardsContainer || typeof cardsContainer !== 'object') {
    return [{
      category: categoryKey,
      title: resolveCategoryLabel(categoryKey, settingsObj),
      order: resolveCategoryOrder(settingsObj),
      settings: settingsObj,
    }];
  }

  const cards = Object.entries(cardsContainer)
    .map(([cardKey, cardObj]) => {
      const co = (cardObj && typeof cardObj === 'object') ? cardObj : {};
      const title = (typeof co.cardPretty === 'string' && co.cardPretty.trim().length) ? co.cardPretty : String(cardKey);
      const order = (typeof co.cardOrder === 'number') ? co.cardOrder : undefined;
      const settings = (co && typeof co.settings === 'object' && co.settings) ? co.settings : {};
      return { category: categoryKey, title, order, settings, cardKey: String(cardKey) };
    });

  cards.sort((a, b) => {
    const ao = a.order;
    const bo = b.order;
    if (typeof ao === 'number' && typeof bo === 'number' && ao !== bo) return ao - bo;
    if (typeof ao === 'number' && typeof bo !== 'number') return -1;
    if (typeof bo === 'number' && typeof ao !== 'number') return 1;
    const t = String(a.title).localeCompare(String(b.title));
    if (t !== 0) return t;
    return String(a.cardKey || '').localeCompare(String(b.cardKey || ''));
  });
  return cards;
}

const activeTab = ref("live");
const runtimeDashboard = ref(null);
const canFlash = ref(false);
const guiLoggingEnabled = ref(false);
const toasts = ref([]); // {id,message,type,sticky,ts}
let toastCounter = 0;

const showHttpOnlyHint = ref(false);
const httpOnlyHintUrl = ref('');
const HTTP_ONLY_HINT_STORAGE_KEY = 'cm.dismiss.httpOnlyHint.v1';

// Detect if there is any live UI content available
const hasLiveContent = ref(true);

const guiPopup = ref(null);
const guiPopupQueue = [];
const guiActionBusy = ref(false);
const activeGuiPopup = computed(() => guiPopup.value);
const guiContextDisplay = [
  { key: "category", label: "Category" },
  { key: "key", label: "Key" },
  { key: "value", label: "Value" },
  { key: "pin", label: "Pin" },
  { key: "role", label: "Role" },
  { key: "mode", label: "Mode" },
  { key: "origin", label: "Request" },
  { key: "endpoint", label: "Endpoint" },
  { key: "forceParam", label: "Force parameter" },
];
function formatContextValue(value) {
  if (value === null || typeof value === "undefined") return "";
  if (typeof value === "object") {
    try {
      return JSON.stringify(value);
    } catch (e) {
      return String(value);
    }
  }
  return String(value);
}
const contextEntries = computed(() => {
  const ctx = activeGuiPopup.value && activeGuiPopup.value.context;
  if (!ctx || typeof ctx !== "object") return [];
  return guiContextDisplay
    .map(({ key, label }) => {
      const raw = ctx[key];
      if (typeof raw === "undefined" || raw === null || raw === "") return null;
      return { label, value: formatContextValue(raw) };
    })
    .filter(Boolean);
});

// Settings authentication
const showSettingsAuth = ref(false);
const settingsAuthenticated = ref(false);
const settingsPassword = ref("");
const settingsPasswordInput = ref(null);
const pendingFlashStart = ref(false); // Track if we need to start flash after auth

// Token-based settings auth (issued by POST /config/auth)
const settingsAuthToken = ref("");
const settingsAuthTokenExpiresAtMs = ref(0);
const settingsAuthPasswordRequired = ref(true);

const selectedSettingsCategory = ref('');

const THEME_COOKIE_KEY = 'cm.theme.v1';
const themeMode = ref(resolveInitialTheme());

function readCookie(name) {
  try {
    const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    const m = document.cookie.match(new RegExp('(?:^|; )' + escaped + '=([^;]*)'));
    return m ? decodeURIComponent(m[1]) : '';
  } catch (e) {
    return '';
  }
}

function writeCookie(name, value, maxAgeSec) {
  try {
    const parts = [`${name}=${encodeURIComponent(String(value))}`, 'path=/'];
    if (typeof maxAgeSec === 'number') parts.push(`max-age=${Math.max(0, Math.floor(maxAgeSec))}`);
    document.cookie = parts.join('; ');
  } catch (e) {}
}

function resolveInitialTheme() {
  const stored = (readCookie(THEME_COOKIE_KEY) || '').toLowerCase();
  if (stored === 'dark' || stored === 'light') return stored;
  try {
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  } catch (e) {
    return 'light';
  }
}

function applyTheme(mode) {
  const m = (mode === 'dark') ? 'dark' : 'light';
  document.documentElement.dataset.theme = m;
}

function toggleTheme() {
  themeMode.value = (themeMode.value === 'dark') ? 'light' : 'dark';
  writeCookie(THEME_COOKIE_KEY, themeMode.value, 60 * 60 * 24 * 365);
  applyTheme(themeMode.value);
}

const settingsCategories = computed(() => {
  return (orderedSettingsCategories.value || []).map(([categoryKey, settingsObj]) => {
    return {
      key: categoryKey,
      label: resolveCategoryLabel(categoryKey, settingsObj),
      settings: settingsObj,
    };
  });
});

watch(
  [() => settingsCategories.value],
  () => {
    if (settingsCategories.value.length === 0) return;
    const exists = settingsCategories.value.some((c) => c.key === selectedSettingsCategory.value);
    if (!exists) selectedSettingsCategory.value = settingsCategories.value[0].key;
  },
  { immediate: true }
);

watch(guiLoggingEnabled, (enabled) => {
  if (!enabled && activeTab.value === "log") {
    activeTab.value = "live";
  }
});

function isSettingsAuthTokenValid() {
  return (
    typeof settingsAuthToken.value === "string" &&
    settingsAuthToken.value.length > 0 &&
    Date.now() < settingsAuthTokenExpiresAtMs.value
  );
}

async function ensureSettingsAuthToken(opts = {}) {
  const timeoutMs = (opts && typeof opts.timeoutMs === 'number') ? opts.timeoutMs : 1500;
  if (isSettingsAuthTokenValid()) return true;

  // Password-less setups: try to authenticate silently with empty password
  const probe = await authenticateSettings("", { timeoutMs, silent: true });
  return !!probe.ok;
}

async function authenticateSettings(password, opts = {}) {
  const { timeoutMs = 1500, silent = false } = opts;
  try {
    const controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
    const timeoutId = controller ? setTimeout(() => controller.abort(), timeoutMs) : null;

    const r = await fetch("/config/auth", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ password: String(password || "") }),
      signal: controller ? controller.signal : undefined,
    });

    if (timeoutId) clearTimeout(timeoutId);

    const json = await r.json().catch(() => ({}));
    if (r.ok && json.status === "ok" && typeof json.token === "string") {
      const ttlSec = Number(json.ttlSec || 0);
      settingsAuthToken.value = json.token;
      settingsAuthTokenExpiresAtMs.value = Date.now() + Math.max(1, ttlSec) * 1000;
      settingsAuthenticated.value = true;
      settingsAuthPasswordRequired.value = false;
      return { ok: true };
    }

    if (r.status === 401 || r.status === 403) {
      settingsAuthPasswordRequired.value = true;
      settingsAuthenticated.value = false;
      return { ok: false, reason: "unauthorized" };
    }

    settingsAuthenticated.value = false;
    if (!silent) {
      const reason = (json && json.reason) ? String(json.reason) : `http_${r.status}`;
      notify(`Auth failed: ${reason}`, "error", 8000);
    }
    return { ok: false, reason: (json && json.reason) ? String(json.reason) : "unknown" };
  } catch (e) {
    settingsAuthenticated.value = false;
    if (!silent) {
      const isTimeout = e && (e.name === 'AbortError');
      notify(isTimeout ? "Auth timeout" : "Auth request failed", "error", 8000);
    }
    return { ok: false, reason: (e && e.name === 'AbortError') ? "timeout" : "network" };
  }
}

async function fetchStoredPassword(category, key) {
  const ok = await ensureSettingsAuthToken({ timeoutMs: 1500 });
  if (!ok) {
    settingsAuthenticated.value = false;
    showSettingsAuth.value = true;
    throw new Error("Not authenticated");
  }
  const r = await fetch(
    `/config/password?category=${rURIComp(category)}&key=${rURIComp(key)}`,
    {
      method: "GET",
      headers: { "X-Settings-Token": settingsAuthToken.value },
    }
  );
  const json = await r.json().catch(() => ({}));
  if (!r.ok || json.status !== "ok") {
    if (r.status === 401 || r.status === 403) {
      settingsAuthenticated.value = false;
      showSettingsAuth.value = true;
    }
    throw new Error(json.reason || "Failed");
  }
  return String(json.value ?? "");
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
provide("fetchStoredPassword", fetchStoredPassword);

const GUI_MESSAGE_TYPES = new Set(["errorMessage", "warningMessage", "infoMessage"]);
const GUI_DEFAULT_ACTION = { id: "close", label: "Close", primary: true };
let guiWebSocket = null;
let guiWsRetry = 0;
let guiWsReconnectTimer = null;

function badgeLabel(type) {
  if (type === "errorMessage") return "Error";
  if (type === "warningMessage") return "Warning";
  return "Info";
}

function normalizeGuiPayload(payload) {
  if (!payload || typeof payload !== "object") return null;
  if (!GUI_MESSAGE_TYPES.has(payload.type)) return null;
  const rawActions = Array.isArray(payload.actions) ? payload.actions : [];
  const actions = rawActions
    .map((act, idx) => {
      if (!act) return null;
      const id = act.id ? String(act.id) : `action_${idx}`;
      const label = act.label ? String(act.label) : id;
      return { id, label, primary: !!act.primary };
    })
    .filter((act) => act && act.label);
  if (!actions.length) {
    actions.push({ ...GUI_DEFAULT_ACTION });
  }
  return {
    type: payload.type,
    title:
      payload.title ||
      (payload.type === "errorMessage" ? "Validation issue" : "Information"),
    message: payload.message || "",
    details: payload.details || "",
    context: payload.context || {},
    actions,
  };
}

function queueGuiPopupMessage(payload) {
  const normalized = normalizeGuiPayload(payload);
  if (!normalized) return;
  if (!guiPopup.value) {
    guiPopup.value = normalized;
    return;
  }
  guiPopupQueue.push(normalized);
}

function showNextGuiPopup() {
  if (guiPopupQueue.length) {
    guiPopup.value = guiPopupQueue.shift();
    return;
  }
  guiPopup.value = null;
}

function dismissGuiPopup() {
  guiActionBusy.value = false;
  showNextGuiPopup();
}

async function handleGuiAction(actionId, action = {}) {
  const current = activeGuiPopup.value;
  if (!current) return;
  if (actionId === "cancel" || actionId === "close") {
    dismissGuiPopup();
    return;
  }
  const url = buildGuiActionUrl(current.context || {}, actionId);
  const body = buildGuiActionBody(current.context || {});
  if (!url || body === null) {
    notify("Unable to apply the selected action", "error", 6000);
    dismissGuiPopup();
    return;
  }
  guiActionBusy.value = true;
  try {
    const response = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body,
    });
    const json = await response.json().catch(() => ({}));
    if (!response.ok || json.status !== "ok") {
      throw new Error(json.reason || `HTTP ${response.status}`);
    }
    notify(`${action.label || "Action"} succeeded`, "success");
  } catch (e) {
    notify(`${action.label || "Action"} failed: ${e.message}`, "error", 8000);
  } finally {
    guiActionBusy.value = false;
    dismissGuiPopup();
  }
}

function buildGuiActionUrl(context, actionId) {
  if (context && context.guiActionEndpoint) {
    const endpoint = String(context.guiActionEndpoint);
    const sep = endpoint.includes("?") ? "&" : "?";
    let url = `${endpoint}${sep}actionId=${rURIComp(actionId)}`;
    if (context.guiMessageId) {
      url += `&messageId=${rURIComp(context.guiMessageId)}`;
    }
    return url;
  }
  const base = context ? (context.endpoint || defaultGuiEndpoint(context)) : null;
  if (!base) return null;
  if (actionId === "force" && context && context.forceParam) {
    const sep = base.includes("?") ? "&" : "?";
    return `${base}${sep}${context.forceParam}`;
  }
  return base;
}

function defaultGuiEndpoint(context) {
  const origin = context.origin;
  const category = context.category;
  const key = context.key;
  if (origin === "apply_all") return "/config/apply_all";
  if (origin === "save_all") return "/config/save_all";
  if (!category || !key) return null;
  const cat = rURIComp(category);
  const k = rURIComp(key);
  if (origin === "save") return `/config/save?category=${cat}&key=${k}`;
  if (origin === "apply") return `/config/apply?category=${cat}&key=${k}`;
  if (context.isApply) return `/config/apply?category=${cat}&key=${k}`;
  return `/config/save?category=${cat}&key=${k}`;
}

function buildGuiActionBody(context) {
  if (context && context.guiActionEndpoint) {
    return "{}";
  }
  if (typeof context.payload !== "undefined" && context.payload !== null) {
    return typeof context.payload === "string"
      ? context.payload
      : JSON.stringify(context.payload);
  }
  if (typeof context.value !== "undefined") {
    return JSON.stringify({ value: context.value });
  }
  return null;
}

function initGuiWebSocket() {
  if (typeof window === "undefined") return;
  const proto = window.location.protocol === "https:" ? "wss://" : "ws://";
  const url = proto + window.location.host + "/ws";
  connectGuiWebSocket(url);
}

function connectGuiWebSocket(url) {
  closeGuiWebSocket();
  let connectionTimeout = null;
  try {
    guiWebSocket = new WebSocket(url);
  } catch (e) {
    scheduleGuiReconnect(url);
    return;
  }
  connectionTimeout = setTimeout(() => {
    if (guiWebSocket && guiWebSocket.readyState === WebSocket.CONNECTING) {
      try {
        guiWebSocket.close();
      } catch (e) {
        /* ignore */
      }
      guiWebSocket = null;
    }
    scheduleGuiReconnect(url);
  }, 2000);
  guiWebSocket.onopen = () => {
    clearTimeout(connectionTimeout);
    guiWsRetry = 0;
  };
  guiWebSocket.onclose = () => {
    clearTimeout(connectionTimeout);
    scheduleGuiReconnect(url);
  };
  guiWebSocket.onerror = () => {
    clearTimeout(connectionTimeout);
    scheduleGuiReconnect(url);
  };
  guiWebSocket.onmessage = handleGuiWsMessage;
}

function scheduleGuiReconnect(url) {
  if (guiWsReconnectTimer) return;
  const delay = Math.min(10000, 1000 + guiWsRetry * 700);
  guiWsRetry = Math.min(10, guiWsRetry + 1);
  guiWsReconnectTimer = setTimeout(() => {
    guiWsReconnectTimer = null;
    connectGuiWebSocket(url);
  }, delay);
}

function closeGuiWebSocket() {
  if (guiWsReconnectTimer) {
    clearTimeout(guiWsReconnectTimer);
    guiWsReconnectTimer = null;
  }
  if (guiWebSocket) {
    try {
      guiWebSocket.close();
    } catch (e) {
      /* ignore */
    }
    guiWebSocket = null;
  }
}

function handleGuiWsMessage(event) {
  if (typeof event.data === "string" && event.data === "__ping") {
    try {
      guiWebSocket?.send("__pong");
    } catch (e) {
      /* ignore */
    }
    return;
  }
  try {
    const parsed = JSON.parse(event.data);
    if (!parsed || typeof parsed !== "object") return;
    queueGuiPopupMessage(parsed);
  } catch (e) {
    // Ignore malformed frames
  }
}

function isPrivateIpv4(hostname) {
  const m = /^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$/.exec(hostname);
  if (!m) return false;
  const a = Number(m[1]);
  const b = Number(m[2]);
  const c = Number(m[3]);
  const d = Number(m[4]);
  if ([a, b, c, d].some((x) => Number.isNaN(x) || x < 0 || x > 255)) return false;
  if (a === 10) return true;
  if (a === 192 && b === 168) return true;
  if (a === 172 && b >= 16 && b <= 31) return true;
  return false;
}

function shouldShowHttpOnlyHint() {
  try {
    if (localStorage.getItem(HTTP_ONLY_HINT_STORAGE_KEY) === '1') return false;
  } catch {
    // Ignore storage access errors
  }

  // Only show when the UI is actually loaded via HTTPS (e.g. reverse proxy).
  // If the browser auto-upgrades to HTTPS and the device can't serve it, the page won't load
  // and we can't show any hint here.
  return window.location.protocol === 'https:';
}

function dismissHttpOnlyHint() {
  showHttpOnlyHint.value = false;
  try {
    localStorage.setItem(HTTP_ONLY_HINT_STORAGE_KEY, '1');
  } catch {
    // Ignore storage access errors
  }
}

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
    // Check content-length header vs actual response (compare bytes, not JS string length)
    const contentLength = r.headers.get('content-length');
    //console.log(`[Frontend] config.json - Content-Length: ${contentLength}`);

    const text = await r.text();
    if (contentLength) {
      const headerBytes = parseInt(contentLength, 10);
      const actualBytes = (typeof TextEncoder !== 'undefined')
        ? new TextEncoder().encode(text).length
        : text.length;
      if (Number.isFinite(headerBytes) && headerBytes !== actualBytes) {
        console.warn(`[Frontend] Content-Length mismatch (bytes)! Header: ${headerBytes}, Actual: ${actualBytes}`);
      }
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
      await switchToSettings();
      notify("No live dashboard defined – opening Settings", "info");
    }
  }
}
async function switchToSettings() {
  // If token is valid, allow immediate access.
  if (isSettingsAuthTokenValid()) {
    settingsAuthenticated.value = true;
    showSettingsAuth.value = false;
    activeTab.value = "settings";
    await loadSettings();
    return;
  }

  // Try a silent auth probe with an empty password.
  // This enables password-less setups without prompting the user.
  const probe = await authenticateSettings("", { timeoutMs: 1500, silent: true });
  if (probe.ok) {
    showSettingsAuth.value = false;
    activeTab.value = "settings";
    await loadSettings();
    return;
  }

  // Show modal immediately (avoid UI delay on slow WiFi/requests)
  showSettingsAuth.value = true;
  // Focus password input after modal appears
  setTimeout(() => {
    settingsPasswordInput.value?.focus();
  }, 100);

  // Do not auto-auth with empty password (would generate 403 noise on protected setups).
}

async function confirmSettingsAuth() {
  const res = await authenticateSettings(settingsPassword.value, { timeoutMs: 3000 });
  if (!res.ok) {
    notify(res.reason === 'unauthorized' ? "Invalid password" : `Auth failed: ${res.reason}`, "error");
    settingsPassword.value = "";
    settingsPasswordInput.value?.focus();
    return;
  }

  showSettingsAuth.value = false;
  settingsPassword.value = ""; // Clear password for security

  // Check if we need to start flash after authentication
  if (pendingFlashStart.value) {
    pendingFlashStart.value = false;
    notify("Access granted - Starting OTA flash...", "success");
    runtimeDashboard.value?.startFlash();
    return;
  }

  // Normal settings access
  activeTab.value = "settings";
  loadSettings();
  notify(settingsAuthPasswordRequired.value ? "Settings access granted" : "Settings unlocked", "success");
}

function cancelSettingsAuth() {
  showSettingsAuth.value = false;
  settingsPassword.value = "";
  pendingFlashStart.value = false; // Clear any pending flash start
  activeTab.value = "live"; // Stay on live tab
}
async function injectVersion() {
  try {
    // Preferred: structured app info
    const r = await fetch("/appinfo");
    if (r.ok) {
      const info = await r.json().catch(() => ({}));
      appName.value = info && typeof info.appName === 'string' ? info.appName.trim() : "";
      appTitle.value = info && typeof info.appTitle === 'string' ? info.appTitle.trim() : "";
      version.value = info && typeof info.version === 'string' ? info.version.trim() : "";
      guiLoggingEnabled.value = !!(info && info.guiLogging);

      const tabBase = (appTitle.value && appTitle.value.length)
        ? appTitle.value
        : ((appName.value && appName.value.length) ? appName.value : "ESP32 Configuration");
      document.title = (version.value && version.value.length) ? `${tabBase} ${version.value}` : tabBase;
      return;
    }

    // Fallback: legacy /version (plain text "<name> <semver>")
    const r2 = await fetch("/version");
    if (!r2.ok) throw new Error("Version fetch failed");
    const raw = String(await r2.text()).trim();
    const m = raw.match(/^(.*)\s+(\d+\.\d+\.\d+(?:[-+].*)?)$/);
    const legacyName = (m && m[1]) ? String(m[1]).trim() : raw;
    const legacyVer = (m && m[2]) ? String(m[2]).trim() : "";
    appName.value = legacyName;
    version.value = legacyVer;
    document.title = (legacyVer && legacyVer.length) ? `${legacyName} ${legacyVer}` : legacyName;
  } catch (e) {
    appName.value = "";
    appTitle.value = "";
    version.value = "";
    guiLoggingEnabled.value = false;
    document.title = "ESP32 Configuration";
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
    if (isPasswordField(category, key, settingData) && isUnsetPasswordValue(value)) {
      updateToast(tid, `Apply skipped ${opKey}: No new password entered`, "error");
      return;
    }
    if (isPasswordField(category, key, settingData)) {
      const inputEl = collectSettingsInputs().find((i) => i.name === `${category}.${key}`);
      const revealedValue = inputEl && inputEl.dataset ? inputEl.dataset.revealedValue : undefined;
      if (revealedValue !== undefined && String(value) === String(revealedValue)) {
        updateToast(tid, `Apply skipped ${opKey}: Password unchanged`, "error");
        return;
      }
    }
    
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
    // Add timeout to HTTP request
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 10000); // 10 second timeout
    
    // Get setting metadata to check if this is a password field
    const settingData = config.value[category] && config.value[category][key];
    if (isPasswordField(category, key, settingData) && isUnsetPasswordValue(value)) {
      updateToast(tid, `Save skipped ${opKey}: No new password entered`, "error");
      return;
    }
    if (isPasswordField(category, key, settingData)) {
      const inputEl = collectSettingsInputs().find((i) => i.name === `${category}.${key}`);
      const revealedValue = inputEl && inputEl.dataset ? inputEl.dataset.revealedValue : undefined;
      if (revealedValue !== undefined && String(value) === String(revealedValue)) {
        updateToast(tid, `Save skipped ${opKey}: Password unchanged`, "error");
        return;
      }
    }
    
    const r = await fetch(
      `/config/save?category=${rURIComp(category)}&key=${rURIComp(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value }),
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
      (!input.value || input.value === "***" || (input.dataset.revealedValue !== undefined && input.value === input.dataset.revealedValue))
    ) {
      // Skip placeholder or empty password fields
      return;
    }
    let v = input.type === "checkbox" ? input.checked : input.value;
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
    if (
      input.dataset.isPassword === "1" &&
      (!input.value || input.value === "***" || (input.dataset.revealedValue !== undefined && input.value === input.dataset.revealedValue))
    ) {
      // Skip placeholder/empty/unchanged password fields
      return;
    }
    let v = input.type === "checkbox" ? input.checked : input.value;
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
  // If settings are protected, require auth before starting OTA flash
  if (!isSettingsAuthTokenValid()) {
    pendingFlashStart.value = true;
    showSettingsAuth.value = true;
    setTimeout(() => {
      settingsPasswordInput.value?.focus();
    }, 100);
    return;
  }

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
  applyTheme(themeMode.value);
  if (shouldShowHttpOnlyHint()) {
    httpOnlyHintUrl.value = `http://${window.location.host}/`;
    showHttpOnlyHint.value = true;
  }
  loadUserTheme();
  loadSettings();
  injectVersion();
  checkLiveContent();
  initGuiWebSocket();
});
onBeforeUnmount(() => {
  closeGuiWebSocket();
});
</script>
<style scoped>
#mainHeader {
  color: var(--cm-accent-strong, #034875);
  margin: 0.2rem 0 0;
  border-bottom: 2px solid var(--cm-accent, #3498db);
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

.http-only-hint {
  display: flex;
  gap: 10px;
  align-items: center;
  justify-content: center;
  margin: -0.5rem 10px 0.75rem;
  padding: 10px 12px;
  background: #fff3cd;
  border: 1px solid #ffe69c;
  border-radius: 8px;
  color: #6b4a00;
  font-size: 0.9rem;
}

.http-only-hint__text {
  line-height: 1.3;
}

.http-only-hint__link {
  margin-left: 6px;
  color: #034875;
  font-weight: 700;
  text-decoration: underline;
}

.http-only-hint__dismiss {
  background: transparent;
  border: 1px solid rgba(0, 0, 0, 0.2);
  border-radius: 6px;
  padding: 6px 10px;
  cursor: pointer;
  color: inherit;
  font-weight: 600;
}
.tabs button {
  background: var(--cm-tab-bg, slategray);
  color: var(--cm-tab-fg, #f5f5f5);
  padding: 0.5rem 1.1rem;
  border-radius: 6px;
  border: none;
  cursor: pointer;
  font-weight: 600;
  transition: background 0.2s;
}
.tabs button.active {
  background: var(--cm-tab-active-bg, darkorange);
  color: var(--cm-tab-active-fg, #fff);
}
.theme-btn {
  background: var(--cm-tab-bg, slategray);
  color: var(--cm-tab-fg, #f5f5f5);
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

.category-tabs {
  display: flex;
  gap: 0.5rem;
  overflow-x: auto;
  padding: 0 10px 0.75rem;
  margin: 0 0 0.25rem;
}

.cat-tab {
  flex: 0 0 auto;
  background: var(--cm-tab-bg, slategray);
  color: var(--cm-tab-fg, #f5f5f5);
  padding: 0.5rem 0.9rem;
  border-radius: 6px;
  border: none;
  cursor: pointer;
  font-weight: 600;
  transition: background 0.2s;
}

.cat-tab.active {
  background: var(--cm-tab-active-bg, darkorange);
  color: var(--cm-tab-active-fg, #fff);
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

.pwd-toggle {
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

.pwd-toggle:hover {
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
.gui-popup-overlay {
  position: fixed;
  inset: 0;
  z-index: 5000;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1.5rem;
  background:
    radial-gradient(circle at top right, rgba(87, 182, 255, 0.25), transparent 55%),
    rgba(1, 6, 16, 0.85);
  backdrop-filter: blur(12px);
  animation: guiPopupFade 0.4s ease;
}
.gui-popup-card {
  width: min(520px, 90vw);
  background: linear-gradient(135deg, #050c16, #162339 90%);
  border-radius: 18px;
  border: 1px solid rgba(255, 255, 255, 0.08);
  padding: 1.8rem;
  box-shadow: 0 18px 40px rgba(0, 0, 0, 0.55), 0 0 25px rgba(87, 182, 255, 0.35);
  color: #f8fafc;
  font-family: "Space Grotesk", "IBM Plex Sans", system-ui, sans-serif;
}
.gui-popup-card.errorMessage {
  border-color: rgba(255, 99, 132, 0.35);
  box-shadow: 0 18px 40px rgba(0, 0, 0, 0.55), 0 0 25px rgba(255, 99, 132, 0.45);
}
.gui-popup-card.warningMessage {
  border-color: rgba(255, 192, 0, 0.4);
  box-shadow: 0 18px 40px rgba(0, 0, 0, 0.55), 0 0 25px rgba(255, 196, 66, 0.45);
}
.gui-popup-card.infoMessage {
  border-color: rgba(87, 182, 255, 0.3);
}
.gui-popup-header {
  display: flex;
  align-items: center;
  gap: 0.8rem;
  margin-bottom: 0.45rem;
}
.gui-popup-badge {
  font-size: 0.65rem;
  letter-spacing: 0.25em;
  text-transform: uppercase;
  padding: 0.15rem 0.85rem;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.12);
}
.gui-popup-card.errorMessage .gui-popup-badge {
  background: rgba(255, 99, 132, 0.25);
  color: #ffe0e6;
}
.gui-popup-card.warningMessage .gui-popup-badge {
  background: rgba(255, 192, 0, 0.25);
  color: #1d1a0d;
}
.gui-popup-card.infoMessage .gui-popup-badge {
  background: rgba(87, 182, 255, 0.25);
  color: #0b1f30;
}
.gui-popup-message {
  margin: 0;
  font-size: 1.1rem;
  line-height: 1.5;
  font-weight: 600;
}
.gui-popup-details {
  margin: 0.35rem 0;
  color: rgba(255, 255, 255, 0.8);
  font-size: 0.95rem;
  line-height: 1.4;
}
.gui-popup-context {
  margin-top: 1rem;
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: 0.5rem;
}
.gui-popup-context div {
  border: 1px solid rgba(255, 255, 255, 0.06);
  padding: 0.45rem 0.75rem;
  border-radius: 10px;
  background: rgba(255, 255, 255, 0.02);
}
.gui-popup-context dt {
  margin: 0;
  font-size: 0.6rem;
  text-transform: uppercase;
  letter-spacing: 0.2em;
  color: rgba(255, 255, 255, 0.5);
}
.gui-popup-context dd {
  margin: 0;
  font-weight: 600;
  font-size: 0.85rem;
}
.gui-popup-actions {
  margin-top: 1.1rem;
  display: flex;
  flex-wrap: wrap;
  justify-content: flex-end;
  gap: 0.65rem;
}
.gui-action-btn {
  flex: 1 1 120px;
  min-width: 100px;
  padding: 0.65rem 1rem;
  border-radius: 999px;
  border: 1px solid rgba(255, 255, 255, 0.5);
  background: rgba(255, 255, 255, 0.04);
  color: #f8fafc;
  font-weight: 600;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  cursor: pointer;
  transition: transform 0.2s, box-shadow 0.2s;
}
.gui-action-btn.primary {
  background: linear-gradient(120deg, #57b6ff, #84d0ff);
  border-color: transparent;
  color: #041022;
  box-shadow: 0 12px 25px rgba(87, 182, 255, 0.35);
}
.gui-action-btn.ghost:hover:not(:disabled),
.gui-action-btn.primary:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 16px 28px rgba(0, 0, 0, 0.4);
}
.gui-action-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
  box-shadow: none;
}
.busy-dot {
  display: inline-block;
  width: 10px;
  height: 10px;
  margin-right: 0.3rem;
  border-radius: 50%;
  background: #fff;
  animation: guiPulse 0.9s ease-in-out infinite;
  vertical-align: middle;
}
@keyframes guiPopupFade {
  from {
    opacity: 0;
    transform: translateY(20px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}
@keyframes guiPulse {
  0% {
    transform: scale(1);
  }
  50% {
    transform: scale(1.3);
  }
  100% {
    transform: scale(1);
  }
}
</style>
