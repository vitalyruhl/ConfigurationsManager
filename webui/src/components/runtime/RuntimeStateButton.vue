<template>
  <div class="rw act" :data-group="group" :data-key="field.key">
    <span class="lab">{{ field.label }}</span>
    <span class="val">
      <button
        class="btn"
        type="button"
        :class="{ on: isOn }"
        @click="emitToggle"
      >
        {{ field.label }}: {{ isOn ? 'ON' : 'OFF' }}
      </button>
    </span>
    <span class="un"></span>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  group: {
    type: String,
    required: true,
  },
  field: {
    type: Object,
    required: true,
  },
  value: {
    type: [Boolean, Number, String],
    default: false,
  },
});

const emit = defineEmits(['toggle']);

const isOn = computed(() => {
  if (typeof props.value === 'boolean') return props.value;
  if (props.value === 0 || props.value === 1) return !!props.value;
  if (typeof props.value === 'string') return props.value === '1' || props.value.toLowerCase() === 'true';
  return !!props.value;
});

function emitToggle() {
  emit('toggle', {
    group: props.group,
    field: props.field,
    nextValue: !isOn.value,
  });
}
</script>

<style scoped>
.btn {
  background: #1976d2;
  color: #fff;
  border: none;
  padding: 0.3rem 0.8rem;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.8rem;
  font-weight: 600;
  letter-spacing: 0.4px;
  transition: background 0.2s ease;
}

.btn:hover {
  background: #125a9f;
}

.btn.on {
  background: #2e7d32;
}

.btn.on:hover {
  background: #256628;
}

.btn:active {
  filter: brightness(0.9);
}
</style>
