<template>
  <section class="live-view">
    <input
      ref="otaFileInput"
      type="file"
      accept=".bin,.bin.gz"
      class="hidden-file-input"
      @change="onFlashFileSelected"
    />

    <!-- OTA Password Modal -->
    <div v-if="showPasswordModal" class="modal-overlay" @click="cancelPasswordInput">
      <div class="modal-content" @click.stop>
        <h3>OTA Password Required</h3>
        <div class="password-input-container">
          <input
            ref="passwordInput"
            v-model="otaPassword"
            type="password"
            placeholder="Enter OTA password"
            @keyup.enter="confirmPasswordInput"
            @keyup.escape="cancelPasswordInput"
          />
        </div>
        <div class="modal-actions">
          <button @click="cancelPasswordInput" class="cancel-btn">Cancel</button>
          <button @click="confirmPasswordInput" class="confirm-btn">Continue</button>
        </div>
      </div>
    </div>

    <div v-if="isLogView" class="log-view">
      <div class="log-head">
        <span>{{ logEntries.length ? "Live logging (WebSocket)" : "Waiting for logs..." }}</span>
        <button type="button" class="clear-btn" @click="clearLogs">Clear</button>
      </div>
      <div class="log-list">
        <table v-if="useLogTable" class="log-table">
          <thead>
            <tr>
              <th v-if="showLogTime" class="col-ts">Time</th>
              <th class="col-level">Level</th>
              <th class="col-tag">Tag</th>
              <th class="col-msg">Message</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(entry, idx) in logEntries" :key="entry.id || idx" :data-level="entry.level">
              <td v-if="showLogTime" class="log-ts">
                <span v-if="entry.dt || entry.ts !== null">{{ entry.dt || entry.ts }}</span>
              </td>
              <td class="log-level" :class="levelClass(entry.level)" :data-level="entry.level">{{ entry.level }}</td>
              <td class="log-tag">
                <span v-if="entry.tag">[{{ entry.tag }}]</span>
              </td>
              <td class="log-msg">{{ entry.msg }}</td>
            </tr>
          </tbody>
        </table>

        <div v-else class="log-list-compact">
          <div v-for="(entry, idx) in logEntries" :key="entry.id || idx" class="log-row" :data-level="entry.level">
            <span v-if="showLogTime" class="log-ts">{{ entry.dt || entry.ts }}</span>
            <span class="log-level" :class="levelClass(entry.level)" :data-level="entry.level">{{ entry.level }}</span>
            <span v-if="entry.tag" class="log-tag">[{{ entry.tag }}]</span>
            <span class="log-msg">{{ entry.msg }}</span>
          </div>
        </div>
      </div>
    </div>

    <div v-else>
      <div v-if="layoutTabs.length" class="runtime-tabs" role="tablist" aria-label="Live pages">
        <button
          v-for="tab in layoutTabs"
          :key="tab.key"
          class="rt-tab"
          type="button"
          :class="{ active: tab.key === activeLivePage }"
          :aria-selected="tab.key === activeLivePage"
          @click="activeLivePage = tab.key"
        >
          {{ tab.title }}
        </button>
      </div>

      <div class="live-cards">
        <div class="card" v-for="card in displayRuntimeCards" :key="card.key">
          <h3>{{ card.title }}</h3>

          <div v-if="card.items && card.items.length" class="tbl">
            <template v-for="f in card.items" :key="f.key">
              <hr v-if="f.isDivider" class="dv" :data-label="f.label" />

              <RuntimeActionButton
                v-else-if="f.isButton"
                :group="fieldSourceGroup(f)"
                :field="f"
                @action="handleRuntimeButton"
              />

              <RuntimeMomentaryButton
                v-else-if="f.isMomentaryButton"
                :group="fieldSourceGroup(f)"
                :field="f"
                :value="runtimeValue(f)"
                @set="handleMomentarySet"
              />

              <RuntimeStateButton
                v-else-if="f.isStateButton"
                :group="fieldSourceGroup(f)"
                :field="f"
                :value="runtimeValue(f)"
                @toggle="handleStateToggle"
              />

              <RuntimeSlider
                v-else-if="f.isIntSlider || f.isFloatSlider"
                :group="fieldSourceGroup(f)"
                :field="f"
                :value="runtimeValue(f)"
                :mode="f.isFloatSlider ? 'float' : 'int'"
                @commit="handleSliderCommit"
              />

              <RuntimeNumberInput
                v-else-if="f.isIntInput || f.isFloatInput"
                :group="fieldSourceGroup(f)"
                :field="f"
                :value="runtimeValue(f)"
                :mode="f.isFloatInput ? 'float' : 'int'"
                @commit="handleInputCommit"
              />

              <RuntimeCheckbox
                v-else-if="f.isCheckbox"
                :group="fieldSourceGroup(f)"
                :field="f"
                :value="runtimeValue(f)"
                @change="handleCheckboxChange"
              />

              <div v-else-if="f.isString" class="rw str">
                <span class="lab">{{ f.label }}</span>
                <span class="val">
                  <template v-if="f.staticValue">{{ f.staticValue }}</template>
                  <template v-else-if="hasRuntimeValue(f)">
                    {{ runtimeValue(f) }}
                  </template>
                  <template v-else>—</template>
                </span>
                <span class="un"></span>
              </div>

              <div
                v-else-if="hasRuntimeValue(f)"
                :class="['rw', valueClasses(runtimeValue(f), f, fieldSourceGroup(f)), ...fieldClasses(f, 'row')]"
                :data-group="fieldSourceGroup(f)"
                :data-key="f.key"
                :data-type="f.isBool ? 'bool' : f.isString ? 'string' : 'numeric'"
                :data-state="f.isBool ? boolState(runtimeValue(f), f) : null"
              >
                <template v-if="f.isBool">
                  <span
                    v-if="fieldVisible(f, 'label')"
                    class="lab bl"
                    :class="fieldClasses(f, 'label')"
                    :style="fieldCss(f, 'label')"
                  >
                    <span
                      v-if="boolDotVisible(runtimeValue(f), f)"
                      class="bd"
                      :class="boolDotClasses(runtimeValue(f), f)"
                      :style="boolDotStyle(runtimeValue(f), f)"
                    ></span>
                    {{ f.label }}
                  </span>
                  <span v-else class="lab bl"></span>

                  <span
                    v-if="hasVisibleAlarm && showBoolStateText && fieldVisible(f, 'state')"
                    class="val"
                    :class="fieldClasses(f, 'state')"
                    :style="fieldCss(f, 'state')"
                  >
                    {{ formatBool(runtimeValue(f), f) }}
                  </span>
                  <span v-else class="val"></span>

                  <span
                    v-if="fieldVisible(f, 'unit', false)"
                    class="un"
                    :class="fieldClasses(f, 'unit')"
                    :style="fieldCss(f, 'unit')"
                  ></span>
                  <span v-else class="un"></span>
                </template>
                <template v-else>
                  <span
                    v-if="fieldVisible(f, 'label')"
                    class="lab"
                    :class="fieldClasses(f, 'label')"
                    :style="fieldCss(f, 'label')"
                  >
                    {{ f.label }}
                  </span>
                  <span v-else class="lab"></span>

                  <span
                    v-if="fieldVisible(f, 'values')"
                    class="val"
                    :class="fieldClasses(f, 'values')"
                    :style="fieldCss(f, 'values')"
                  >
                    {{ formatValue(runtimeValue(f), f) }}
                  </span>
                  <span v-else class="val"></span>

                  <span
                    v-if="fieldVisible(f, 'unit', !!f.unit)"
                    class="un"
                    :class="fieldClasses(f, 'unit')"
                    :style="fieldCss(f, 'unit')"
                  >
                    {{ f.unit }}
                  </span>
                  <span v-else class="un"></span>
                </template>
              </div>
            </template>
          </div>

          <div v-for="group in card.groups" :key="group.key" class="grp">
            <div v-if="group.title" class="grp-title">{{ group.title }}</div>
            <div class="tbl">
              <template v-for="f in group.fields" :key="f.key">
                <hr v-if="f.isDivider" class="dv" :data-label="f.label" />

                <RuntimeActionButton
                  v-else-if="f.isButton"
                  :group="fieldSourceGroup(f)"
                  :field="f"
                  @action="handleRuntimeButton"
                />

                <RuntimeMomentaryButton
                  v-else-if="f.isMomentaryButton"
                  :group="fieldSourceGroup(f)"
                  :field="f"
                  :value="runtimeValue(f)"
                  @set="handleMomentarySet"
                />

                <RuntimeStateButton
                  v-else-if="f.isStateButton"
                  :group="fieldSourceGroup(f)"
                  :field="f"
                  :value="runtimeValue(f)"
                  @toggle="handleStateToggle"
                />

                <RuntimeSlider
                  v-else-if="f.isIntSlider || f.isFloatSlider"
                  :group="fieldSourceGroup(f)"
                  :field="f"
                  :value="runtimeValue(f)"
                  :mode="f.isFloatSlider ? 'float' : 'int'"
                  @commit="handleSliderCommit"
                />

                <RuntimeNumberInput
                  v-else-if="f.isIntInput || f.isFloatInput"
                  :group="fieldSourceGroup(f)"
                  :field="f"
                  :value="runtimeValue(f)"
                  :mode="f.isFloatInput ? 'float' : 'int'"
                  @commit="handleInputCommit"
                />

                <RuntimeCheckbox
                  v-else-if="f.isCheckbox"
                  :group="fieldSourceGroup(f)"
                  :field="f"
                  :value="runtimeValue(f)"
                  @change="handleCheckboxChange"
                />

                <div v-else-if="f.isString" class="rw str">
                  <span class="lab">{{ f.label }}</span>
                  <span class="val">
                    <template v-if="f.staticValue">{{ f.staticValue }}</template>
                    <template v-else-if="hasRuntimeValue(f)">
                      {{ runtimeValue(f) }}
                    </template>
                    <template v-else>—</template>
                  </span>
                  <span class="un"></span>
                </div>

                <div
                  v-else-if="hasRuntimeValue(f)"
                  :class="['rw', valueClasses(runtimeValue(f), f, fieldSourceGroup(f)), ...fieldClasses(f, 'row')]"
                  :data-group="fieldSourceGroup(f)"
                  :data-key="f.key"
                  :data-type="f.isBool ? 'bool' : f.isString ? 'string' : 'numeric'"
                  :data-state="f.isBool ? boolState(runtimeValue(f), f) : null"
                >
                  <template v-if="f.isBool">
                    <span
                      v-if="fieldVisible(f, 'label')"
                      class="lab bl"
                      :class="fieldClasses(f, 'label')"
                      :style="fieldCss(f, 'label')"
                    >
                      <span
                        v-if="boolDotVisible(runtimeValue(f), f)"
                        class="bd"
                        :class="boolDotClasses(runtimeValue(f), f)"
                        :style="boolDotStyle(runtimeValue(f), f)"
                      ></span>
                      {{ f.label }}
                    </span>
                    <span v-else class="lab bl"></span>

                    <span
                      v-if="hasVisibleAlarm && showBoolStateText && fieldVisible(f, 'state')"
                      class="val"
                      :class="fieldClasses(f, 'state')"
                      :style="fieldCss(f, 'state')"
                    >
                      {{ formatBool(runtimeValue(f), f) }}
                    </span>
                    <span v-else class="val"></span>

                    <span
                      v-if="fieldVisible(f, 'unit', false)"
                      class="un"
                      :class="fieldClasses(f, 'unit')"
                      :style="fieldCss(f, 'unit')"
                    ></span>
                    <span v-else class="un"></span>
                  </template>
                  <template v-else>
                    <span
                      v-if="fieldVisible(f, 'label')"
                      class="lab"
                      :class="fieldClasses(f, 'label')"
                      :style="fieldCss(f, 'label')"
                    >
                      {{ f.label }}
                    </span>
                    <span v-else class="lab"></span>

                    <span
                      v-if="fieldVisible(f, 'values')"
                      class="val"
                      :class="fieldClasses(f, 'values')"
                      :style="fieldCss(f, 'values')"
                    >
                      {{ formatValue(runtimeValue(f), f) }}
                    </span>
                    <span v-else class="val"></span>

                    <span
                      v-if="fieldVisible(f, 'unit', !!f.unit)"
                      class="un"
                      :class="fieldClasses(f, 'unit')"
                      :style="fieldCss(f, 'unit')"
                    >
                      {{ f.unit }}
                    </span>
                    <span v-else class="un"></span>
                  </template>
                </div>
              </template>
            </div>
          </div>

          <hr
            v-if="cardHasRuntimeSource(card, 'system') && runtime.uptime !== undefined"
            class="dv"
          />

          <p
            v-if="cardHasRuntimeSource(card, 'system') && runtime.uptime !== undefined"
            class="uptime"
          >
            Uptime: {{ formatUptime(runtime.uptime) }}
          </p>
          <p
            v-if="
              cardHasRuntimeSource(card, 'system') &&
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
          <p v-if="cardHasRuntimeSource(card, 'system')" class="uptime ota-status">
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
            v-if="cardHasRuntimeSource(card, 'system') && runtime.uptime !== undefined && hasVisibleAlarm"
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
    </div>
    <div class="live-status">
      Mode: {{ wsConnected ? "WebSocket" : "Polling" }}
    </div>
  </section>
