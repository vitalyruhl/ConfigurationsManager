<template>
  <div class="rw act" :data-group="group" :data-key="field.key">
    <span class="lab">{{ field.label }}</span>
    <span class="val">
      <button
        class="btn"
        type="button"
        :class="pressed ? 'pressed' : ''"
        @pointerdown.prevent="onPointerDown"
        @pointerup.prevent="onPointerUp"
        @pointercancel.prevent="onPointerCancel"
        @pointerleave.prevent="onPointerCancel"
        @click="onClick"
      >
        {{ pressed ? (field.onLabel || 'ON') : (field.offLabel || 'OFF') }}
      </button>
    </span>
    <span class="un"></span>
  </div>
</template>

<script setup>
import { computed, ref } from 'vue';

const props = defineProps({ group: { type: String, required: true }, field: { type: Object, required: true }, value: Boolean });
const emit = defineEmits(['set']);

const pressed = computed(() => !!props.value);

const ignoreNextClick = ref(false);
const isHeldDown = ref(false);

/** @param {boolean} nextValue */
function emitValue(nextValue) {
  emit('set', { group: props.group, field: props.field, value: !!nextValue });
}

function onPointerDown() {
  if (isHeldDown.value) return;
  ignoreNextClick.value = true;
  isHeldDown.value = true;
  emitValue(true);
}

function onPointerUp() {
  if (!isHeldDown.value) return;
  ignoreNextClick.value = true;
  isHeldDown.value = false;
  emitValue(false);
}

function onPointerCancel() {
  if (!isHeldDown.value) return;
  ignoreNextClick.value = true;
  isHeldDown.value = false;
  emitValue(false);
}

function onClick() {
  // Click happens on release; ignore to prevent double-trigger.
  if (ignoreNextClick.value) {
    ignoreNextClick.value = false;
  }
}
</script>

<style scoped>
.rw{ display:grid; grid-template-columns:1fr auto auto; align-items:center; gap:.5rem; }
.lab{ font-weight:600; }
.btn{ padding:.25rem .6rem; border-radius:.4rem; border:1px solid #888; cursor:pointer; background:#f4f4f4; color:#111; }
.btn.pressed{ background:#2ecc71; border-color:#27ae60; color:#fff; }
</style>
