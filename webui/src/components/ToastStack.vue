<template>
  <div class="toast-wrapper">
    <div
      v-for="toast in toasts"
      :key="toast.id"
      class="toast"
      :class="toast.type"
    >
      <span class="toast-msg">{{ toast.message }}</span>
      <button
        class="toast-close"
        type="button"
        aria-label="Close"
        @click="$emit('dismiss', toast.id)"
      >
        ✕
      </button>
    </div>
  </div>
</template>

<script setup>
const props = defineProps({
  toasts: {
    type: Array,
    default: () => [],
  },
});

defineEmits(['dismiss']);
</script>

<style scoped>
.toast-wrapper {
  position: fixed;
  bottom: 16px;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  flex-direction: column;
  gap: 8px;
  z-index: 2000;
  align-items: center;
}

.toast {
  position: relative;
  background: #303030;
  color: #fff;
  padding: 10px 14px;
  border-radius: 6px;
  min-width: 200px;
  max-width: 360px;
  box-shadow: 0 4px 10px rgba(0, 0, 0, 0.4);
  font-size: 0.85rem;
  line-height: 1.3;
  display: flex;
  align-items: center;
  gap: 10px;
}

.toast.info {
  background: #455a64;
}

.toast.success {
  background: #2e7d32;
}

.toast.error {
  background: #c62828;
}

.toast-close {
  background: transparent;
  border: none;
  color: #fff;
  cursor: pointer;
  font-size: 0.9rem;
  padding: 0 4px;
}

.toast-msg {
  flex: 1;
}
</style>
