<template>
	<div class="rw sl">
		<span class="lab">{{ field.label }}</span>
		<span class="val">{{ displayValue }}</span>
		<span class="un">{{ field.unit || '' }}</span>
	</div>
	<div class="rw sl-ctrl">
		<input
			type="range"
			:min="min"
			:max="max"
			:step="step"
			v-model.number="local"
			@change="commit"
			@mouseup="commit"
			@touchend="commit"
		/>
	</div>
	<div class="rw sl-num">
		<input
			type="number"
			:min="min"
			:max="max"
			:step="step"
			v-model.number="local"
			@change="commit"
		/>
	</div>
	<!-- spacer row for compactness -->
	<div class="rw sl-gap"></div>
  
</template>

<script setup>
import { computed, ref, watch } from 'vue';

const props = defineProps({
	group: { type: String, required: true },
	field: { type: Object, required: true },
	value: { type: [Number, String], default: 0 },
	mode: { type: String, default: 'int' } // 'int' | 'float'
});

const emit = defineEmits(['commit']);

const min = computed(() => typeof props.field.min === 'number' ? props.field.min : 0);
const max = computed(() => typeof props.field.max === 'number' ? props.field.max : 100);
const precision = computed(() => typeof props.field.precision === 'number' ? props.field.precision : (props.mode === 'float' ? 2 : 0));
const step = computed(() => props.mode === 'float' ? (precision.value >= 3 ? 0.001 : precision.value === 2 ? 0.01 : 0.1) : 1);

const local = ref(parseVal(props.value));

watch(() => props.value, (v) => {
	local.value = parseVal(v);
});

function parseVal(v) {
	const n = typeof v === 'string' ? parseFloat(v) : (typeof v === 'number' ? v : 0);
	if (Number.isNaN(n)) return 0;
	return clamp(n, min.value, max.value);
}

function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }

const displayValue = computed(() => {
	const n = Number(local.value);
	if (props.mode === 'int') return Math.round(n);
	return n.toFixed(precision.value);
});

function commit() {
	let out = Number(local.value);
	if (props.mode === 'int') out = Math.round(out);
	out = clamp(out, min.value, max.value);
	emit('commit', { group: props.group, field: props.field, value: out });
}
</script>

<style scoped>
.rw { display: grid; grid-template-columns: 1fr auto auto; align-items: center; gap: .5rem; }
.lab { font-weight: 600; }
.sl-ctrl { grid-template-columns: 1fr; }
.sl-num { grid-template-columns: 1fr; }
.sl-gap { height: .25rem; }
input[type="range"] { width: 100%; }
input[type="number"] { width: 8rem; justify-self: end; }
</style>
