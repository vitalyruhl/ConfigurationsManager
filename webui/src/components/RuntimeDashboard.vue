<template>
  <section class="live-view">
    <input
      ref="otaFileInput"
      type="file"
      accept=".bin,.bin.gz"
      class="hidden-file-input"
      @change="onFlashFileSelected"
    />

    <div class="live-cards">
      <div class="card" v-for="group in displayRuntimeGroups" :key="group.name">
        <h3>{{ group.title }}</h3>

        <div class="tbl">
          <template v-for="f in group.fields" :key="f.key">
            <hr v-if="f.isDivider" class="dv" :data-label="f.label" />

            <RuntimeActionButton
              v-else-if="f.isButton"
              :group="group.name"
              :field="f"
              @action="handleRuntimeButton"
            />

            <RuntimeStateButton
              v-else-if="f.isStateButton"
              :group="group.name"
              :field="f"
              :value="runtime[group.name] && runtime[group.name][f.key]"
              @toggle="handleStateToggle"
            />

            <RuntimeSlider
              v-else-if="f.isIntSlider || f.isFloatSlider"
              :group="group.name"
              :field="f"
              :value="runtime[group.name] && runtime[group.name][f.key]"
              :mode="f.isFloatSlider ? 'float' : 'int'"
              @commit="handleSliderCommit"
            />

            <RuntimeNumberInput
              v-else-if="f.isIntInput || f.isFloatInput"
              :group="group.name"
              :field="f"
              :value="runtime[group.name] && runtime[group.name][f.key]"
              :mode="f.isFloatInput ? 'float' : 'int'"
              @commit="handleInputCommit"
            />

            <RuntimeCheckbox
              v-else-if="f.isCheckbox"
              :group="group.name"
              :field="f"
              :value="runtime[group.name] && runtime[group.name][f.key]"
              @change="handleCheckboxChange"
            />

            <div v-else-if="f.isString" class="rw str">
              <span class="lab">{{ f.label }}</span>
              <span class="val">
                <template v-if="f.staticValue">{{ f.staticValue }}</template>
                <template v-else-if="runtime[group.name] && runtime[group.name][f.key] !== undefined">
                  {{ runtime[group.name][f.key] }}
                </template>
                <template v-else>—</template>
              </span>
              <span class="un"></span>
            </div>

            <div
              v-else-if="runtime[group.name] && runtime[group.name][f.key] !== undefined"
              :class="['rw', valueClasses(runtime[group.name][f.key], f)]"
              :data-group="group.name"
              :data-key="f.key"
              :data-type="f.isBool ? 'bool' : f.isString ? 'string' : 'numeric'"
              :data-state="f.isBool ? boolState(runtime[group.name][f.key], f) : null"
            >
              <template v-if="f.isBool">
                <span
                  v-if="fieldVisible(f, 'label')"
                  class="lab bl"
                  :style="fieldCss(f, 'label')"
                >
                  <span
                    v-if="boolDotVisible(runtime[group.name][f.key], f)"
                    class="bd"
                    :style="boolDotStyle(runtime[group.name][f.key], f)"
                  ></span>
                  {{ f.label }}
                </span>
                <span v-else class="lab bl"></span>

                <span
                  v-if="showBoolStateText && fieldVisible(f, 'state')"
                  class="val"
                  :style="fieldCss(f, 'state')"
                >
                  {{ formatBool(runtime[group.name][f.key], f) }}
                </span>
                <span v-else class="val"></span>

                <span
                  v-if="fieldVisible(f, 'unit', false)"
                  class="un"
                  :style="fieldCss(f, 'unit')"
                ></span>
                <span v-else class="un"></span>
              </template>
              <template v-else>
                <span
                  v-if="fieldVisible(f, 'label')"
                  class="lab"
                  :style="fieldCss(f, 'label')"
                >
                  {{ f.label }}
                </span>
                <span v-else class="lab"></span>

                <span
                  v-if="fieldVisible(f, 'values')"
                  class="val"
                  :style="fieldCss(f, 'values')"
                >
                  {{ formatValue(runtime[group.name][f.key], f) }}
                </span>
                <span v-else class="val"></span>

                <span
                  v-if="fieldVisible(f, 'unit', !!f.unit)"
                  class="un"
                  :style="fieldCss(f, 'unit')"
                >
                  {{ f.unit }}
                </span>
                <span v-else class="un"></span>
              </template>
            </div>
          </template>
        </div>

        <hr v-if="group.name === 'system' && runtime.uptime !== undefined" class="dv" />

        <p v-if="group.name === 'system' && runtime.uptime !== undefined" class="uptime">
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
      Mode: {{ wsConnected ? 'WebSocket' : 'Polling' }}
    </div>
  </section>