</template>

<script setup>
import { computed, inject, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";

import RuntimeActionButton from "./runtime/RuntimeActionButton.vue";
import RuntimeCheckbox from "./runtime/RuntimeCheckbox.vue";
import RuntimeMomentaryButton from "./runtime/RuntimeMomentaryButton.vue";
import RuntimeNumberInput from "./runtime/RuntimeNumberInput.vue";
import RuntimeSlider from "./runtime/RuntimeSlider.vue";
import RuntimeStateButton from "./runtime/RuntimeStateButton.vue";

const props = defineProps({
  config: {
    type: Object,
    default: () => ({}),
  },
  view: {
    type: String,
    default: "live",
  },
});

const emit = defineEmits(["can-flash-change"]);

const LOG_MAX_ENTRIES = 1000;

// Keep log buffer across view switches in browser memory.
const logStore = {
  entries: ref([]),
  enabled: ref(false),
};

const notify = inject("notify", () => {});
const updateToast = inject("updateToast", () => {});
const dismissToast = inject("dismissToast", () => {});

const runtime = ref({});
const runtimeMeta = ref([]);
const runtimeGroups = ref([]);
const liveLayout = ref(null);
const activeLivePage = ref("");
const logEntries = logStore.entries;
const logEnabled = logStore.enabled;
const showBoolStateText = ref(false);
const flashing = ref(false);
const otaFileInput = ref(null);
const wsConnected = ref(false);
const otaEndpointAvailable = ref(null); // null = unknown, true = reachable & not 403-disabled, false = definitely disabled/absent
const isLogView = computed(() => props.view === "log");
const showLogTime = computed(() => logEntries.value.some((e) => e.dt || e.ts !== null));
const useLogTable = computed(() => {
  if (typeof window === "undefined") return true;
  return window.matchMedia("(min-width: 768px)").matches;
});

function levelClass(level) {
  if (!level) return "";
  const upper = String(level).toUpperCase();
  if (upper === "WARN") return "lvl-warn";
  if (upper === "ERROR" || upper === "FATAL") return "lvl-error";
  return "";
}

// OTA Password Modal
const showPasswordModal = ref(false);
const otaPassword = ref('');
const passwordInput = ref(null);
const pendingOtaFile = ref(null);
const savedOtaPassword = ref('');

const builtinSystemHiddenFields = new Set(["loopAvg", "otaActive"]);

function formatUptime(uptimeMs) {
  const ms = Number(uptimeMs);
  if (!Number.isFinite(ms) || ms < 0) return "—";

  const totalSeconds = Math.floor(ms / 1000);
  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;

  if (days > 0) return `${days}d ${hours}h ${minutes}m ${seconds}s`;
  if (hours > 0) return `${hours}h ${minutes}m ${seconds}s`;
  if (minutes > 0) return `${minutes}m ${seconds}s`;
  return `${seconds}s`;
}

function normalizeGroupToken(value) {
  return String(value || '')
    .toLowerCase()
    .replace(/[^a-z0-9]/g, '');
}

function fieldSourceGroup(field) {
  if (!field) return '';
  if (typeof field.sourceGroup === 'string' && field.sourceGroup.length) {
    return field.sourceGroup;
  }
  if (typeof field.group === 'string' && field.group.length) {
    return field.group;
  }
  return '';
}

function createRuntimeField(meta, layoutGroup, sourceGroup) {
  return {
    key: meta.key,
    label: meta.label,
    onLabel: meta.onLabel || "",
    offLabel: meta.offLabel || "",
    unit: meta.unit,
    precision: meta.precision,
    warnMin: meta.warnMin,
    warnMax: meta.warnMax,
    alarmMin: meta.alarmMin,
    alarmMax: meta.alarmMax,
    isBool: meta.isBool,
    isButton: meta.isButton || false,
    isMomentaryButton: meta.isMomentaryButton || false,
    isStateButton: meta.isStateButton || false,
    isIntSlider: meta.isIntSlider || false,
    isFloatSlider: meta.isFloatSlider || false,
    isCheckbox: meta.isCheckbox || false,
    card: meta.card || null,
    min: meta.min,
    max: meta.max,
    init: meta.init,
    isIntInput: meta.isIntInput || false,
    isFloatInput: meta.isFloatInput || false,
    boolAlarmValue:
      typeof meta.boolAlarmValue === "boolean"
        ? !!meta.boolAlarmValue
        : undefined,
    isString: meta.isString || false,
    isDivider: meta.isDivider || false,
    staticValue: meta.staticValue || "",
    triggerOnPress: meta.triggerOnPress === true,
    order: meta.order !== undefined ? meta.order : 100,
    style: meta.style || null,
    styleRules: normalizeStyle(meta.style || null),
    group: layoutGroup,
    sourceGroup: sourceGroup,
    page: meta.page || null,
  };
}

function sortRuntimeFields(fields) {
  fields.sort((a, b) => {
    if (a.isDivider && b.isDivider) return a.order - b.order;
    if (a.order === b.order) return a.label.localeCompare(b.label);
    return a.order - b.order;
  });
}

function runtimeValue(field) {
  const groupName = fieldSourceGroup(field);
  if (!groupName || !runtime.value) return undefined;
  const groupData = runtime.value[groupName];
  if (!groupData || typeof groupData !== 'object') return undefined;
  return groupData[field.key];
}

function hasRuntimeValue(field) {
  const groupName = fieldSourceGroup(field);
  if (!groupName || !runtime.value) return false;
  const groupData = runtime.value[groupName];
  if (!groupData || typeof groupData !== 'object') return false;
  return Object.prototype.hasOwnProperty.call(groupData, field.key);
}

function cardHasRuntimeSource(card, sourceName) {
  if (!card || !sourceName) return false;
  const token = normalizeGroupToken(sourceName);
  const sources = Array.isArray(card.runtimeSources) ? card.runtimeSources : [];
  return sources.some((entry) => normalizeGroupToken(entry) === token);
}

function buildLiveLayoutPages(groups, layout) {
  if (!layout || !Array.isArray(layout.pages) || !layout.pages.length) {
    return [];
  }

  const fieldMap = new Map();
  const assigned = new Set();

  for (const group of groups) {
    if (!group || !Array.isArray(group.fields)) continue;
    if (!Array.isArray(group.runtimeSources)) {
      group.runtimeSources = [group.name];
    }
    for (const field of group.fields) {
      if (!field || typeof field.key !== 'string') continue;
      fieldMap.set(field.key, field);
    }
  }

  const sortByOrder = (list) => {
    const arr = Array.isArray(list) ? [...list] : [];
    arr.sort((a, b) => {
      const ao = typeof a?.order === 'number' ? a.order : 1000;
      const bo = typeof b?.order === 'number' ? b.order : 1000;
      if (ao === bo) {
        const aTitle = String(a?.title || a?.name || '');
        const bTitle = String(b?.title || b?.name || '');
        return aTitle.localeCompare(bTitle);
      }
      return ao - bo;
    });
    return arr;
  };

  const pages = [];
  const sortedPages = sortByOrder(layout.pages);

  sortedPages.forEach((page, pageIndex) => {
    const pageTitle = page?.title || page?.name || `Live ${pageIndex + 1}`;
    const pageKey = normalizeGroupToken(page?.key || page?.slug || page?.name) || `page_${pageIndex}`;
    const cards = sortByOrder(page?.cards || []);
    const cardsForPage = [];

    cards.forEach((card, cardIndex) => {
      const cardTitle = card?.title || card?.name || pageTitle;
      const cardKey = normalizeGroupToken(card?.key || card?.slug || card?.name) || `card_${pageIndex}_${cardIndex}`;
      const cardItems = [];
      const cardSources = new Set();

      const itemIds = Array.isArray(card?.items) ? card.items : [];
      itemIds.forEach((id) => {
        if (!fieldMap.has(id)) return;
        const field = fieldMap.get(id);
        cardItems.push(field);
        const source = fieldSourceGroup(field);
        if (source) cardSources.add(source);
        assigned.add(id);
      });

      const groupsForCard = [];
      const groupsList = sortByOrder(card?.groups || []);
      groupsList.forEach((groupEntry, groupIndex) => {
        const itemList = Array.isArray(groupEntry?.items) ? groupEntry.items : [];
        const collectedFields = [];
        const groupSources = new Set();
        itemList.forEach((id) => {
          if (!fieldMap.has(id)) return;
          const field = fieldMap.get(id);
          collectedFields.push(field);
          const source = fieldSourceGroup(field);
          if (source) {
            groupSources.add(source);
            cardSources.add(source);
          }
          assigned.add(id);
        });
        if (collectedFields.length) {
          const normalizedGroupName = normalizeGroupToken(groupEntry?.name || groupEntry?.title) || `group_${cardIndex}_${groupIndex}`;
          groupsForCard.push({
            key: normalizedGroupName,
            title: groupEntry?.title || groupEntry?.name || '',
            order: typeof groupEntry?.order === 'number' ? groupEntry.order : 1000 + groupIndex,
            fields: collectedFields,
            runtimeSources: Array.from(groupSources),
          });
        }
      });

      if (cardItems.length || groupsForCard.length) {
        cardsForPage.push({
          key: cardKey,
          title: cardTitle,
          order: typeof card?.order === 'number' ? card.order : 1000 + cardIndex,
          items: cardItems,
          groups: groupsForCard,
          runtimeSources: Array.from(cardSources),
        });
      }
    });

    if (cardsForPage.length) {
      pages.push({
        key: pageKey,
        title: pageTitle,
        order: typeof page?.order === 'number' ? page.order : 1000 + pageIndex,
        cards: cardsForPage,
      });
    }
  });

  if (fieldMap.size && assigned.size < fieldMap.size) {
    const fallbackCards = new Map();
    for (const [fieldKey, field] of fieldMap.entries()) {
      if (assigned.has(fieldKey)) continue;
      const bucketKey = fieldSourceGroup(field) || 'other';
      if (!fallbackCards.has(bucketKey)) {
        fallbackCards.set(bucketKey, {
          key: `fallback_${bucketKey}`,
          title: fallbackBucketTitle(bucketKey),
          order: 1000,
          items: [],
          groups: [],
          runtimeSources: [],
        });
      }
      const bucket = fallbackCards.get(bucketKey);
      bucket.items.push(field);
      const source = fieldSourceGroup(field);
      if (source && !bucket.runtimeSources.includes(source)) {
        bucket.runtimeSources.push(source);
      }
    }

    if (fallbackCards.size) {
      pages.push({
        key: '__unassigned__',
        title: 'Other',
        order: Number.MAX_SAFE_INTEGER,
        cards: Array.from(fallbackCards.values()),
      });
    }
  }

  pages.sort((a, b) => {
    const ao = typeof a.order === 'number' ? a.order : 1000;
    const bo = typeof b.order === 'number' ? b.order : 1000;
    if (ao === bo) {
      return a.title.localeCompare(b.title);
    }
    return ao - bo;
  });

  const groupOrderValue = (grp) => {
    const orders = (grp.fields || []).map((field) => (typeof field.order === 'number' ? field.order : 1000));
    return orders.length ? Math.min(...orders) : 1000;
  };

  pages.forEach((page) => {
    page.cards.sort((a, b) => {
      const ao = typeof a.order === 'number' ? a.order : 1000;
      const bo = typeof b.order === 'number' ? b.order : 1000;
      if (ao === bo) return a.title.localeCompare(b.title);
      return ao - bo;
    });
    page.cards.forEach((card) => {
      card.groups.sort((a, b) => {
        const ao = groupOrderValue(a);
        const bo = groupOrderValue(b);
        if (ao === bo) return a.title.localeCompare(b.title);
        return ao - bo;
      });
    });
  });

  return pages;
}

function buildCardsFromMeta(metaList) {
  if (!Array.isArray(metaList) || !metaList.length) {
    return [];
  }

  const cards = new Map();

  const ensureCard = (key, title) => {
    if (!cards.has(key)) {
      cards.set(key, {
        key,
        title,
        order: 1000,
        items: [],
        groups: [],
        runtimeSources: [],
        _groups: new Map(),
        _minOrder: Number.POSITIVE_INFINITY,
      });
    }
    return cards.get(key);
  };

  const ensureGroup = (card, key, title) => {
    if (!card._groups.has(key)) {
      card._groups.set(key, {
        key,
        title,
        order: 1000,
        fields: [],
        runtimeSources: [],
        _minOrder: Number.POSITIVE_INFINITY,
      });
    }
    return card._groups.get(key);
  };

  metaList.forEach((m, idx) => {
    if (m.group === "system" && builtinSystemHiddenFields.has(m.key)) {
      return;
    }
    const sourceGroup = typeof m.sourceGroup === 'string' && m.sourceGroup.length
      ? m.sourceGroup
      : m.group;
    let cardTitle = typeof m.card === 'string' && m.card.length ? m.card : '';
    let groupTitle = typeof m.group === 'string' && m.group.length ? m.group : '';
    if (!cardTitle && groupTitle) {
      cardTitle = groupTitle;
      groupTitle = '';
    }
    if (!cardTitle) {
      cardTitle = sourceGroup || 'default';
    }
    const cardKey = normalizeGroupToken(cardTitle) || `card_${idx}`;
    const card = ensureCard(cardKey, cardTitle);

    const field = createRuntimeField(m, groupTitle || cardTitle, sourceGroup);
    const orderValue = typeof field.order === 'number' ? field.order : 1000;
    card._minOrder = Math.min(card._minOrder, orderValue);

    if (groupTitle) {
      const groupKey = normalizeGroupToken(groupTitle) || `group_${card._groups.size}`;
      const group = ensureGroup(card, groupKey, groupTitle);
      group.fields.push(field);
      group._minOrder = Math.min(group._minOrder, orderValue);
      if (sourceGroup && !group.runtimeSources.includes(sourceGroup)) {
        group.runtimeSources.push(sourceGroup);
      }
    } else {
      card.items.push(field);
    }

    if (sourceGroup && !card.runtimeSources.includes(sourceGroup)) {
      card.runtimeSources.push(sourceGroup);
    }
  });

  try {
    const sys = runtime.value.system || {};
    if (sys && typeof sys === 'object') {
      for (const card of cards.values()) {
        const hasSystem = card.runtimeSources.some((entry) => normalizeGroupToken(entry) === 'system')
          || normalizeGroupToken(card.title) === 'system';
        if (!hasSystem) {
          continue;
        }
        const existingKeys = new Set();
        card.items.forEach((f) => existingKeys.add(f.key));
        card._groups.forEach((g) => g.fields.forEach((f) => existingKeys.add(f.key)));

        Object.keys(sys).forEach((k) => {
          if (builtinSystemHiddenFields.has(k)) return;
          if (existingKeys.has(k)) return;
          card.items.push({
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
            group: "system",
            sourceGroup: "system",
            page: null,
          });
          card._minOrder = Math.min(card._minOrder, 999);
          if (!card.runtimeSources.includes("system")) {
            card.runtimeSources.push("system");
          }
        });
      }
    }
  } catch (e) {
    /* ignore */
  }

  const result = [];
  for (const card of cards.values()) {
    sortRuntimeFields(card.items);

    const groupList = Array.from(card._groups.values());
    groupList.forEach((group) => {
      sortRuntimeFields(group.fields);
      if (!Array.isArray(group.runtimeSources) || !group.runtimeSources.length) {
        group.runtimeSources = [...card.runtimeSources];
      }
      group.order = Number.isFinite(group._minOrder) ? group._minOrder : 1000;
      delete group._minOrder;
    });
    groupList.sort((a, b) => {
      const ao = typeof a.order === 'number' ? a.order : 1000;
      const bo = typeof b.order === 'number' ? b.order : 1000;
      if (ao === bo) return a.title.localeCompare(b.title);
      return ao - bo;
    });

    const cardOrder = Number.isFinite(card._minOrder) ? card._minOrder : 1000;
    result.push({
      key: card.key,
      title: card.title,
      order: cardOrder,
      items: card.items,
      groups: groupList,
      runtimeSources: card.runtimeSources,
    });
  }

  result.sort((a, b) => {
    const ao = typeof a.order === 'number' ? a.order : 1000;
    const bo = typeof b.order === 'number' ? b.order : 1000;
    if (ao === bo) return a.title.localeCompare(b.title);
    return ao - bo;
  });

  return result;
}


let pollTimer = null;
let ws = null;
let wsRetry = 0;
let wsConnecting = false;

let checkboxDebounceTimer = null;

const rURIComp = encodeURIComponent;

function appendLogEntry(entry) {
  if (!entry) return;
  const ts =
    entry.ts === null
      ? null
      : typeof entry.ts === "number"
        ? entry.ts
        : Date.now();
  const level = entry.level || "INFO";
  const msg = entry.msg || "";
  const tag = entry.tag || "";
  const dt = entry.dt || "";
  logEntries.value.push({
    id: `${ts}_${logEntries.value.length}`,
    ts,
    dt,
    level,
    msg,
    tag,
  });
  logEnabled.value = true;
  if (logEntries.value.length > LOG_MAX_ENTRIES) {
    logEntries.value.splice(0, logEntries.value.length - LOG_MAX_ENTRIES);
  }
}

function clearLogs() {
  logEntries.value = [];
}

const sortedRuntimeGroups = computed(() => {
  const groups = Array.isArray(runtimeGroups.value) ? runtimeGroups.value : [];
  const visible = groups.filter((group) => groupHasVisibleContent(group));

  const runtimeOrder = (() => {
    const r = runtime.value && typeof runtime.value === 'object' ? runtime.value : {};
    const keys = Object.keys(r)
      .filter((k) => k !== 'uptime')
      .filter((k) => r && r[k] && typeof r[k] === 'object' && !Array.isArray(r[k]));
    const map = new Map();
    let idx = 0;
    for (const k of keys) {
      map.set(normalizeGroupToken(k), idx++);
    }
    return map;
  })();

  const ordered = [...visible];
  ordered.sort((a, b) => {
    const aFields = Array.isArray(a?.fields) ? a.fields : [];
    const bFields = Array.isArray(b?.fields) ? b.fields : [];

    const aToken = normalizeGroupToken(a?.name || a?.title);
    const bToken = normalizeGroupToken(b?.name || b?.title);

    const hasOverride = (token, fields) => {
      if (token !== 'system') return false;
      for (const f of fields) {
        if (!f || typeof f.order !== 'number') continue;
        if (f.order < 0) return true;
      }
      return false;
    };

    const aSystemOverride = hasOverride(aToken, aFields);
    const bSystemOverride = hasOverride(bToken, bFields);

    if (!aSystemOverride && aToken === 'system' && bToken !== 'system') return 1;
    if (!bSystemOverride && bToken === 'system' && aToken !== 'system') return -1;

    const aRuntimePos = runtimeOrder.has(aToken) ? runtimeOrder.get(aToken) : null;
    const bRuntimePos = runtimeOrder.has(bToken) ? runtimeOrder.get(bToken) : null;
    if (aRuntimePos !== null && bRuntimePos !== null && aRuntimePos !== bRuntimePos) {
      return aRuntimePos - bRuntimePos;
    }
    if (aRuntimePos !== null && bRuntimePos === null) return -1;
    if (aRuntimePos === null && bRuntimePos !== null) return 1;

    const computeGroupOrder = (groupToken, fields) => {
      let minOrder = 1000;
      let hasAnyOrderNumber = false;
      let hasNonZeroOrder = false;
      for (const f of fields) {
        if (!f || typeof f.order !== 'number') continue;
        hasAnyOrderNumber = true;
        minOrder = Math.min(minOrder, f.order);
        if (f.order !== 0) hasNonZeroOrder = true;
      }
      if (groupToken === 'system' && !hasNonZeroOrder) {
        return { hasOrder: false, order: 1000 };
      }
      return { hasOrder: hasAnyOrderNumber && minOrder < 1000, order: minOrder };
    };

    const aMeta = computeGroupOrder(aToken, aFields);
    const bMeta = computeGroupOrder(bToken, bFields);
    if (aMeta.hasOrder && bMeta.hasOrder && aMeta.order !== bMeta.order) return aMeta.order - bMeta.order;
    if (aMeta.hasOrder !== bMeta.hasOrder) return aMeta.hasOrder ? -1 : 1;

    const priorityOrder = {
      alerts: 0,
      sensors: 1,
      control: 2,
      controls: 2,
      controll: 2,
    };
    const aPrio = Object.prototype.hasOwnProperty.call(priorityOrder, aToken)
      ? priorityOrder[aToken]
      : Number.POSITIVE_INFINITY;
    const bPrio = Object.prototype.hasOwnProperty.call(priorityOrder, bToken)
      ? priorityOrder[bToken]
      : Number.POSITIVE_INFINITY;
    if (aPrio !== bPrio) return aPrio - bPrio;

    const aTitle = String(a?.title || a?.name || '');
    const bTitle = String(b?.title || b?.name || '');
    const titleCmp = aTitle.localeCompare(bTitle);
    if (titleCmp !== 0) return titleCmp;
    return String(a?.name || '').localeCompare(String(b?.name || ''));
  });
  return ordered;
});

const layoutPages = computed(() => buildLiveLayoutPages(sortedRuntimeGroups.value, liveLayout.value));
const layoutTabs = computed(() => layoutPages.value.map((page) => ({ key: page.key, title: page.title })));

const displayRuntimeCards = computed(() => {
  if (!layoutPages.value.length) {
    const cardsFromMeta = buildCardsFromMeta(runtimeMeta.value);
    if (cardsFromMeta.length) {
      return cardsFromMeta;
    }
    return sortedRuntimeGroups.value.map((group) => ({
      key: group.name,
      title: group.title,
      order: typeof group.order === 'number' ? group.order : 1000,
      items: Array.isArray(group.fields) ? group.fields : [],
      groups: [],
      runtimeSources: Array.isArray(group.runtimeSources) ? group.runtimeSources : [],
    }));
  }
  const fallback = layoutPages.value[0];
  const currentKey = activeLivePage.value || (fallback ? fallback.key : '');
  const currentPage = layoutPages.value.find((page) => page.key === currentKey) || fallback;
  return currentPage ? currentPage.cards : [];
});

watch(layoutPages, (pages) => {
  if (!pages.length) {
    activeLivePage.value = "";
    return;
  }
  if (!pages.some((page) => page.key === activeLivePage.value)) {
    activeLivePage.value = pages[0].key;
  }
}, { immediate: true });

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

// Replace previous canFlash with strict gating:
// - Disable when probe says disabled (403/404)
// - Enable when probe says enabled (200) OR runtime.system.otaActive === true
// - Ignore config/meta for enabling to avoid stale states; only use config to pre-disable when explicitly false
const canFlash = computed(() => {
  //console.log('[canFlash] Computing... probe:', otaEndpointAvailable.value, 'flashing:', flashing.value);
  
  // Probe is authoritative when negative
  if (otaEndpointAvailable.value === false) {
    //console.log('[canFlash] Disabled by probe');
    return false;
  }

  // If runtime system indicates OTA active, trust it
  try {
    if (
      runtime.value?.system &&
      Object.prototype.hasOwnProperty.call(runtime.value.system, "otaActive")
    ) {
      const result = !!runtime.value.system.otaActive && !flashing.value;
      //console.log('[canFlash] Runtime otaActive:', runtime.value.system.otaActive, 'Result:', result);
      return result;
    }
  } catch (e) {}

  // If endpoint probe succeeded, enable
  if (otaEndpointAvailable.value === true) {
    //console.log('[canFlash] Enabled by probe success');
    return !flashing.value;
  }

  // If config explicitly says disabled, pre-disable
  const systemConfig = props.config?.System;
  if (
    systemConfig &&
    Object.prototype.hasOwnProperty.call(systemConfig, "OTAEn") &&
    systemConfig.OTAEn &&
    typeof systemConfig.OTAEn.value !== "undefined" &&
    !systemConfig.OTAEn.value
  ) {
    //console.log('[canFlash] Disabled by config');
    return false;
  }

  // Default: disabled until we know
  //console.log('[canFlash] Disabled by default (waiting for probe)');
  return false;
});

const otaEnabled = computed(() => {
  // Same source of truth as canFlash, but without the flashing guard
  if (otaEndpointAvailable.value === false) return false;
  try {
    if (
      runtime.value?.system &&
      Object.prototype.hasOwnProperty.call(runtime.value.system, "otaActive")
    ) {
      return !!runtime.value.system.otaActive;
    }
  } catch (e) {}
  if (otaEndpointAvailable.value === true) return true;
  const systemConfig = props.config?.System;
  if (
    systemConfig &&
    Object.prototype.hasOwnProperty.call(systemConfig, "OTAEn") &&
    systemConfig.OTAEn &&
    typeof systemConfig.OTAEn.value !== "undefined"
  ) {
    return !!systemConfig.OTAEn.value;
  }
  return false;
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
    const { visible, className, classes, ...rest } = ruleValue;
    const cssProps = { ...rest };
    const entry = {
      css: cssProps,
      visible: Object.prototype.hasOwnProperty.call(ruleValue, "visible")
        ? !!visible
        : undefined,
    };
    if (typeof className === "string") {
      entry.className = className;
    }
    if (typeof classes !== "undefined") {
      entry.classes = classes;
    }
    normalized[ruleKey] = entry;
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
  
  // Start WebSocket attempt
  startWebSocket(url);
  
  // Also start a backup polling mechanism with a delay to ensure we have data flow
  setTimeout(() => {
    if (!wsConnected.value && !pollTimer) {
      //console.log("WebSocket not connected, starting backup polling");
      fallbackPolling();
    }
  }, 3000); // Give WebSocket 3 seconds to connect
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
    if (wsConnecting) return; // avoid racing connections
    wsConnecting = true;
    ws = new WebSocket(url);
    
    // Set a timeout to quickly detect if WebSocket is not available
    const connectionTimeout = setTimeout(() => {
      if (ws && ws.readyState === WebSocket.CONNECTING) {
        try {
          ws.close();
        } catch (e) {
          /* ignore */
        }
        wsConnected.value = false;
        wsConnecting = false;
        // Don't retry if WebSocket is not available (likely disabled on server)
        if (wsRetry === 0) {
          //console.log("WebSocket not available, using polling mode");
          if (!pollTimer) {
            fallbackPolling();
          }
          return;
        }
        scheduleWsReconnect(url);
      }
    }, 2000); // 2 second timeout for initial connection
    
    ws.onopen = () => {
      clearTimeout(connectionTimeout);
      wsConnected.value = true;
      wsRetry = 0;
      wsConnecting = false;
      // Stop polling when WebSocket is connected
      if (pollTimer) {
        clearInterval(pollTimer);
        pollTimer = null;
      }
      setTimeout(() => {
        if (!runtime.value.uptime) fetchRuntime();
      }, 300);
    };
    ws.onclose = () => {
      clearTimeout(connectionTimeout);
      wsConnected.value = false;
      wsConnecting = false;
      scheduleWsReconnect(url);
    };
    ws.onerror = () => {
      clearTimeout(connectionTimeout);
      wsConnected.value = false;
      wsConnecting = false;
      scheduleWsReconnect(url);
    };
    ws.onmessage = (ev) => {
      // Manual heartbeat: respond to server "__ping" with "__pong"
      if (typeof ev.data === "string" && ev.data === "__ping") {
        try { ws.send("__pong"); } catch (e) {}
        return;
      }
      try {
        const parsed = JSON.parse(ev.data);
        if (!parsed || typeof parsed !== "object") {
          if (!pollTimer) {
            fallbackPolling();
          }
          return;
        }
        if (parsed.type === "log") {
          appendLogEntry(parsed);
          return;
        }
        if (parsed.type === "logReady") {
          logEnabled.value = true;
          return;
        }
        if (typeof parsed.type === "string") {
          return; // ignore GUI messages or other typed frames
        }
        runtime.value = parsed;
        buildRuntimeGroups();
      } catch (e) {
        // If we receive non-JSON frames repeatedly, WebSocket mode can look "frozen".
        // Start polling as a resilient fallback.
        if (!pollTimer) {
          fallbackPolling();
        }
      }
    };
  } catch (e) {
    scheduleWsReconnect(url);
  }
}

function scheduleWsReconnect(url) {
  // If we've retried many times without success, WebSocket is likely disabled
  if (wsRetry >= 5) {
    //console.log("WebSocket appears to be disabled after multiple failures, using polling mode only");
    if (!pollTimer) {
      fallbackPolling();
    }
    return;
  }
  
  // Start with a gentler backoff to avoid thrashing on flaky links
  const delay = Math.min(8000, 1000 + wsRetry * wsRetry * 600);
  wsRetry++;
  setTimeout(() => {
    startWebSocket(url);
  }, delay);
  if (!pollTimer) {
    fallbackPolling();
  }
}

function fetchWithTimeout(resource, options = {}, timeoutMs = 4000) {
  // Firefox-compatible timeout implementation
  if (typeof AbortController !== 'undefined') {
    // Modern browsers with AbortController support
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), timeoutMs);
    const opts = { ...options, signal: controller.signal };
    return fetch(resource, opts)
      .finally(() => clearTimeout(id));
  } else {
    // Fallback for older browsers
    return Promise.race([
      fetch(resource, options),
      new Promise((_, reject) => 
        setTimeout(() => reject(new Error('Request timeout')), timeoutMs)
      )
    ]);
  }
}

