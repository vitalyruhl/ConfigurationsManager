<template>
  <div class="category">
    <h2>{{ prettyName }}</h2>
    <Setting
      v-for="(settingData, key) in settings"
      :key="key"
      v-if="key !== 'categoryPretty'"
      :category="category"
      :keyName="key"
      :settingData="settingData"
      @apply="onApply"
      @save="onSave"
    />
  </div>
</template>

<script setup>
import { computed } from 'vue';
import Setting from './Setting.vue';
const props = defineProps({
  category: String,
  settings: Object
});
const emit = defineEmits(['apply-single', 'save-single']);
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
