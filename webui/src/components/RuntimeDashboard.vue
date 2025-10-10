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
                <template
                  v-else-if="
                    runtime[group.name] &&
                    runtime[group.name][f.key] !== undefined
                  "
                >
                  {{ runtime[group.name][f.key] }}
                </template>
                <template v-else>—</template>
              </span>
              <span class="un"></span>
            </div>

            <div
              v-else-if="
                runtime[group.name] && runtime[group.name][f.key] !== undefined
              "
              :class="['rw', valueClasses(runtime[group.name][f.key], f)]"
              :data-group="group.name"
              :data-key="f.key"
              :data-type="f.isBool ? 'bool' : f.isString ? 'string' : 'numeric'"
              :data-state="
                f.isBool ? boolState(runtime[group.name][f.key], f) : null
              "
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
                    :class="boolDotClasses(runtime[group.name][f.key], f)"
                    :style="boolDotStyle(runtime[group.name][f.key], f)"
                  ></span>
                  {{ f.label }}
                </span>
                <span v-else class="lab bl"></span>

                <span
                  v-if="hasVisibleAlarm && showBoolStateText && fieldVisible(f, 'state')"
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
          v-if="
            group.name === 'system' &&
            runtime.system &&
            runtime.system.loopAvg !== undefined
          "
          class="uptime"
        >
          Loop Avg:
          {{
            typeof runtime.system.loopAvg === "number"
              ? runtime.system.loopAvg.toFixed(2)
              : runtime.system.loopAvg
          }}
          ms
        </p>
        <p v-if="group.name === 'system'" class="uptime ota-status">
          OTA:
          <span
            v-if="otaEndpointAvailable === null"
            class="badge off"
            title="probing..."
            >checking...</span
          >
          <span
            v-else
            :class="[
              'badge',
              otaEnabled
                ? runtime.system?.otaActive
                  ? 'on-active'
                  : 'on'
                : 'off',
            ]"
            :title="
              otaEnabled
                ? runtime.system?.otaActive
                  ? 'OTA server active'
                  : 'OTA enabled (not active yet)'
                : 'OTA disabled'
            "
          >
            {{
              otaEnabled
                ? runtime.system?.otaActive
                  ? "active"
                  : "enabled"
                : "disabled"
            }}
          </span>
        </p>

        <div
          v-if="group.name === 'system' && runtime.uptime !== undefined && hasVisibleAlarm"
          class="tbl"
        >
          <div class="rw cr">
            <span class="lab">Show state text</span>
            <label class="switch val">
              <input
                type="checkbox"
                v-model="showBoolStateText"
                class="switch"
              />
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
</template>

<script setup>
import { computed, inject, onBeforeUnmount, onMounted, ref, watch } from "vue";

import RuntimeActionButton from "./runtime/RuntimeActionButton.vue";
import RuntimeCheckbox from "./runtime/RuntimeCheckbox.vue";
import RuntimeNumberInput from "./runtime/RuntimeNumberInput.vue";
import RuntimeSlider from "./runtime/RuntimeSlider.vue";
import RuntimeStateButton from "./runtime/RuntimeStateButton.vue";

const props = defineProps({
  config: {
    type: Object,
    default: () => ({}),
  },
});

const emit = defineEmits(["can-flash-change"]);

const notify = inject("notify", () => {});
const updateToast = inject("updateToast", () => {});
const dismissToast = inject("dismissToast", () => {});

const runtime = ref({});
const runtimeMeta = ref([]);
const runtimeGroups = ref([]);
const showBoolStateText = ref(false);
const flashing = ref(false);
const otaFileInput = ref(null);
const wsConnected = ref(false);
const otaEndpointAvailable = ref(null); // null = unknown, true = reachable & not 403-disabled, false = definitely disabled/absent

const builtinSystemHiddenFields = new Set(["loopAvg"]);

let pollTimer = null;
let ws = null;
let wsRetry = 0;

let checkboxDebounceTimer = null;

const rURIComp = encodeURIComponent;

const displayRuntimeGroups = computed(() =>
  runtimeGroups.value.filter((group) => groupHasVisibleContent(group))
);

