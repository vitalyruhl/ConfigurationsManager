<template>
  <div class="rw act" :data-group="group" :data-key="field.key">
    <span class="lab">{{ field.label }}</span>
    <span class="val">
      <button class="btn" type="button" @click="emitAction">{{ field.label }}</button>
    </span>
    <span class="un"></span>
  </div>
</template>

<script setup>

const props = defineProps({
  group: {
    type: String,
    required: true,
  },
  field: {
    type: Object,
    required: true,
  },
});

const emit = defineEmits(['action']);

function emitAction() {
  emit('action', {
    group: props.group,
    key: props.field.key,
    field: props.field,
  });
}
</script>
