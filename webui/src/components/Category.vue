<template>
  <div class="category">
    <h2>{{ prettyName }}</h2>
    <template v-for="item in groupedSettings" :key="item.key">
      <div v-if="item.type === 'pair'" class="setting-pair">
        <Setting
          v-if="shouldShow(item.left.data)"
          :category="category"
          :keyName="item.left.key"
          :settingData="item.left.data"
          :busy="!!busyMap[category + '.' + item.left.key]"
          @apply="onApply"
          @save="onSave"
        />
        <Setting
          v-if="shouldShow(item.right.data)"
          :category="category"
          :keyName="item.right.key"
          :settingData="item.right.data"
          :busy="!!busyMap[category + '.' + item.right.key]"
          @apply="onApply"
          @save="onSave"
        />
      </div>
      <Setting
        v-else
        v-if="shouldShow(item.data)"
        :category="category"
        :keyName="item.key"
        :settingData="item.data"
        :busy="!!busyMap[category + '.' + item.key]"
        @apply="onApply"
        @save="onSave"
      />
    </template>
  </div>
</template>
<style scoped>
/* Intentionally minimized: styles provided globally or in parent. */
:deep(.setting-pair){
  margin:15px 0;
  padding:15px;
  background:var(--cm-card-bg);
  border:1px solid var(--cm-card-border);
  border-radius:5px;
  display:grid;
  grid-template-columns:1fr;
  gap:12px;
  color:var(--cm-fg)
}
:deep(.setting-pair > div){width:100%}
:deep(.setting-pair .setting){
  margin:0;
  padding:0;
  background:transparent;
  border:none;
  width:100%;
  max-width:none;
  flex-direction:column;
  align-items:stretch
}
:deep(.setting-pair .input-area){flex:1}
:deep(.setting-pair .actions){width:auto}
@media (min-width:768px){
  :deep(.setting-pair){grid-template-columns:1fr 1fr;gap:16px}
}
:deep(.setting-pair .actions){width:100%}
</style>
<script setup>
import { computed } from 'vue';
import Setting from './Setting.vue';
const props = defineProps({
  category: String,
  settings: Object,
  title: { type: String, default: '' },
  busyMap: { type: Object, default: () => ({}) }
});
const emit = defineEmits(['apply-single', 'save-single']);
const filteredSettings = computed(() => {
  // Filter out meta keys (category/card containers)
  return Object.fromEntries(
    Object.entries(props.settings).filter(([key, val]) => key !== 'categoryPretty' && key !== 'categoryOrder' && key !== 'cards')
  );
});

const sortedSettings = computed(() => {
  const entries = Object.entries(filteredSettings.value || {});
  entries.sort((a, b) => {
    const aKey = a[0];
    const bKey = b[0];
    const aData = a[1] || {};
    const bData = b[1] || {};
    const aOrder = typeof aData.sortOrder === 'number' ? aData.sortOrder : 1000;
    const bOrder = typeof bData.sortOrder === 'number' ? bData.sortOrder : 1000;
    if (aOrder !== bOrder) return aOrder - bOrder;
    const aName = String(aData.displayName || aData.name || aKey);
    const bName = String(bData.displayName || bData.name || bKey);
    const nameCmp = aName.localeCompare(bName);
    if (nameCmp !== 0) return nameCmp;
    return aKey.localeCompare(bKey);
  });
  return entries;
});

function resolveSettingLabel(settingKey, settingData) {
  return String(settingData.displayName || settingData.name || settingKey);
}

const groupedSettings = computed(() => {
  const entries = sortedSettings.value || [];
  const labelToEntry = new Map();
  entries.forEach(([key, data]) => {
    labelToEntry.set(resolveSettingLabel(key, data), { key, data });
  });

  const used = new Set();
  const result = [];

  for (const [key, data] of entries) {
    if (used.has(key)) continue;
    const label = resolveSettingLabel(key, data);
    const isTopic = label.endsWith(' Topic');
    const isJsonKey = label.endsWith(' JSON Key');

    if (isTopic) {
      const base = label.slice(0, -' Topic'.length);
      const matchLabel = `${base} JSON Key`;
      const match = labelToEntry.get(matchLabel);
      if (match && !used.has(match.key) && shouldShow(data) && shouldShow(match.data)) {
        used.add(key);
        used.add(match.key);
        result.push({
          type: 'pair',
          key: `${key}__${match.key}`,
          left: { key, data },
          right: { key: match.key, data: match.data },
        });
        continue;
      }
    }

    if (isJsonKey) {
      const base = label.slice(0, -' JSON Key'.length);
      const matchLabel = `${base} Topic`;
      const match = labelToEntry.get(matchLabel);
      if (match && !used.has(match.key) && shouldShow(data) && shouldShow(match.data)) {
        // Let the Topic entry render the pair to keep ordering stable.
        continue;
      }
    }

    used.add(key);
    result.push({ type: 'single', key, data });
  }

  return result;
});
function shouldShow(settingData){
  // Firmware can provide dynamic visibility via showIfResolved or showIf; fallback to always true
  if (settingData === null || typeof settingData !== 'object') return true;
  if (Object.prototype.hasOwnProperty.call(settingData, 'showIf') && typeof settingData.showIf === 'boolean') {
    return settingData.showIf; // already resolved to boolean in JSON (preferred)
  }
  // If backend later sends e.g. showIfResolved
  if (Object.prototype.hasOwnProperty.call(settingData, 'showIfResolved') && typeof settingData.showIfResolved === 'boolean') {
    return settingData.showIfResolved;
  }
  return true;
}
const prettyName = computed(() => {
  if (typeof props.title === 'string' && props.title.trim().length) return props.title;
  // Prefer category-level pretty name if provided
  if (
    Object.prototype.hasOwnProperty.call(props.settings, 'categoryPretty') &&
    typeof props.settings.categoryPretty === 'string' &&
    props.settings.categoryPretty.trim().length
  ) {
    return props.settings.categoryPretty;
  }
  // Fallback: look for a per-setting provided pretty name
  for (const key in props.settings) {
    if (key === 'categoryPretty') continue;
    const sd = props.settings[key];
    if (sd && typeof sd === 'object' && sd.categoryPretty) return sd.categoryPretty;
  }
  if (props.category === 'IO') return 'I/O';
  return props.category;
});
function onApply(key, value) {
  emit('apply-single', props.category, key, value);
}
function onSave(key, value) {
  emit('save-single', props.category, key, value);
}
</script>