// Show the boolean state-text toggle only if at least one alarm-capable field is present and enabled
const hasVisibleAlarm = computed(() => {
  try {
    if (!Array.isArray(runtimeMeta.value)) return false;
    for (const g of runtimeMeta.value) {
      const fields = Array.isArray(g?.fields) ? g.fields : [];
      for (const f of fields) {
        const enabled = f?.enabled !== false; // undefined -> enabled
        if (!enabled) continue;
        // Boolean alarm (either explicit flag or semantics)
        if (f?.isBool && (f?.hasAlarm || f?.alarmWhenTrue)) return true;
        // Numeric thresholds (warn/alarm bounds present)
        if (
          typeof f?.warnMin !== 'undefined' ||
          typeof f?.warnMax !== 'undefined' ||
          typeof f?.alarmMin !== 'undefined' ||
          typeof f?.alarmMax !== 'undefined'
        ) {
          return true;
        }
      }
    }
  } catch (e) {}
  return false;
});

// Replace previous canFlash and otaEnabled definitions to include otaEndpointAvailable gating
const canFlash = computed(() => {
  let enabled = false;
  try {
    if (runtime.value?.system) {
      if (
        Object.prototype.hasOwnProperty.call(runtime.value.system, "otaActive")
      ) {
        enabled = !!runtime.value.system.otaActive;
      } else if (
        Object.prototype.hasOwnProperty.call(runtime.value.system, "allowOTA")
      ) {
        enabled = !!runtime.value.system.allowOTA;
      }
    }
  } catch (e) {}
  if (!enabled) {
    const systemConfig = props.config?.System;
    if (
      systemConfig &&
      Object.prototype.hasOwnProperty.call(systemConfig, "OTAEn") &&
      systemConfig.OTAEn &&
      typeof systemConfig.OTAEn.value !== "undefined"
    ) {
      enabled = !!systemConfig.OTAEn.value;
    }
  }
  if (runtimeMeta.value.length) {
    const systemMeta = runtimeMeta.value.find(
      (group) => group.name === "system"
    );
    if (systemMeta) {
      const field = systemMeta.fields.find((f) => f.key === "OTAEn");
      if (field && field.enabled !== undefined) {
        enabled = !!field.enabled;
      }
    }
  }
  // Gate by endpoint availability if probed
  if (otaEndpointAvailable.value === false) enabled = false;
  return enabled && !flashing.value;
});

const otaEnabled = computed(() => {
  let enabled = false;
  try {
    if (runtime.value?.system) {
      if (
        Object.prototype.hasOwnProperty.call(runtime.value.system, "otaActive")
      ) {
        enabled = !!runtime.value.system.otaActive;
      } else if (
        Object.prototype.hasOwnProperty.call(runtime.value.system, "allowOTA")
      ) {
        enabled = !!runtime.value.system.allowOTA;
      }
    }
  } catch (e) {}
  if (!enabled) {
    const systemConfig = props.config?.System;
    if (
      systemConfig &&
      Object.prototype.hasOwnProperty.call(systemConfig, "OTAEn") &&
      systemConfig.OTAEn &&
      typeof systemConfig.OTAEn.value !== "undefined"
    ) {
      enabled = !!systemConfig.OTAEn.value;
    }
  }
  if (runtimeMeta.value.length) {
    const systemMeta = runtimeMeta.value.find(
      (group) => group.name === "system"
    );
    if (systemMeta) {
      const field = systemMeta.fields.find((f) => f.key === "OTAEn");
      if (field && field.enabled !== undefined) {
        enabled = !!field.enabled;
      }
    }
  }
  if (otaEndpointAvailable.value === false) enabled = false;
  return enabled;
});

watch(canFlash, (val) => {
  emit("can-flash-change", val);
});
watch(otaEnabled, (v) => emit("can-flash-change", v && !flashing.value));

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

function notifySafe(
  message,
  type = "info",
  duration = 3000,
  sticky = false,
  id = null
) {
  return typeof notify === "function"
    ? notify(message, type, duration, sticky, id)
    : null;
}

function updateToastSafe(id, message, type = "info", duration = 2500) {
  if (typeof updateToast === "function") {
    updateToast(id, message, type, duration);
  }
}

function dismissToastSafe(id) {
  if (typeof dismissToast === "function") {
    dismissToast(id);
  }
}

async function loadInitialPreferences() {
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
}