async function fetchRuntime() {
  try {
    const r = await fetchWithTimeout("/runtime.json?ts=" + Date.now(), {}, 4000);
    if (!r.ok) return;
    runtime.value = await r.json();
    buildRuntimeGroups();
  } catch (e) {
    // ignore network/abort; polling will retry
  }
}

async function fetchRuntimeMeta() {
  try {
    const r = await fetchWithTimeout("/runtime_meta.json?ts=" + Date.now(), {}, 5000);
    if (!r.ok) return;
    runtimeMeta.value = await r.json();
    buildRuntimeGroups();
  } catch (e) {
    // ignore network/abort; polling will retry
  }
}

async function fetchLiveLayout() {
  try {
    const r = await fetchWithTimeout("/live_layout.json?ts=" + Date.now(), {}, 5000);
    if (!r.ok) {
      liveLayout.value = null;
      return;
    }
    liveLayout.value = await r.json();
  } catch (e) {
    liveLayout.value = null;
  }
}

function buildRuntimeGroups() {
  if (runtimeMeta.value.length) {
    const grouped = {};
    for (const m of runtimeMeta.value) {
      if (m.group === "system" && builtinSystemHiddenFields.has(m.key)) {
        continue;
      }
      const sourceGroup = typeof m.sourceGroup === 'string' && m.sourceGroup.length
        ? m.sourceGroup
        : m.group;
      const layoutGroup = typeof m.group === 'string' && m.group.length
        ? m.group
        : (m.card || sourceGroup || 'default');
      const groupKey = layoutGroup || (m.card || sourceGroup || 'default');
      if (!grouped[groupKey]) {
        grouped[groupKey] = {
          name: groupKey,
          title: capitalize(groupKey),
          fields: [],
          runtimeSources: [],
        };
      }
      grouped[groupKey].fields.push(createRuntimeField(m, layoutGroup, sourceGroup));
      if (sourceGroup && !grouped[groupKey].runtimeSources.includes(sourceGroup)) {
        grouped[groupKey].runtimeSources.push(sourceGroup);
      }
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
              group: "system",
              sourceGroup: "system",
              page: null,
            });
          }
        });
      }
    } catch (e) {
      /* ignore */
    }

    runtimeGroups.value = Object.values(grouped).map((g) => {
      sortRuntimeFields(g.fields);
      if (!Array.isArray(g.runtimeSources) || !g.runtimeSources.length) {
        g.runtimeSources = [g.name];
      }
      return g;
    });
    return;
  }

  const fallback = [];

  if (runtime.value.sensors) {
    fallback.push({
      name: "sensors",
      title: "Sensors",
      runtimeSources: ["sensors"],
      fields: Object.keys(runtime.value.sensors).map((k) => ({
        key: k,
        label: capitalize(k),
        unit: "",
        precision:
          k.toLowerCase().includes("temp") || k.toLowerCase().includes("dew")
            ? 1
            : 0,
        group: "sensors",
        sourceGroup: "sensors",
      })),
    });
  }
  if (runtime.value.system) {
    fallback.push({
      name: "system",
      title: "System",
      runtimeSources: ["system"],
      fields: Object.keys(runtime.value.system)
        .filter((k) => !builtinSystemHiddenFields.has(k))
        .map((k) => ({
          key: k,
          label: capitalize(k),
          unit: "",
          precision: 0,
          group: "system",
          sourceGroup: "system",
        })),
    });
  }
  runtimeGroups.value = fallback;
}