</template>

<script setup>
import { computed, inject, onBeforeUnmount, onMounted, ref, watch } from 'vue';

import RuntimeActionButton from './runtime/RuntimeActionButton.vue';
import RuntimeCheckbox from './runtime/RuntimeCheckbox.vue';
import RuntimeNumberInput from './runtime/RuntimeNumberInput.vue';
import RuntimeSlider from './runtime/RuntimeSlider.vue';
import RuntimeStateButton from './runtime/RuntimeStateButton.vue';

const props = defineProps({
  config: {
    type: Object,
    default: () => ({}),
  },
});

const emit = defineEmits(['can-flash-change']);

const notify = inject('notify', () => {});
const updateToast = inject('updateToast', () => {});
const dismissToast = inject('dismissToast', () => {});

const runtime = ref({});
const runtimeMeta = ref([]);
const runtimeGroups = ref([]);
const showBoolStateText = ref(false);
const flashing = ref(false);
const otaFileInput = ref(null);
const wsConnected = ref(false);

let pollTimer = null;
let ws = null;
let wsRetry = 0;

let checkboxDebounceTimer = null;

const rURIComp = encodeURIComponent;

const displayRuntimeGroups = computed(() =>
  runtimeGroups.value.filter((group) => groupHasVisibleContent(group))
);

const canFlash = computed(() => {
  let allow = false;
  const systemConfig = props.config?.System;
  if (
    systemConfig &&
    Object.prototype.hasOwnProperty.call(systemConfig, 'OTAEn') &&
    systemConfig.OTAEn &&
    typeof systemConfig.OTAEn.value !== 'undefined'
  ) {
    allow = !!systemConfig.OTAEn.value;
  } else if (
    runtime.value.system &&
    typeof runtime.value.system.allowOTA !== 'undefined'
  ) {
    allow = !!runtime.value.system.allowOTA;
  }
  return allow && !flashing.value;
});

watch(
  canFlash,
  (val) => {
    emit('can-flash-change', val);
  },
  { immediate: true }
);

watch(showBoolStateText, (val) => {
  try {
    if (typeof window !== 'undefined' && window.localStorage) {
      window.localStorage.setItem('cm_showBoolStateText', val ? '1' : '0');
    }
  } catch (e) {
    /* ignore */
  }
});