function initLive() {
  const proto =
    typeof location !== "undefined" && location.protocol === "https:"
      ? "wss://"
      : "ws://";
  const url =
    proto + (typeof location !== "undefined" ? location.host : "") + "/ws";
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
      if (m.group === "system" && builtinSystemHiddenFields.has(m.key)) {
        continue;
      }
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

    try {
      const sys = runtime.value.system || {};
      if (grouped.system) {
        grouped.system.fields = grouped.system.fields.filter(
          (f) => !builtinSystemHiddenFields.has(f.key)
        );
        const existingKeys = new Set(grouped.system.fields.map((f) => f.key));
        Object.keys(sys).forEach((k) => {
          if (builtinSystemHiddenFields.has(k)) return;
          if (!existingKeys.has(k)) {
            grouped.system.fields.push({
              key: k,
              label: capitalize(k),
              unit: k === "freeHeap" ? "KB" : k === "loopAvg" ? "ms" : "",
              precision: k === "loopAvg" ? 2 : 0,
              order: 999,
              isBool: typeof sys[k] === "boolean",
              isString: typeof sys[k] === "string",
              isButton: false,
              isCheckbox: false,
              isDivider: false,
              staticValue: "",
              style: null,
              styleRules: null,
            });
          }
        });
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
      fields: Object.keys(runtime.value.system)
        .filter((k) => !builtinSystemHiddenFields.has(k))
        .map((k) => ({
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
  return group.fields.some((field) =>
    fieldHasVisibleContent(group.name, field)
  );
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
    const res = await fetch(
      `/runtime_action/button?group=${rURIComp(group)}&key=${rURIComp(key)}`,
      {
        method: "POST",
      }
    );
    if (!res.ok) {
      notifySafe(`Button failed: ${group}/${key}`, "error");
      return;
    }
    notifySafe(`Button: ${key}`, "success", 1500);
    fetchRuntime();
  } catch (e) {
    notifySafe(`Button error: ${e.message}`, "error");
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
      `/runtime_action/state_button?group=${rURIComp(group)}&key=${rURIComp(
        f.key
      )}&value=${next ? "true" : "false"}`,
      {
        method: "POST",
      }
    );
    if (!res.ok) {
      notifySafe(`State btn failed: ${f.key}`, "error");
      return;
    }
    if (!runtime.value[group]) runtime.value[group] = {};
    runtime.value[group][f.key] = next;
    notifySafe(`${f.key}: ${next ? "ON" : "OFF"}`, "info", 1200);
  } catch (e) {
    notifySafe(`State btn error: ${e.message}`, "error");
  }
}

function handleStateToggle({ group, field, nextValue }) {
  onStateButton(group, field, nextValue);
}

async function sendInt(group, f, val) {
  try {
    const r = await fetch(
      `/runtime_action/int_slider?group=${rURIComp(group)}&key=${rURIComp(
        f.key
      )}&value=${val}`,
      {
        method: "POST",
      }
    );
    if (!r.ok) {
      notifySafe(`Set failed: ${f.key}`, "error");
    } else {
      if (!runtime.value[group]) runtime.value[group] = {};
      runtime.value[group][f.key] = val;
      notifySafe(`${f.key}=${val}`, "success", 1200);
    }
  } catch (e) {
    notifySafe(`Set error ${f.key}: ${e.message}`, "error");
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
    const r = await fetch(
      `/runtime_action/float_slider?group=${rURIComp(group)}&key=${rURIComp(
        f.key
      )}&value=${val}`,
      {
        method: "POST",
      }
    );
    if (!r.ok) {
      notifySafe(`Set failed: ${f.key}`, "error");
    } else {
      if (!runtime.value[group]) runtime.value[group] = {};
      runtime.value[group][f.key] = val;
      notifySafe(`${f.key}=${val}`, "success", 1200);
    }
  } catch (e) {
    notifySafe(`Set error ${f.key}: ${e.message}`, "error");
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
      const res = await fetch(
        `/runtime_action/checkbox?group=${rURIComp(group)}&key=${rURIComp(
          key
        )}&value=${value ? "true" : "false"}`,
        {
          method: "POST",
        }
      );
      if (!res.ok) {
        notifySafe(`Toggle failed: ${group}/${key}`, "error");
        return;
      }
      notifySafe(`${key}: ${value ? "ON" : "OFF"}`, "info", 1200);
      fetchRuntime();
    } catch (e) {
      notifySafe(`Toggle error ${key}: ${e.message}`, "error");
    }
  }, 160);
}

function handleCheckboxChange({ group, field, checked }) {
  onRuntimeCheckbox(group, field.key, checked);
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

function boolAlarmMatch(val, meta) {
  if (!meta) return false;
  if (typeof meta.boolAlarmValue !== "boolean") return false;
  return !!val === meta.boolAlarmValue;
}

function fallbackBoolDotRule(val, meta) {
  const boolVal = !!val;
  const hasAlarmValue = meta && typeof meta.boolAlarmValue === "boolean";
  const isAlarm = boolAlarmMatch(val, meta);

  const classes = ["bd--fallback"];

  if (isAlarm) {
    classes.push("bd--alarm");
  } else if (hasAlarmValue) {
    classes.push("bd--safe");
  } else if (boolVal) {
    classes.push("bd--on");
  } else {
    classes.push("bd--off");
  }

  if (hasAlarmValue) {
    classes.push("bd--has-alarm-value");
  }

  classes.push(boolVal ? "bd--value-true" : "bd--value-false");

  return {
    visible: true,
    css: null,
    classes,
  };
}

function selectBoolDotRule(val, meta) {
  const rules = ensureStyleRules(meta);
  if (!rules) return fallbackBoolDotRule(val, meta);
  // Helper to check if a rule actually defines any visuals (classes or css)
  const ruleHasVisuals = (rule) => {
    if (!rule) return false;
    if (Array.isArray(rule.classes) && rule.classes.length) return true;
    if (typeof rule.classes === "string" && rule.classes.trim().length) return true;
    if (
      typeof rule.className === "string" &&
      rule.className.trim &&
      rule.className.trim().length
    )
      return true;
    if (rule.css && typeof rule.css === "object" && Object.keys(rule.css).length)
      return true;
    return false;
  };
  const boolVal = !!val;
  const isAlarm = boolAlarmMatch(val, meta);
  let candidate = null;
  if (
    typeof meta.boolAlarmValue === "boolean" &&
    boolVal === meta.boolAlarmValue
  ) {
    candidate = rules.stateDotOnAlarm || candidate;
  }
  if (!candidate && boolVal && rules.stateDotOnTrue) candidate = rules.stateDotOnTrue;
  if (!candidate && !boolVal && rules.stateDotOnFalse) candidate = rules.stateDotOnFalse;
  if (!candidate && isAlarm && rules.stateDotOnTrue) candidate = rules.stateDotOnTrue;
  if (!candidate && isAlarm && rules.stateDotOnFalse) candidate = rules.stateDotOnFalse;
  if (!candidate && boolVal && rules.stateDotOnFalse) candidate = rules.stateDotOnFalse;
  if (!candidate && !boolVal && rules.stateDotOnTrue) candidate = rules.stateDotOnTrue;
  if (!candidate && rules.stateDotOnAlarm) candidate = rules.stateDotOnAlarm;
  // If selected rule has no visuals, fallback to default dot styles
  if (!ruleHasVisuals(candidate)) return fallbackBoolDotRule(val, meta);
  return candidate;
}

function boolDotVisible(val, meta) {
  const rule = selectBoolDotRule(val, meta);
  if (!rule) return false;
  if (rule.visible === undefined) return true;
  return !!rule.visible;
}

function boolDotClasses(val, meta) {
  const rule = selectBoolDotRule(val, meta);
  if (!rule) return [];
  if (Array.isArray(rule.classes)) return rule.classes;
  if (typeof rule.classes === "string") {
    return rule.classes
      .split(/\s+/)
      .map((c) => c.trim())
      .filter(Boolean);
  }
  if (typeof rule.className === "string" && rule.className.trim().length) {
    return [rule.className.trim()];
  }
  return [];
}

function boolDotStyle(val, meta) {
  const rule = selectBoolDotRule(val, meta);
  if (!rule || !rule.css || typeof rule.css !== "object") return {};
  return rule.css;
}

function fallbackPolling() {
  if (pollTimer) clearInterval(pollTimer);
  fetchRuntime();
  pollTimer = setInterval(fetchRuntime, 2000);
}

function startFlash() {
  if (!canFlash.value) {
    notifySafe("OTA is disabled", "error");
    return;
  }
  if (!otaFileInput.value) {
    notifySafe("Browser file input not ready.", "error");
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
    notifySafe("OTA is disabled", "error");
    return;
  }
  if (file.size === 0) {
    notifySafe("Selected file is empty.", "error");
    otaFileInput.value.value = "";
    return;
  }

  let password = window.prompt("Enter OTA password", "");
  if (password === null) {
    otaFileInput.value.value = "";
    return;
  }
  password = password.trim();

  const headers = new Headers();
  if (password.length) headers.append("X-OTA-PASSWORD", password);
  const form = new FormData();
  form.append("firmware", file, file.name);
  flashing.value = true;
  const toastId = notifySafe(`Uploading ${file.name}...`, "info", 15000, true);
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
      updateToastSafe(
        toastId,
        "Unauthorized: wrong OTA password.",
        "error",
        6000
      );
    } else if (response.status === 403 || payload.reason === "ota_disabled") {
      updateToastSafe(toastId, "OTA disabled", "error", 6000);
      otaEndpointAvailable.value = false;
    } else if (!response.ok || payload.status !== "ok") {
      const reason = payload.reason || response.statusText || "Upload failed";
      updateToastSafe(toastId, `Flash failed: ${reason}`, "error", 6000);
    } else {
      updateToastSafe(toastId, "Flash done!", "success", 9000);
      notifySafe("Flash done!", "success", 6000);
      notifySafe("Waiting for reboot...", "info", 8000);
    }
  } catch (error) {
    updateToastSafe(toastId, `Flash failed: ${error.message}`, "error", 6000);
  } finally {
    flashing.value = false;
    if (otaFileInput.value) otaFileInput.value.value = "";
  }
}

