<!-- Enabled slider implementation -->
<template>
  <div class="rw sl">
    <span class="lab">{{ field.label }}</span>
    <span class="val">{{ displayValue }}</span>
    <span class="un">{{ field.unit || '' }}</span>
  </div>
  <div class="rw sl-ctrl">
    <input type="range" :min="min" :max="max" :step="step" v-model.number="local" />
  </div>
  <div class="rw sl-actions">
    <span></span>
    <div class="btns">
      <button class="btn apply" :disabled="!dirty" @click="commit">Set</button>
      <button class="btn reset" :disabled="!dirty" @click="reset">Reset</button>
    </div>
    <span></span>
  </div>
  <div class="rw sl-gap"></div>
  </template>

<script setup>
import { computed, ref, watch } from 'vue';
const props = defineProps({ group: String, field: Object, value: [Number, String], mode: { type: String, default: 'int' } });
const emit = defineEmits(['commit']);
const min = computed(() => typeof props.field.min === 'number' ? props.field.min : 0);
const max = computed(() => typeof props.field.max === 'number' ? props.field.max : 100);
const precision = computed(() => typeof props.field.precision === 'number' ? props.field.precision : (props.mode === 'float' ? 2 : 0));
const step = computed(() => props.mode === 'float' ? (precision.value >= 3 ? 0.001 : precision.value === 2 ? 0.01 : 0.1) : 1);
const local = ref(parseVal(props.value));
const original = computed(() => parseVal(props.value));
watch(() => props.value, v => { local.value = parseVal(v); });
function parseVal(v){ const n = typeof v === 'string' ? parseFloat(v) : (typeof v==='number'?v:0); return Number.isNaN(n)?0:clamp(n,min.value,max.value); }
function clamp(v,lo,hi){ return Math.min(hi, Math.max(lo, v)); }
const displayValue = computed(() => { const n = Number(local.value); return props.mode==='int'? Math.round(n): n.toFixed(precision.value);});
const dirty = computed(() => { const cur=Number(local.value), orig=Number(original.value); return props.mode==='int'? Math.round(cur)!==Math.round(orig) : Number(cur.toFixed(precision.value))!==Number(orig.toFixed(precision.value)); });
function commit(){ let out = Number(local.value); if(props.mode==='int') out = Math.round(out); out = clamp(out, min.value, max.value); emit('commit', { group: props.group, field: props.field, value: out }); }
function reset(){ local.value = original.value; }
</script>

<style scoped>
.rw { display:grid; grid-template-columns:1fr auto auto; align-items:center; gap:.5rem; }
.sl-ctrl { grid-template-columns: 1fr; }
.sl-actions { grid-template-columns: 1fr auto 1fr; }
.sl-gap{ height:.25rem; }
input[type="range"]{ width:100%; }
.btns{ display:flex; gap:.5rem; }
.btn{ padding:.25rem .6rem; border-radius:.4rem; border:1px solid #888; cursor:pointer; }
.btn:disabled{ opacity:.6; cursor:not-allowed; }
.btn.apply{ background:#0b5; color:#fff; border-color:#0a4; }
.btn.reset{ background:#777; color:#fff; border-color:#666; }
</style>
