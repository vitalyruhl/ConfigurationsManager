<template>
  <div class="category">
    <h2>{{ prettyName }}</h2>
    <Setting
      v-for="(settingData, key) in filteredSettings"
      :key="key"
      :category="category"
      :keyName="key"
      :settingData="settingData"
      :busy="!!busyMap[category + '.' + key]"
      v-if="shouldShow(settingData)"
      @apply="onApply"
      @save="onSave"
    />
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
  for (const key in props.settings) {
    if (key === 'categoryPretty') continue;
    if (props.settings[key].categoryPretty) return props.settings[key].categoryPretty;
  }
  return props.category;
});
function onApply(key, value) {
  emit('apply-single', props.category, key, value);
}
function onSave(key, value) {
  emit('save-single', props.category, key, value);
}
</script>