// --- OTA endpoint probing (restored) ---
let otaProbeTimer = null;
async function probeOtaEndpoint() {
  try {
    const r = await fetch("/ota_update", { method: "GET" });
    if (r.status === 404) {
      // Not compiled/exposed
      otaEndpointAvailable.value = false;
    } else if (r.status === 403) {
      // Explicitly disabled at runtime
      otaEndpointAvailable.value = false;
    } else {
      // Any other response (200/401 etc.) means endpoint exists
      otaEndpointAvailable.value = true;
    }
  } catch (e) {
    // Network error -> keep as null (unknown) so we retry
    if (otaEndpointAvailable.value === null) {
      // leave it
    }
  }
}

// Persist preference for showing bool state text
watch(showBoolStateText, (v) => {
  try {
    if (typeof window !== "undefined" && window.localStorage) {
      window.localStorage.setItem("cm_showBoolStateText", v ? "1" : "0");
    }
  } catch (e) {
    /* ignore */
  }
});

onMounted(() => {
  loadInitialPreferences();
  fetchRuntimeMeta();
  fetchRuntime();
  initLive();
  probeOtaEndpoint();
  otaProbeTimer = setInterval(probeOtaEndpoint, 20000);
  // Emit initial can-flash state early so parent can render the button correctly
  emit("can-flash-change", canFlash.value);
});

