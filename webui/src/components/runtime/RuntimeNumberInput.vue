<template>
  <div class="rw sl" :data-group="group" :data-key="field.key">
    <span class="lab">{{ field.label }}</span>
    <span class="val sw">
      <input
        class="num-input"
        type="number"
        :min="min"
        :max="max"
        :step="step"
        :value="currentValue"
        @input="onInput"
      />
      <button class="sb" type="button" @click="commit">Set</button>
    </span>
    <span class="un"></span>
  </div>
</template>

<script setup>
import { computed, ref, watch } from 'vue';

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
    type: [Number, String, null],
    default: null,
  },
  mode: {
    type: String,
    default: 'int',
    validator: (v) => ['int', 'float'].includes(v),
  },
});

const emit = defineEmits(['commit']);

const currentValue = ref(resolveInitialValue());

watch(
  () => props.value,
  (val) => {
    const resolved = resolveIncomingValue(val);
    if (resolved !== null) {
      currentValue.value = resolved;
    }
  }
);

function resolveIncomingValue(val) {
  if (val === null || val === undefined || val === '') {
    return resolveInitialValue();
  }
  const parsed = props.mode === 'float' ? parseFloat(val) : parseInt(val, 10);
  if (Number.isNaN(parsed)) {
    return null;
  }
  return parsed;
}

function resolveInitialValue() {
  const fallback = props.field?.init ?? props.field?.min ?? 0;
  const parsed = props.mode === 'float' ? parseFloat(fallback) : parseInt(fallback, 10);
  return Number.isNaN(parsed) ? 0 : parsed;
}

const min = computed(() => {
  const parsed = props.mode === 'float' ? parseFloat(props.field?.min) : parseInt(props.field?.min, 10);
  if (Number.isNaN(parsed)) return props.mode === 'float' ? 0 : 0;
  return parsed;
});

const max = computed(() => {
  const parsed = props.mode === 'float' ? parseFloat(props.field?.max) : parseInt(props.field?.max, 10);
  if (Number.isNaN(parsed)) return props.mode === 'float' ? 1 : 100;
  return parsed;
});

const step = computed(() => {
  if (props.mode === 'float') {
    const precision = typeof props.field?.precision === 'number' ? props.field.precision : 2;
    return Number((1 / Math.pow(10, precision)).toFixed(precision + 1));
  }
  return 1;
});

function onInput(event) {
  const val = props.mode === 'float' ? parseFloat(event.target.value) : parseInt(event.target.value, 10);
  if (!Number.isNaN(val)) {
    currentValue.value = val;
  }
}

function commit() {
  if (Number.isNaN(currentValue.value)) return;
  emit('commit', {
    group: props.group,
    field: props.field,
    value: currentValue.value,
  });
}
</script>
