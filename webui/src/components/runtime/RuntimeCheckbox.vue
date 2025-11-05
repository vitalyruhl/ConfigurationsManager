<template>
  <div class="rw tg" :data-group="group" :data-key="field.key">
    <span class="lab">{{ field.label }}</span>
    <span class="val">
      <label class="switch">
        <input type="checkbox" :checked="isChecked" @change="onChange" />
        <span class="slider round"></span>
      </label>
    </span>
    <span class="un"></span>
  </div>
</template>

<script setup>
import { computed } from 'vue';
const props = defineProps({
  group: { type: String, required: true },
  field: { type: Object, required: true },
  value: { type: [Boolean, Number, String], default: false },
});
const emit = defineEmits(['change']);
const isChecked = computed(() => {
  if (typeof props.value === 'boolean') return props.value;
  if (props.value === 0 || props.value === 1) return !!props.value;
  if (typeof props.value === 'string') return props.value === '1' || props.value.toLowerCase() === 'true';
  return !!props.value;
});
function onChange(event){ emit('change', { group: props.group, field: props.field, checked: event.target.checked }); }
</script>
