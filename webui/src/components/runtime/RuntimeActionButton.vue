<template>
  <div class="rw act" :data-group="group" :data-key="field.key">
    <span class="lab">{{ field.label }}</span>
    <span class="val">
      <button
        class="btn"
        type="button"
        @pointerdown.prevent="onPointerDown"
        @click="onClick"
      >
        {{ field.label }}
      </button>
    </span>
    <span class="un"></span>
  </div>
</template>

<script setup>
import { computed, ref } from 'vue';

const props = defineProps({ group: { type: String, required: true }, field: { type: Object, required: true } });
const emit = defineEmits(['action']);

const ignoreNextClick = ref(false);

const triggerOnPress = computed(() => props.field && props.field.triggerOnPress === true);

function emitAction(){
  emit('action', { group: props.group, key: props.field.key, field: props.field });
}

function onPointerDown(){
  if (!triggerOnPress.value) return;
  ignoreNextClick.value = true;
  emitAction();
}

function onClick(){
  if (ignoreNextClick.value) {
    ignoreNextClick.value = false;
    return;
  }
  emitAction();
}
</script>
