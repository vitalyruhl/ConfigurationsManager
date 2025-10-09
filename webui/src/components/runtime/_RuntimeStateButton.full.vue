<template>
  <div class="rw sb">
    <span class="lab">{{ field.label }}</span>
    <button class="btn" :class="on ? 'on' : 'off'" @click="toggle">
      {{ on ? (field.onLabel || 'ON') : (field.offLabel || 'OFF') }}
    </button>
    <span class="un"></span>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  group: { type: String, required: true },
  field: { type: Object, required: true },
  value: { type: Boolean, default: false }
});

const emit = defineEmits(['toggle']);
const on = computed(() => !!props.value);

function toggle() {
  emit('toggle', { group: props.group, field: props.field, nextValue: !on.value });
}
</script>

<style scoped>
.rw { display: grid; grid-template-columns: 1fr auto auto; align-items: center; gap: .5rem; }
.lab { font-weight: 600; }
.btn { padding: .25rem .6rem; border-radius: .4rem; border: 1px solid #888; cursor: pointer; }
.btn.on { background: #2ecc71; color: #fff; border-color: #27ae60; }
.btn.off { background: #e74c3c; color: #fff; border-color: #c0392b; }
</style>
