<template>
  <div class="rw sb">
    <span class="lab">{{ field.label }}</span>
    <button
      class="btn"
      :class="on ? 'on' : 'off'"
      @pointerdown.prevent="onPointerDown"
      @pointerup.prevent="onPointerUp"
      @pointercancel.prevent="onPointerCancel"
      @pointerleave.prevent="onPointerCancel"
      @click="onClick"
    >
      {{ on ? (field.onLabel || 'ON') : (field.offLabel || 'OFF') }}
    </button>
    <span class="un"></span>
  </div>
</template>

<script setup>
import { computed, ref } from 'vue';
const props = defineProps({ group: String, field: Object, value: Boolean });
const emit = defineEmits(['toggle']);
const on = computed(() => !!props.value);

const ignoreNextClick = ref(false);
const isHeldDown = ref(false);

const holdToActivate = computed(() => {
  if (props.field && props.field.holdToActivate === true) return true;
  if (props.field && props.field.triggerOnPress === true) return true;
  return false;
});

function emitNext(nextValue){
  emit('toggle', { group: props.group, field: props.field, nextValue: !!nextValue });
}

function toggle(){
  emitNext(!on.value);
}

function onPointerDown(){
  if (!holdToActivate.value) return;
  if (isHeldDown.value) return;
  ignoreNextClick.value = true;
  isHeldDown.value = true;
  emitNext(true);
}

function onPointerUp(){
  if (!holdToActivate.value) return;
  if (!isHeldDown.value) return;
  ignoreNextClick.value = true;
  isHeldDown.value = false;
  emitNext(false);
}

function onPointerCancel(){
  if (!holdToActivate.value) return;
  if (!isHeldDown.value) return;
  ignoreNextClick.value = true;
  isHeldDown.value = false;
  emitNext(false);
}

function onClick(){
  if (ignoreNextClick.value) {
    ignoreNextClick.value = false;
    return;
  }
  if (holdToActivate.value) {
    // Hold-mode buttons are controlled via pointer down/up only.
    return;
  }
  toggle();
}
</script>

<style scoped>
.rw{ display:grid; grid-template-columns:1fr auto auto; align-items:center; gap:.5rem; }
.lab{ font-weight:600; }
.btn{ padding:.25rem .6rem; border-radius:.4rem; border:1px solid #888; cursor:pointer; }
.btn.on{ background:#2ecc71; color:#fff; border-color:#27ae60; }
.btn.off{ background:#e74c3c; color:#fff; border-color:#c0392b; }
</style>