function groupHasVisibleContent(group) {
  if (!group || !Array.isArray(group.fields) || group.fields.length === 0) {
    return false;
  }
  return group.fields.some((field) => fieldHasVisibleContent(field));
}

function fieldHasVisibleContent(field) {
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
    return runtimeHasValue(fieldSourceGroup(field), field.key);
  }
  return runtimeHasValue(fieldSourceGroup(field), field.key);
}

function isInteractiveField(field) {
  return !!(
    field.isButton ||
    field.isMomentaryButton ||
    field.isStateButton ||
    field.isIntSlider ||
    field.isFloatSlider ||
    field.isCheckbox ||
    field.isIntInput ||
    field.isFloatInput
  );
}

function runtimeHasValue(groupName, key) {
  if (!groupName) return false;
  const groupData = runtime.value[groupName];
  if (
    groupData &&
    Object.prototype.hasOwnProperty.call(groupData, key) &&
    groupData[key] !== null &&
    groupData[key] !== undefined
  ) {
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

function handleMomentarySet({ group, field, value }) {
  onStateButton(group, field, value);
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

async function sendIntInput(group, f, val) {
  try {
    const r = await fetch(
      `/runtime_action/int_input?group=${rURIComp(group)}&key=${rURIComp(
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
    // Ensure value uses dot decimal separator for HTTP request
    const normalizedVal = String(val).replace(',', '.');
    
    const r = await fetch(
      `/runtime_action/float_slider?group=${rURIComp(group)}&key=${rURIComp(
        f.key
      )}&value=${normalizedVal}`,
      {
        method: "POST",
      }
    );
    if (!r.ok) {
      notifySafe(`Set failed: ${f.key}`, "error");
    } else {
      if (!runtime.value[group]) runtime.value[group] = {};
      runtime.value[group][f.key] = parseFloat(normalizedVal);
      notifySafe(`${f.key}=${normalizedVal}`, "success", 1200);
    }
  } catch (e) {
    notifySafe(`Set error ${f.key}: ${e.message}`, "error");
  }
}

async function sendFloatInput(group, f, val) {
  try {
    const normalizedVal = String(val).replace(',', '.');

    const r = await fetch(
      `/runtime_action/float_input?group=${rURIComp(group)}&key=${rURIComp(
        f.key
      )}&value=${normalizedVal}`,
      {
        method: "POST",
      }
    );
    if (!r.ok) {
      notifySafe(`Set failed: ${f.key}`, "error");
    } else {
      if (!runtime.value[group]) runtime.value[group] = {};
      runtime.value[group][f.key] = parseFloat(normalizedVal);
      notifySafe(`${f.key}=${normalizedVal}`, "success", 1200);
    }
  } catch (e) {
    notifySafe(`Set error ${f.key}: ${e.message}`, "error");
  }
}

async function handleInputCommit({ group, field, value }) {
  if (field.isFloatInput) {
    await sendFloatInput(group, field, value);
  } else {
    await sendIntInput(group, field, value);
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

function fallbackBucketTitle(bucketKey) {
  const normalized = normalizeGroupToken(bucketKey);
  if (normalized === "sensors") {
    return "Sensors";
  }
  if (!bucketKey) {
    return "Other";
  }
  return capitalize(bucketKey);
}

function formatValue(val, meta) {
  if (val == null) return "";
  if (typeof val === "number") {
    if (meta && typeof meta.precision === "number" && Number.isFinite(val)) {
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

function valueClasses(val, meta, groupName) {
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

  // For numeric fields: if the backend provides explicit alarm flags (dynamic settings-driven),
  // prefer those over static alarmMin/alarmMax from runtime_meta.
  try {
    if (groupName && meta && typeof meta.key === "string") {
      const groupData = runtime.value && runtime.value[groupName];
      if (groupData && typeof groupData === "object") {
        const minKey = meta.key + "_alarm_min";
        const maxKey = meta.key + "_alarm_max";
        const hasMinFlag = groupData[minKey] !== undefined;
        const hasMaxFlag = groupData[maxKey] !== undefined;
        if (hasMinFlag || hasMaxFlag) {
          if (!!groupData[minKey] || !!groupData[maxKey]) return "sev-alarm";
          // No alarm active per flags -> still allow warning styling.
          if (typeof val === "number" && meta) {
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
          }
          return "";
        }
      }
    }
  } catch (e) {
    /* ignore */
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

function normalizeClassTokens(value) {
  if (!value) return [];
  if (Array.isArray(value)) {
    return value
      .flatMap((entry) => normalizeClassTokens(entry))
      .filter(Boolean);
  }
  if (typeof value === "string") {
    return value
      .split(/\s+/)
      .map((token) => token.trim())
      .filter(Boolean);
  }
  return [];
}

function fieldClasses(meta, key) {
  const rule = fieldRule(meta, key);
  if (!rule) return [];
  const classes = [];
  if (typeof rule.className === "string") {
    classes.push(...normalizeClassTokens(rule.className));
  }
  if (typeof rule.classes !== "undefined") {
    classes.push(...normalizeClassTokens(rule.classes));
  }
  return classes;
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
  // Ask for OTA password first (single prompt at button press)
  savedOtaPassword.value = '';
  otaPassword.value = '';
  showPasswordModal.value = true;
  nextTick(() => {
    if (passwordInput.value) passwordInput.value.focus();
  });
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

  // We already asked for password at button press; use saved one
  const pwd = savedOtaPassword.value || '';
  pendingOtaFile.value = null; // not needed anymore
  await performOtaUpdate(file, pwd);
  if (otaFileInput.value) otaFileInput.value.value = "";
}

// Handle password modal confirmation
function confirmPasswordInput() {
  const password = otaPassword.value.trim();
  showPasswordModal.value = false;

  // If a file was already selected (unlikely in new flow), upload now; otherwise open picker
  if (pendingOtaFile.value) {
    performOtaUpdate(pendingOtaFile.value, password);
    pendingOtaFile.value = null;
    otaPassword.value = '';
    return;
  }

  // Store password and then prompt for file selection
  savedOtaPassword.value = password;
  if (!otaFileInput.value) {
    notifySafe("Browser file input not ready.", "error");
    return;
  }
  otaFileInput.value.value = "";
  otaFileInput.value.click();
  otaPassword.value = '';
}

// Handle password modal cancellation
function cancelPasswordInput() {
  showPasswordModal.value = false;
  otaPassword.value = '';
  savedOtaPassword.value = '';
  pendingOtaFile.value = null;
  if (otaFileInput.value) {
    otaFileInput.value.value = "";
  }
}

// Perform the actual OTA update
async function performOtaUpdate(file, password) {
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
    const r = await fetch("/ota_update", {
      method: "GET",
      headers: {
        "X-OTA-PROBE": "1",
      },
    });
    //console.log('[OTA Probe] Status:', r.status, 'Current state:', otaEndpointAvailable.value);
    // Only enable if we get a successful response (200-299)
    if (r.ok && r.status >= 200 && r.status < 300) {
      //console.log('[OTA Probe] Setting to true (200 OK)');
      otaEndpointAvailable.value = true;
    } else {
      //console.log('[OTA Probe] Setting to false (error status:', r.status + ')');
      otaEndpointAvailable.value = false;
    }
    //console.log('[OTA Probe] New state:', otaEndpointAvailable.value, 'canFlash:', canFlash.value);
  } catch (e) {
    //console.log('[OTA Probe] Network error:', e.message);
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
  fetchLiveLayout();
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
.runtime-tabs {
  display: flex;
  gap: 10px;
  margin: 10px 0 14px;
  flex-wrap: wrap;
}

.rt-tab {
  padding: 8px 16px;
  border: 1px solid var(--cm-card-border);
  background: var(--cm-bg);
  color: var(--cm-fg);
  border-radius: 6px;
  cursor: pointer;
  font-weight: 500;
  transition: background-color 0.2s ease, color 0.2s ease, border-color 0.2s ease,
    box-shadow 0.2s ease;
}

.rt-tab:hover {
  border-color: var(--cm-tab-active-bg);
  color: var(--cm-tab-active-bg);
}

.rt-tab:focus-visible {
  outline: 2px solid var(--cm-tab-active-bg);
  outline-offset: 2px;
}

.rt-tab.active {
  background: var(--cm-tab-active-bg);
  color: var(--cm-tab-active-fg, #0d1117);
  border-color: var(--cm-tab-active-bg);
  font-weight: 600;
  box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.08), 0 6px 16px rgba(217, 119, 6, 0.35);
}

.live-cards :deep(.rw.act .btn),
.live-cards :deep(.rw.sl-actions .btn) {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.35rem 0.9rem;
  border: 1px solid var(--cm-tab-active-bg);
  border-radius: 999px;
  background: rgba(217, 119, 6, 0.12);
  color: var(--cm-tab-active-bg);
  font-weight: 600;
  letter-spacing: 0.4px;
  text-transform: uppercase;
  min-width: 4.5rem;
  cursor: pointer;
  transition: background-color 0.2s ease, color 0.2s ease, border-color 0.2s ease,
    box-shadow 0.2s ease, transform 0.15s ease;
}

.live-cards :deep(.rw.act .btn:hover),
.live-cards :deep(.rw.act .btn:focus-visible),
.live-cards :deep(.rw.sl-actions .btn:hover),
.live-cards :deep(.rw.sl-actions .btn:focus-visible) {
  background: var(--cm-tab-active-bg);
  color: var(--cm-tab-active-fg, #0d1117);
  border-color: var(--cm-tab-active-bg);
  box-shadow: 0 6px 18px rgba(217, 119, 6, 0.35);
  transform: translateY(-1px);
}

.live-cards :deep(.rw.sl-actions .btn:disabled) {
  opacity: 0.55;
  cursor: not-allowed;
  box-shadow: none;
  transform: none;
}

.live-cards :deep(.rw.sl-actions .btn.reset) {
  background: rgba(255, 255, 255, 0.08);
  color: var(--cm-fg);
  border-color: rgba(255, 255, 255, 0.18);
}

.live-cards :deep(.rw.sl-actions .btn.reset:hover),
.live-cards :deep(.rw.sl-actions .btn.reset:focus-visible) {
  background: rgba(255, 255, 255, 0.15);
  color: var(--cm-fg);
}

.live-cards :deep(.sl-ctrl input[type="range"]) {
  width: 100%;
  -webkit-appearance: none;
  appearance: none;
  height: 6px;
  border-radius: 999px;
  background: linear-gradient(
    90deg,
    rgba(217, 119, 6, 0.7),
    rgba(217, 119, 6, 0.25)
  );
  outline: none;
}

.live-cards :deep(.sl-ctrl input[type="range"]::-webkit-slider-thumb) {
  -webkit-appearance: none;
  appearance: none;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: var(--cm-tab-active-bg);
  border: 2px solid var(--cm-card-bg);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
  cursor: grab;
  transition: transform 0.15s ease, box-shadow 0.15s ease;
}

.live-cards :deep(.sl-ctrl input[type="range"]::-webkit-slider-thumb:active) {
  cursor: grabbing;
  transform: scale(1.05);
  box-shadow: 0 6px 16px rgba(0, 0, 0, 0.45);
}

.live-cards :deep(.sl-ctrl input[type="range"]::-moz-range-track) {
  height: 6px;
  border-radius: 999px;
  background: linear-gradient(
    90deg,
    rgba(217, 119, 6, 0.7),
    rgba(217, 119, 6, 0.25)
  );
}

.live-cards :deep(.sl-ctrl input[type="range"]::-moz-range-thumb) {
  width: 18px;
  height: 18px;
  border-radius: 50%;
  border: 2px solid var(--cm-card-bg);
  background: var(--cm-tab-active-bg);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
  cursor: grab;
}

.log-view {
  border: 1px solid var(--cm-card-border);
  background: var(--cm-card-bg);
  border-radius: 8px;
  padding: 12px;
  display: flex;
  flex-direction: column;
  gap: 10px;
  min-height: 0;
  max-height: calc(100vh - 220px);
}

.log-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-weight: 600;
}

.clear-btn {
  border: 1px solid var(--cm-card-border);
  background: var(--cm-bg);
  color: var(--cm-fg);
  border-radius: 6px;
  padding: 6px 10px;
  cursor: pointer;
}

.log-list {
  flex: 1;
  min-height: 0;
  overflow: auto;
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono",
    monospace;
  font-size: 12px;
}

.log-table {
  width: 100%;
  border-collapse: collapse;
}

.log-table thead th {
  position: sticky;
  top: 0;
  z-index: 2;
  background: var(--cm-card-bg);
  text-align: left;
  padding: 6px 8px;
  border-bottom: 1px solid var(--cm-card-border);
  font-weight: 600;
}

.log-table tbody td {
  padding: 6px 8px;
  border-bottom: 1px solid var(--cm-card-border);
  vertical-align: top;
}

.log-table tbody tr:hover {
  background: rgba(255, 255, 255, 0.03);
}

.col-ts {
  width: 160px;
}

.col-level {
  width: 70px;
}

.col-tag {
  width: 180px;
}

.log-ts {
  opacity: 0.7;
}

.log-level {
  font-weight: 700;
}

.log-level.lvl-warn {
  background: #fff3cd;
  color: #8a6d00;
  padding: 2px 6px;
  border-radius: 4px;
}

.log-level.lvl-error {
  background: #f8d7da;
  color: #7a1f1f;
  padding: 2px 6px;
  border-radius: 4px;
}

.log-level[data-level="DEBUG"],
.log-level[data-level="TRACE"] {
  color: #6c8af0;
}

.log-tag {
  opacity: 0.85;
}

.log-msg {
  word-break: break-word;
}

.log-row {
  display: flex;
  gap: 8px;
  align-items: flex-start;
  flex-wrap: wrap;
}

.log-list-compact .log-row {
  padding: 6px 4px;
  border-bottom: 1px solid var(--cm-card-border);
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
  background: var(--cm-card-bg);
  border: 1px solid var(--cm-card-border);
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
  text-decoration: underline;
  color: var(--cm-tab-active-bg);
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
.bd--fallback.bd--safe {
  background: #2ecc71;
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
.grp {
  border: 1px solid var(--cm-group-border, var(--cm-card-border));
  border-radius: 8px;
  padding: 0.45rem 0.6rem 0.5rem;
  margin-top: 0.6rem;
  background: var(--cm-group-bg, transparent);
}
.grp-title {
  font-size: 0.68rem;
  letter-spacing: 0.5px;
  text-transform: uppercase;
  color: var(--cm-group-title, var(--cm-fg));
  margin-bottom: 0.35rem;
}
.grp .tbl {
  margin-top: 0;
}

/* Boolean dot alarm styling - fallback when style rules are disabled */
.bd.bd--alarm {
  background-color: #e74c3c !important;
  animation: alarm-pulse 1.5s ease-in-out infinite;
}

.bd.bd--safe {
  background-color: #2ecc71 !important;
}

.bd.bd--on {
  background-color: #1f6feb !important;
}

.bd.bd--off {
  background-color: #6e7681 !important;
}

@keyframes alarm-pulse {
  0%, 100% {
    opacity: 1;
    transform: scale(1);
  }
  50% {
    opacity: 0.6;
    transform: scale(1.1);
  }
}

/* OTA Password Modal */
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
  z-index: 1000;
}

.modal-content {
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 8px;
  padding: 24px;
  max-width: 400px;
  width: 90%;
  color: #f0f6fc;
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

.modal-actions {
  display: flex;
  gap: 8px;
  justify-content: flex-end;
}

.modal-actions button {
  padding: 8px 16px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 14px;
  transition: all 0.2s;
}

.cancel-btn {
  background: #6e7681;
  color: #f0f6fc;
}

.cancel-btn:hover {
  background: #8b949e;
}

.confirm-btn {
  background: #238636;
  color: #f0f6fc;
}

.confirm-btn:hover {
  background: #2ea043;
}
</style>