onBeforeUnmount(() => {
  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }
  if (otaProbeTimer) {
    clearInterval(otaProbeTimer);
    otaProbeTimer = null;
  }
  try {
    if (ws) ws.close();
  } catch (e) {
    /* ignore */
  }
});

// Expose startFlash so parent components can trigger the hidden file input
defineExpose({ startFlash });
</script>

<style scoped>
.live-view {
  padding: 0.75rem 0.5rem 2.5rem;
}
.live-cards {
  display: grid;
  gap: 1rem;
  /* Limit max column width so single cards don't stretch full width */
  grid-template-columns: repeat(auto-fit, minmax(260px, 420px));
  justify-content: center;
  align-items: start;
}
.card {
  background: #f5f6f8;
  border: 1px solid #30363d;
  border-radius: 10px;
  padding: 0.65rem 0.75rem 0.9rem;
  position: relative;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.25);
}
.card h3 {
  margin: 0 0 0.4rem;
  font-size: 0.95rem;
  letter-spacing: 0.5px;
  font-weight: 600;
}
.tbl {
  display: block;
  width: 100%;
}
.rw {
  display: grid;
  grid-template-columns: 1fr auto auto;
  align-items: center;
  font-size: 0.78rem;
  line-height: 1.25rem;
  padding: 2px 0 1px;
}
.rw.sl {
  --slider-control-gap: 0 0.3rem;
}
.rw.cr {
  margin-top: 0.35rem;
}
.rw .lab {
  font-weight: 500;
  padding-right: 0.4rem;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.rw .lab.bl {
  display: flex;
  align-items: center;
  gap: 0.4rem;
}
.rw .val {
  text-align: right;
  min-width: 2.2rem;
  font-variant-numeric: tabular-nums;
}
.rw .un {
  padding-left: 0.35rem;
  min-width: 1.6rem;
  text-align: left;
  opacity: 0.65;
  font-size: 0.7rem;
}
.rw.sev-warn .val {
  color: #d29922;
}
.rw.sev-alarm .val {
  color: #f85149;
  font-weight: 600;
}
.bd {
  width: 0.65rem;
  height: 0.65rem;
  border-radius: 50%;
  display: inline-block;
  box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.15) inset,
    0 0 2px rgba(0, 0, 0, 0.6);
}
.bd--fallback.bd--on {
  background: #238636;
}
.bd--fallback.bd--off {
  background: #6e7681;
}
.bd--fallback.bd--alarm {
  background: #f85149;
  animation: alarmPulse 1.1s ease-in-out infinite;
}
@keyframes alarmPulse {
  0%,
  100% {
    box-shadow: 0 0 0 1px rgba(248, 81, 73, 0.9),
      0 0 4px 2px rgba(248, 81, 73, 0.4);
  }
  50% {
    box-shadow: 0 0 0 1px rgba(248, 81, 73, 0.5),
      0 0 6px 3px rgba(248, 81, 73, 0.6);
  }
}
.dv {
  position: relative;
  border: none;
  border-top: 1px solid #30363d;
  margin: 0.55rem 0 0.4rem;
}
.dv::before {
  content: attr(data-label);
  position: absolute;
  left: 0;
  top: 50%;
  transform: translateY(-50%);
  background: #161b22;
  padding: 0 0.25rem;
  font-size: 0.62rem;
  letter-spacing: 0.5px;
  text-transform: uppercase;
  color: #8b949e;
}
.uptime {
  font-size: 0.64rem;
  letter-spacing: 0.5px;
  opacity: 0.9;
  margin: 0.25rem 0 0;
  font-weight: 500;
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.3rem;
}
.ota-status {
  margin-top: 0.25rem;
}
.badge {
  display: inline-block;
  line-height: 1.05rem;
  padding: 0 0.55em;
  font-size: 0.62rem;
  font-weight: 600;
  letter-spacing: 0.6px;
  border-radius: 0.8rem;
  background: #444c56;
  color: #fff;
  text-transform: uppercase;
  position: relative;
  top: 0;
}
.badge.on {
  background: #1f6feb;
}
.badge.on-active {
  background: #238636;
  animation: badgePulse 1.4s ease-in-out infinite;
}
.badge.off {
  background: #6e7681;
  opacity: 0.75;
}
@keyframes badgePulse {
  0%,
  100% {
    filter: brightness(1);
    box-shadow: 0 0 0 0 rgba(35, 134, 54, 0.5);
  }
  50% {
    filter: brightness(1.15);
    box-shadow: 0 0 0 4px rgba(35, 134, 54, 0);
  }
}
label.switch {
  width: 32px;
  height: 16px;
  position: relative;
  display: inline-block;
}
label.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}
label.switch .slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: #394049;
  transition: 0.3s;
  border-radius: 16px;
}
label.switch .slider:before {
  position: absolute;
  content: "";
  height: 12px;
  width: 12px;
  left: 2px;
  top: 2px;
  background: #fff;
  border-radius: 50%;
  transition: 0.3s;
}
label.switch input:checked + .slider {
  background: #1f6feb;
}
label.switch input:checked + .slider:before {
  transform: translateX(16px);
}
.hidden-file-input {
  display: none;
}
.live-status {
  position: fixed;
  bottom: 0;
  right: 0;
  background: #d3d4d6;
  border-top: 1px solid #30363d;
  border-left: 1px solid #30363d;
  padding: 0.35rem 0.65rem;
  font-size: 0.6rem;
  letter-spacing: 0.5px;
  opacity: 0.85;
  border-top-left-radius: 6px;
}
.tbl::-webkit-scrollbar {
  width: 8px;
  height: 8px;
}
.tbl::-webkit-scrollbar-track {
  background: transparent;
}
.tbl::-webkit-scrollbar-thumb {
  background: #30363d;
  border-radius: 4px;
}
</style>
