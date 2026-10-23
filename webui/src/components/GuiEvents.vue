<template>
  <div v-if="current" class="gui-overlay" role="alertdialog" :aria-label="current.title">
    <section class="gui-card">
      <h2>{{ current.title }}</h2>
      <p>{{ current.message }}</p>
      <p v-if="current.details">{{ current.details }}</p>
      <button v-for="action in current.actions" :key="action.id" :disabled="working || disabled" @click="runAction(action)">{{ action.label }}</button>
    </section>
  </div>
</template>
<script setup>
import { ref, inject, onMounted, onBeforeUnmount } from 'vue';
const props = defineProps({ disabled: Boolean });
const notify = inject('notify', () => {});
const onOtaStatus = inject('onOtaStatus', () => {});
const current = ref(null), working = ref(false);
const queue = [];
let socket, timer, disposed = false;
function receive(event) {
  if (event.data === '__ping') { socket?.send('__pong'); return; }
  try {
    const value = JSON.parse(event.data);
    if (value?.type === 'otaStatus') { onOtaStatus(value); return; }
    if (!['errorMessage', 'warningMessage', 'infoMessage'].includes(value?.type)) return;
    const message = { ...value, title: value.title || 'Information',
      actions: Array.isArray(value.actions) && value.actions.length ? value.actions : [{ id: 'close', label: 'Close' }] };
    if (current.value) queue.push(message); else current.value = message;
  } catch { /* Ignore malformed and unrelated frames. */ }
}
async function runAction(action) {
  if (props.disabled || working.value) return;
  if (action.id === 'close' || action.id === 'cancel') { current.value = queue.shift() || null; return; }
  const context = current.value?.context;
  if (context?.guiActionEndpoint !== '/gui/action' || !context.guiMessageId) { notify('Unable to apply the selected action', 'error'); return; }
  working.value = true;
  try {
    const query = new URLSearchParams({ actionId: action.id, messageId: context.guiMessageId });
    const response = await fetch(`/gui/action?${query}`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
    const result = await response.json();
    if (!response.ok || result.status !== 'ok') throw new Error('Action unavailable');
    notify(`${action.label || 'Action'} succeeded`, 'success');
  } catch (error) { notify(`${action.label || 'Action'} failed: ${error.message}`, 'error'); }
  finally { working.value = false; current.value = queue.shift() || null; }
}
function connect() {
  if (disposed) return;
  socket = new WebSocket(`${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/ws?runtime=none`);
  socket.onmessage = receive;
  const retry = () => { if (!disposed && !timer) timer = setTimeout(() => { timer = null; connect(); }, 2000); };
  socket.onclose = retry;
  socket.onerror = retry;
}
onMounted(connect);
onBeforeUnmount(() => { disposed = true; clearTimeout(timer); socket?.close(); });
</script>
<style scoped>
.gui-overlay { position: fixed; inset: 0; background: #0009; display: grid; place-items: center; z-index: 2000; }
.gui-card { background: var(--cm-card-bg, white); color: var(--cm-fg, #222); padding: 1.5rem; border-radius: .5rem; max-width: min(540px, 90vw); }
button { margin: .5rem; padding: .6rem 1rem; }
</style>
