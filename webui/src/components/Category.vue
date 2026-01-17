<template>
  <div class="category">
    <h2>{{ prettyName }}</h2>
    <template v-for="[key, settingData] in sortedSettings" :key="key">
      <Setting
        v-if="shouldShow(settingData)"
        :category="category"
        :keyName="key"
        :settingData="settingData"
        :busy="!!busyMap[category + '.' + key]"
        @apply="onApply"
        @save="onSave"
      />
    </template>
  </div>
</template>
<style scoped>
/* Intentionally minimized: styles provided globally or in parent. */
</style>
<script setup>
import { computed } from 'vue';
import Setting from './Setting.vue';
const props = defineProps({
  category: String,
  settings: Object,
  busyMap: { type: Object, default: () => ({}) }
});
const emit = defineEmits(['apply-single', 'save-single']);
const filteredSettings = computed(() => {
  // Filter out categoryPretty meta key
  return Object.fromEntries(
    Object.entries(props.settings).filter(([key, val]) => key !== 'categoryPretty')
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