function normalizeStyle(style) {
  if (!style || typeof style !== 'object') return null;
  const normalized = {};
  Object.entries(style).forEach(([ruleKey, ruleValue]) => {
    if (!ruleValue || typeof ruleValue !== 'object') return;
    const { visible, ...rest } = ruleValue;
    const cssProps = { ...rest };
    normalized[ruleKey] = {
      css: cssProps,
      visible: Object.prototype.hasOwnProperty.call(ruleValue, 'visible')
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

function notifySafe(message, type = 'info', duration = 3000, sticky = false, id = null) {
  return typeof notify === 'function'
    ? notify(message, type, duration, sticky, id)
    : null;
}

function updateToastSafe(id, message, type = 'info', duration = 2500) {
  if (typeof updateToast === 'function') {
    updateToast(id, message, type, duration);
  }
}

function dismissToastSafe(id) {
  if (typeof dismissToast === 'function') {
    dismissToast(id);
  }
}

async function loadInitialPreferences() {
  try {
    if (typeof window !== 'undefined' && window.localStorage) {
      const stored = window.localStorage.getItem('cm_showBoolStateText');
      if (stored === '1') {
        showBoolStateText.value = true;
      } else if (stored === '0') {
        showBoolStateText.value = false;
      }
    }
  } catch (e) {
    /* ignore */
  }
}

function initLive() {
  const proto = typeof location !== 'undefined' && location.protocol === 'https:' ? 'wss://' : 'ws://';
  const url = proto + (typeof location !== 'undefined' ? location.host : '') + '/ws';
  startWebSocket(url);
}

function startWebSocket(url) {
  try {
    if (ws) {
      try {
        ws.close();
      } catch (e) {
        /* ignore */
      }
    }
    ws = new WebSocket(url);
    ws.onopen = () => {
      wsConnected.value = true;
      wsRetry = 0;
      setTimeout(() => {
        if (!runtime.value.uptime) fetchRuntime();
      }, 300);
    };
    ws.onclose = () => {
      wsConnected.value = false;
      scheduleWsReconnect(url);
    };
    ws.onerror = () => {
      wsConnected.value = false;
      scheduleWsReconnect(url);
    };
    ws.onmessage = (ev) => {
      try {
        runtime.value = JSON.parse(ev.data);
        buildRuntimeGroups();
      } catch (e) {
        /* ignore */
      }
    };
  } catch (e) {
    scheduleWsReconnect(url);
  }
}

function scheduleWsReconnect(url) {
  const delay = Math.min(5000, 300 + wsRetry * wsRetry * 200);
  wsRetry++;
  setTimeout(() => {
    startWebSocket(url);
  }, delay);
  if (!pollTimer) {
    fallbackPolling();
  }
}

async function fetchRuntime() {
  try {
    const r = await fetch('/runtime.json?ts=' + Date.now());
    if (!r.ok) return;
    runtime.value = await r.json();
    buildRuntimeGroups();
  } catch (e) {
    /* ignore */
  }
}

async function fetchRuntimeMeta() {
  try {
    const r = await fetch('/runtime_meta.json?ts=' + Date.now());
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
      if (!grouped[m.group]) {
        grouped[m.group] = {
          name: m.group,
          title: capitalize(m.group),
          fields: [],
        };
      }
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
          typeof m.boolAlarmValue === 'boolean' ? !!m.boolAlarmValue : undefined,
        isString: m.isString || false,
        isDivider: m.isDivider || false,
        staticValue: m.staticValue || '',
        order: m.order !== undefined ? m.order : 100,
        style: m.style || null,
        styleRules: normalizeStyle(m.style || null),
      });
    }

    try {
      const sys = runtime.value.system || {};
      if (grouped.system) {
        const existingKeys = new Set(grouped.system.fields.map((f) => f.key));
        Object.keys(sys).forEach((k) => {
          if (!existingKeys.has(k)) {
            grouped.system.fields.push({
              key: k,
              label: capitalize(k),
              unit: k === 'freeHeap' ? 'KB' : k === 'loopAvg' ? 'ms' : '',
              precision: k === 'loopAvg' ? 2 : 0,
              order: 999,
              isBool: typeof sys[k] === 'boolean',
              isString: typeof sys[k] === 'string',
              isButton: false,
              isCheckbox: false,
              isDivider: false,
              staticValue: '',
              style: null,
              styleRules: null,
            });
          }
        });
      } else if (Object.keys(sys).length) {
        grouped.system = {
          name: 'system',
          title: 'System',
          fields: Object.keys(sys).map((k) => ({
            key: k,
            label: capitalize(k),
            unit: k === 'freeHeap' ? 'KB' : k === 'loopAvg' ? 'ms' : '',
            precision: k === 'loopAvg' ? 2 : 0,
            order: 999,
            isBool: typeof sys[k] === 'boolean',
            isString: typeof sys[k] === 'string',
            isButton: false,
            isCheckbox: false,
            isDivider: false,
            staticValue: '',
            style: null,
            styleRules: null,
          })),
        };
      }
    } catch (e) {
      /* ignore */
    }

    runtimeGroups.value = Object.values(grouped).map((g) => {
      g.fields.sort((a, b) => {
        if (a.isDivider && b.isDivider) return a.order - b.order;
        if (a.order === b.order) return a.label.localeCompare(b.label);
        return a.order - b.order;
      });
      return g;
    });

    const extraCards = {};
    for (const g of runtimeGroups.value) {
      g.fields = g.fields.filter((f) => {
        if (f.card) {
          if (!extraCards[f.card]) {
            extraCards[f.card] = {
              name: f.card,
              title: capitalize(f.card),
              fields: [],
            };
          }
          extraCards[f.card].fields.push(f);
          return false;
        }
        return true;
      });
    }

    for (const cardName in extraCards) {
      extraCards[cardName].fields.sort((a, b) => {
        if (a.order === b.order) return a.label.localeCompare(b.label);
        return a.order - b.order;
      });
      runtimeGroups.value.push(extraCards[cardName]);
    }
    return;
  }

  const fallback = [];
  if (runtime.value.sensors) {
    fallback.push({
      name: 'sensors',
      title: 'Sensors',
      fields: Object.keys(runtime.value.sensors).map((k) => ({
        key: k,
        label: capitalize(k),
        unit: '',
        precision:
          k.toLowerCase().includes('temp') || k.toLowerCase().includes('dew')
            ? 1
            : 0,
      })),
    });
  }
  if (runtime.value.system) {
    fallback.push({
      name: 'system',
      title: 'System',
      fields: Object.keys(runtime.value.system).map((k) => ({
        key: k,
        label: capitalize(k),
        unit: '',
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

async function triggerRuntimeButton(group, key) {
  try {
    const res = await fetch(`/runtime_action/button?group=${rURIComp(group)}&key=${rURIComp(key)}`, {
      method: 'POST',
    });
    if (!res.ok) {
      notifySafe(`Button failed: ${group}/${key}`, 'error');
      return;
    }
    notifySafe(`Button: ${key}`, 'success', 1500);
    fetchRuntime();
  } catch (e) {
    notifySafe(`Button error: ${e.message}`, 'error');
  }
}

function handleRuntimeButton(payload) {
  triggerRuntimeButton(payload.group, payload.key);
}

async function onStateButton(group, f, nextOverride = null) {
  const cur = runtime.value[group] && runtime.value[group][f.key];
  const next = nextOverride === null ? !cur : !!nextOverride;
  try {
    const res = await fetch(
      `/runtime_action/state_button?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${next ? 'true' : 'false'}`,
      {
        method: 'POST',
      }
    );
    if (!res.ok) {
      notifySafe(`State btn failed: ${f.key}`, 'error');
      return;
    }
    if (!runtime.value[group]) runtime.value[group] = {};
    runtime.value[group][f.key] = next;
    notifySafe(`${f.key}: ${next ? 'ON' : 'OFF'}`, 'info', 1200);
  } catch (e) {
    notifySafe(`State btn error: ${e.message}`, 'error');
  }
}

function handleStateToggle({ group, field, nextValue }) {
  onStateButton(group, field, nextValue);
}

async function sendInt(group, f, val) {
  try {
    const r = await fetch(`/runtime_action/int_slider?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${val}`, {
      method: 'POST',
    });
    if (!r.ok) {
      notifySafe(`Set failed: ${f.key}`, 'error');
    } else {
      if (!runtime.value[group]) runtime.value[group] = {};
      runtime.value[group][f.key] = val;
      notifySafe(`${f.key}=${val}`, 'success', 1200);
    }
  } catch (e) {
    notifySafe(`Set error ${f.key}: ${e.message}`, 'error');
  }
}

async function handleSliderCommit({ group, field, value }) {
  if (field.isFloatSlider) {
    await sendFloat(group, field, value);
  } else {
    await sendInt(group, field, value);
  }
}

async function sendFloat(group, f, val) {
  try {
    const r = await fetch(`/runtime_action/float_slider?group=${rURIComp(group)}&key=${rURIComp(f.key)}&value=${val}`, {
      method: 'POST',
    });
    if (!r.ok) {
      notifySafe(`Set failed: ${f.key}`, 'error');
    } else {
      if (!runtime.value[group]) runtime.value[group] = {};
      runtime.value[group][f.key] = val;
      notifySafe(`${f.key}=${val}`, 'success', 1200);
    }
  } catch (e) {
    notifySafe(`Set error ${f.key}: ${e.message}`, 'error');
  }
}

async function handleInputCommit({ group, field, value }) {
  if (field.isFloatInput) {
    await sendFloat(group, field, value);
  } else {
    await sendInt(group, field, value);
  }
}

async function onRuntimeCheckbox(group, key, value) {
  if (checkboxDebounceTimer) clearTimeout(checkboxDebounceTimer);
  checkboxDebounceTimer = setTimeout(async () => {
    try {
      const res = await fetch(`/runtime_action/checkbox?group=${rURIComp(group)}&key=${rURIComp(key)}&value=${value ? 'true' : 'false'}`, {
        method: 'POST',
      });
      if (!res.ok) {
        notifySafe(`Toggle failed: ${group}/${key}`, 'error');
        return;
      }
      notifySafe(`${key}: ${value ? 'ON' : 'OFF'}`, 'info', 1200);
      fetchRuntime();
    } catch (e) {
      notifySafe(`Toggle error ${key}: ${e.message}`, 'error');
    }
  }, 160);
}

function handleCheckboxChange({ group, field, checked }) {
  onRuntimeCheckbox(group, field.key, checked);
}

function capitalize(s) {
  if (!s) return '';
  return s.charAt(0).toUpperCase() + s.slice(1);
}

function formatValue(val, meta) {
  if (val == null) return '';
  if (typeof val === 'number') {
    if (meta && typeof meta.precision === 'number' && !Number.isInteger(val)) {
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
  return val ? 'On' : 'Off';
}

function severityClass(val, meta) {
  if (typeof val !== 'number' || !meta) return '';
  if (
    meta.alarmMin !== undefined &&
    meta.alarmMin !== null &&
    meta.alarmMin !== '' &&
    meta.alarmMin !== false &&
    !Number.isNaN(meta.alarmMin) &&
    val < meta.alarmMin
  )
    return 'sev-alarm';
  if (
    meta.alarmMax !== undefined &&
    meta.alarmMax !== null &&
    meta.alarmMax !== '' &&
    meta.alarmMax !== false &&
    !Number.isNaN(meta.alarmMax) &&
    val > meta.alarmMax
  )
    return 'sev-alarm';
  if (
    meta.warnMin !== undefined &&
    meta.warnMin !== null &&
    !Number.isNaN(meta.warnMin) &&
    val < meta.warnMin
  )
    return 'sev-warn';
  if (
    meta.warnMax !== undefined &&
    meta.warnMax !== null &&
    !Number.isNaN(meta.warnMax) &&
    val > meta.warnMax
  )
    return 'sev-warn';
  return '';
}

function valueClasses(val, meta) {
  if (meta && meta.isBool) {
    const classes = ['br'];
    if (
      typeof meta.boolAlarmValue === 'boolean' &&
      !!val === meta.boolAlarmValue
    ) {
      classes.push('sev-alarm');
    }
    return classes.join(' ');
  }
  return severityClass(val, meta);
}

function boolState(val, meta) {
  if (!meta || !meta.isBool) return '';
  const v = !!val;
  if (typeof meta.boolAlarmValue === 'boolean' && v === meta.boolAlarmValue)
    return 'alarm';
  return v ? 'on' : 'off';
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
    typeof meta.boolAlarmValue === 'boolean' &&
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

function startFlash() {
  if (!canFlash.value) {
    notifySafe('OTA is disabled', 'error');
    return;
  }
  if (!otaFileInput.value) {
    notifySafe('Browser file input not ready.', 'error');
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
    notifySafe('OTA is disabled', 'error');
    return;
  }
  if (file.size === 0) {
    notifySafe('Selected file is empty.', 'error');
    otaFileInput.value.value = '';
    return;
  }

  let password = window.prompt('Enter OTA password', '');
  if (password === null) {
    otaFileInput.value.value = '';
    return;
  }
  password = password.trim();

  const headers = new Headers();
  if (password.length) {
    headers.append('X-OTA-PASSWORD', password);
  }

  const form = new FormData();
  form.append('firmware', file, file.name);

  flashing.value = true;
  const toastId = notifySafe(`Uploading ${file.name}...`, 'info', 15000, true);
  try {
    const response = await fetch('/ota_update', {
      method: 'POST',
      headers,
      body: form,
    });

    let payload = {};
    const text = await response.text();
    try {
      payload = text ? JSON.parse(text) : {};
    } catch (e) {
      payload = { status: 'error', reason: text || 'invalid_response' };
    }

    if (response.status === 401) {
      updateToastSafe(toastId, 'Unauthorized: wrong OTA password.', 'error', 6000);
    } else if (response.status === 403 || payload.reason === 'ota_disabled') {
      updateToastSafe(toastId, 'OTA disabled', 'error', 6000);
    } else if (!response.ok || payload.status !== 'ok') {
      const reason = payload.reason || response.statusText || 'Upload failed';
      updateToastSafe(toastId, `Flash failed: ${reason}`, 'error', 6000);
    } else {
      updateToastSafe(toastId, 'Flash done!', 'success', 9000);
      notifySafe('Flash done!', 'success', 6000);
      notifySafe('Waiting for reboot...', 'info', 8000);
    }
  } catch (error) {
    updateToastSafe(toastId, `Flash failed: ${error.message}`, 'error', 6000);
  } finally {
    flashing.value = false;
    if (otaFileInput.value) otaFileInput.value.value = '';
  }
}

onMounted(() => {
  loadInitialPreferences();
  initLive();
  fetchRuntime();
  fetchRuntimeMeta();
});

onBeforeUnmount(() => {
  if (pollTimer) clearInterval(pollTimer);
  if (ws) ws.close();
  if (checkboxDebounceTimer) clearTimeout(checkboxDebounceTimer);
});

defineExpose({
  startFlash,
  canFlash,
});
</script>

<style scoped>
.hidden-file-input {
  display: none;
}

.live-cards {
  display: flex;
  flex-wrap: wrap;
  gap: 1rem;
  justify-content: center;
}

.live-status {
  text-align: center;
  margin-top: 1rem;
  font-size: 0.85rem;
  color: #555;
}

.card {
  background: #fff;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.15);
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

.rw.sl input[type='range'] {
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

.uptime {
  font-size: 0.85rem;
  color: #555;
  margin: 0.3rem 0 0.15rem;
  padding: 0.25rem 0.4rem 0.15rem;
  text-align: center;
}

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
  content: '';
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

.btn {
  background: #1976d2;
  color: #fff;
  border: none;
  padding: 0.25rem 0.6rem;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.75rem;
  font-weight: 600;
}

.btn:hover {
  background: #125a9f;
}

.live-view h3 {
  color: orange;
  text-decoration: underline;
  margin: 0.5rem auto;
}
</style>
