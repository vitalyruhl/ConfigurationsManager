<template>
  <div class="category">
    <h2>{{ prettyName }}</h2>
    <Setting
      v-for="(settingData, key) in filteredSettings"
      :key="key"
      :category="category"
      :keyName="key"
      :settingData="settingData"
      @apply="onApply"
      @save="onSave"
    />
  </div>
</template>
<style scoped>
.category {
  margin: 10px 0;
  padding: 15px;
  background: white;
  border-radius: 10px;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
}
h2 {
  color: #3498db;
  margin-top: 0;
  border-bottom: 2px solid #3498db;
  padding-bottom: 10px;
  font-size: 1.2rem;
}
</style>
<script setup>
import { computed } from 'vue';
import Setting from './Setting.vue';
const props = defineProps({
  category: String,
  settings: Object
});
const emit = defineEmits(['apply-single', 'save-single']);
const filteredSettings = computed(() => {
  // Filtere categoryPretty raus
  return Object.fromEntries(
    Object.entries(props.settings).filter(([key, val]) => key !== 'categoryPretty')
  );
});
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

