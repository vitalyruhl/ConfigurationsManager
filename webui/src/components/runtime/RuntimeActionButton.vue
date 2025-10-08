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

.btn:active {
  background: #0d4674;
}
</style>
