<template>
  <div v-if="visible">
    <div v-if="type === 'boolean'" class="setting bool-setting">
      <RuntimeCheckbox
        :group="category"
        :field="checkboxField"
        :value="inputValue"
        @change="onCheckboxChange"
      />
      <div class="actions bool-actions">
        <button class="save-btn" :disabled="busy" @click="emit('save', keyName, inputValue)">
          <span v-if="busy">…</span><span v-else>Save</span>
        </button>
      </div>
    </div>
    <div v-else class="setting">
      <label :title="keyName">{{ displayName }}:</label>
      <div class="input-area">
        <input
          v-if="isPassword"
          type="password"
          :placeholder="'***'"
          :value="inputValue"
          @input="onInput($event.target.value)"
          :name="`${category}.${keyName}`"
          data-is-password="1"
        />
        <input
          v-else-if="type === 'number'"
          type="number"
          :value="inputValue"
          @input="onInput($event.target.value)"
          :name="`${category}.${keyName}`"
        />
        <input
          v-else
          type="text"
          :value="inputValue"
          @input="onInput($event.target.value)"
          :name="`${category}.${keyName}`"
        />
      </div>
      <div class="actions">
        <button
          class="apply-btn"
          :disabled="busy"
          @click="emit('apply', keyName, inputValue)"
        >
          <span v-if="busy">…</span><span v-else>Apply</span>
        </button>
        <button class="save-btn" :disabled="busy" @click="emit('save', keyName, inputValue)">
          <span v-if="busy">…</span><span v-else>Save</span>
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.setting {
  margin: 15px 0;
  padding: 15px;
  background: #f8f9fa;
  border-radius: 5px;
  display: flex;
  flex-direction: column;
  gap: 10px;
  align-items: flex-start;
}

.bool-setting {
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
  gap: 1.25rem;
  max-width: 520px;
}

.bool-setting :deep(.rw) {
  margin: 0;
}

.bool-setting :deep(.lab) {
  font-weight: 600;
  padding-right: 2rem;
}

label {
  font-weight: 600;
  min-width: 70px;
}

.input-area {
  flex: 2;
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.input-area input[type='text'],
.input-area input[type='number'],
.input-area input[type='password'] {
  flex: 1;
}

.actions {
  display: flex;
  flex-direction: column;
  gap: 5px;
  width: 100%;
}

.actions button {
  padding: 8px 16px;
  font-size: 14px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: 0.3s;
  width: 100%;
  margin-bottom: 5px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.12);
}

.apply-btn {
  background-color: #3498db;
  color: #fff;
}

.save-btn {
  background-color: #27ae60;
  color: #fff;
}

.bool-actions {
  width: auto;
  align-items: flex-end;
}

.bool-actions button {
  width: auto;
  margin-bottom: 0;
}

input,
select {
  width: 100%;
  padding: 8px;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 14px;
  box-sizing: border-box;
}

@media (min-width: 768px) {
  .setting {
    flex-direction: row;
    align-items: center;
    max-width: 70%;
  }

  .actions {
    flex-direction: row;
    width: auto;
  }

  .actions button {
    width: auto;
  }

  input,
  select {
    width: auto;
    flex: 2;
  }
}
</style>

<script setup>
import { computed, ref, watch } from 'vue';
import RuntimeCheckbox from './runtime/RuntimeCheckbox.vue';

const props = defineProps({
  category: String,
  keyName: String,
  settingData: Object,
  busy: { type: Boolean, default: false },
});

const emit = defineEmits(['apply', 'save']);

const displayName =
  props.settingData.displayName || props.keyName;

const isPassword =
  props.keyName.toLowerCase().includes('pass') ||
  props.settingData.isPassword === true;

const type =
  typeof props.settingData.value === 'boolean'
    ? 'boolean'
    : typeof props.settingData.value === 'number'
      ? 'number'
      : 'string';

function initialValue() {
  if (type === 'boolean') {
    return !!props.settingData.value;
  }
  if (isPassword && (!props.settingData.value || props.settingData.value === '***')) {
    return '';
  }
  return props.settingData.value;
}

const inputValue = ref(initialValue());

// Visibility: the server can deliver a resolved showIf as boolean
const visible = ref(true);

if (
  Object.prototype.hasOwnProperty.call(props.settingData, 'showIf') &&
  typeof props.settingData.showIf === 'boolean'
) {
  visible.value = props.settingData.showIf;
} else if (
  Object.prototype.hasOwnProperty.call(props.settingData, 'showIfResolved') &&
  typeof props.settingData.showIfResolved === 'boolean'
) {
  visible.value = props.settingData.showIfResolved;
}

const checkboxField = computed(() => ({
  key: props.keyName,
  label: displayName,
}));

watch(
  () => props.settingData.value,
  (v) => {
    inputValue.value = type === 'boolean' ? !!v : v;
  }
);

function onInput(val) {
  inputValue.value = val;
}

function onCheckboxChange(event) {
  const next =
    event && Object.prototype.hasOwnProperty.call(event, 'checked')
      ? !!event.checked
      : !!event;
  inputValue.value = next;
  emit('apply', props.keyName, inputValue.value);
}
</script>
