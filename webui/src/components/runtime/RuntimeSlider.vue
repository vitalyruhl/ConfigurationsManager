<template>
  <div class="rw sl" :data-group="group" :data-key="field.key">
    <span class="lab">{{ field.label }}</span>
    <span class="val sw">
      <input
        type="range"
        :min="min"
        :max="max"
        :step="step"
        :value="displaySliderValue"
        @input="onInput"
        class="asl"
      />
      <span class="sv">{{ formattedValue }}</span>
      <span class="un">{{ field.unit || ' ' }}</span>
      <button class="btn sb" type="button" @click="commit">Set</button>
    </span>
  </div>
</template>
<style scoped>
.sv {margin: 0 .5rem;}
.un {margin: 0 .5rem;}
.val .sw {margin: 0 .5rem;}
.asl {vertical-align: middle;max-width: 8rem;}
</style>

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
    if (!Number.isNaN(resolved)) {
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
    return resolveInitialValue();
  }
  return parsed;
}

function resolveInitialValue() {
  const fallback = props.field?.init ?? props.field?.min ?? 0;
  return resolveNumeric(fallback);
}

function resolveNumeric(v) {
  const parsed = props.mode === 'float' ? parseFloat(v) : parseInt(v, 10);
  return Number.isNaN(parsed) ? 0 : parsed;
}

const min = computed(() => resolveNumeric(props.field?.min ?? 0));
const max = computed(() => resolveNumeric(props.field?.max ?? (props.mode === 'float' ? 1 : 100)));
const step = computed(() => {
  if (props.mode === 'float') {
    const precision = typeof props.field?.precision === 'number' ? props.field.precision : 2;
    return Number((1 / Math.pow(10, precision)).toFixed(precision + 1));
  }
  return 1;
});

const displaySliderValue = computed(() => currentValue.value);

const formattedValue = computed(() => {
  if (props.mode === 'float') {
    const precision = typeof props.field?.precision === 'number' ? props.field.precision : 2;
    return Number.isNaN(currentValue.value)
      ? '-'
      : Number(currentValue.value).toFixed(precision);
  }
  return Number.isNaN(currentValue.value) ? '-' : currentValue.value;
});

function onInput(ev) {
  const val = props.mode === 'float' ? parseFloat(ev.target.value) : parseInt(ev.target.value, 10);
  currentValue.value = Number.isNaN(val) ? currentValue.value : val;
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
