<template>
  <div v-if="visible">
  <div v-if="type === 'boolean'" class="setting bool-setting">
      <label class="bool-label" :title="keyName">{{ displayName }}:</label>
      <div class="checkbox-wrapper">
  <label class="switch small">
          <input 
            type="checkbox" 
            :checked="inputValue" 
            @change="onCheckboxChange($event.target.checked)"
          />
          <span class="slider round"></span>
        </label>
      </div>
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
          ref="passwordInputEl"
          :type="passwordVisible ? 'text' : 'password'"
          :placeholder="'***'"
          :value="inputValue"
          @input="onInput($event.target.value)"
          :name="`${category}.${keyName}`"
          data-is-password="1"
        />
        <button
          v-if="isPassword"
          type="button"
          class="pwd-toggle"
          :disabled="passwordBusy"
          @click="togglePasswordVisibility"
          :title="passwordVisible ? 'Hide password' : 'Show password'"
          :aria-label="passwordVisible ? 'Hide password' : 'Show password'"
        >
          <span v-if="passwordBusy">…</span>
          <span v-else class="icon" aria-hidden="true">
            <svg v-if="!passwordVisible" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z" />
              <circle cx="12" cy="12" r="3" />
            </svg>
            <svg v-else xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z" />
              <circle cx="12" cy="12" r="3" />
              <path d="M4 4l16 16" />
            </svg>
          </span>
        </button>
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
.setting{margin:15px 0;padding:15px;background:#f8f9fa;border-radius:5px;display:flex;flex-direction:column;gap:10px;align-items:flex-start}.bool-setting{flex-direction:row;align-items:center;justify-content:space-between;gap:1.25rem;max-width:520px}.bool-setting .bool-label{font-weight:600;padding-right:2rem}label{font-weight:600;min-width:70px}.input-area{flex:2;display:flex;align-items:center;gap:.75rem}.input-area input[type=text],.input-area input[type=number],.input-area input[type=password]{flex:1}.pwd-toggle{background:#f8f9fa;border:1px solid #ddd;border-radius:4px;padding:0.4rem 0.6rem;cursor:pointer;font-size:1rem;transition:background-color 0.2s;flex-shrink:0}.pwd-toggle:hover{background:#e9ecef}.pwd-toggle:active{background:#dee2e6}.actions{display:flex;flex-direction:column;gap:5px;width:100%}.actions button{padding:8px 16px;font-size:14px;border:none;border-radius:4px;cursor:pointer;transition:.3s;width:100%;margin-bottom:5px;box-shadow:0 1px 3px rgba(0,0,0,.12)}.apply-btn{background:#3498db;color:#fff}.save-btn{background:#27ae60;color:#fff}.bool-actions{width:auto;align-items:flex-end}.bool-actions button{width:auto;margin-bottom:0}input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;font-size:14px;box-sizing:border-box}

/* Password toggle: match Apply button sizing, but slightly darker */
.input-area .pwd-toggle{padding:8px 12px;font-size:14px;border:none;border-radius:4px;cursor:pointer;transition:.3s;box-shadow:0 1px 3px rgba(0,0,0,.12);background:#2f86c7;color:#fff;flex-shrink:0;min-width:40px;height:34px;display:flex;align-items:center;justify-content:center}
.input-area .pwd-toggle:disabled{opacity:.6;cursor:not-allowed}
.input-area .pwd-toggle:hover:not(:disabled){filter:brightness(.97)}
.input-area .pwd-toggle .icon{display:flex;align-items:center;justify-content:center}

/* Switch styles for boolean settings */
.checkbox-wrapper {
  display: flex;
  align-items: center;
}
.switch {
  position: relative;
  display: inline-block;
  width: 60px;
  height: 34px;
}
.switch.small { width: 34px; height: 24px; }
.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}
.slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: #ccc;
  transition: .4s;
}
/* Ensure external styles (e.g., width: 3rem) don't override the scoped slider width */
.switch.small .slider { width: 100% !important; }
.slider:before {
  position: absolute;
  content: "";
  height: 26px;
  width: 26px;
  left: 4px;
  bottom: 4px;
  background-color: white;
  transition: .4s;
}
.switch.small .slider:before { height: 18px; width: 18px; left: 3px; bottom: 3px; }
input:checked + .slider {
  background-color: #2196F3;
}
input:focus + .slider {
  box-shadow: 0 0 1px #2196F3;
}
input:checked + .slider:before {
  transform: translateX(26px);
}
/* For small variant, position knob using right instead of transform so it aligns regardless of track width */
.switch.small input:checked + .slider:before {
  transform: none;
  left: auto;
  right: 3px;
}
.slider.round {
  border-radius: 34px;
}
.slider.round:before {
  border-radius: 50%;
}

@media (min-width:768px){.setting{flex-direction:row;align-items:center;max-width:70%}.actions{flex-direction:row;width:auto}.actions button{width:auto}input,select{width:auto;flex:2}}
</style>

<script setup>
import { computed, ref, watch, inject } from 'vue';

const props = defineProps({
  category: String,
  keyName: String,
  settingData: Object,
  busy: { type: Boolean, default: false },
});

const emit = defineEmits(['apply', 'save']);

const notify = inject('notify', null);
const fetchStoredPassword = inject('fetchStoredPassword', null);

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
const passwordVisible = ref(false);
const passwordBusy = ref(false);
const passwordInputEl = ref(null);

function clearRevealedValueMarker() {
  if (passwordInputEl.value && passwordInputEl.value.dataset) {
    delete passwordInputEl.value.dataset.revealedValue;
  }
}

async function togglePasswordVisibility() {
  if (!isPassword) return;

  if (passwordVisible.value) {
    passwordVisible.value = false;
    inputValue.value = '';
    clearRevealedValueMarker();
    return;
  }

  if (typeof fetchStoredPassword !== 'function') {
    if (typeof notify === 'function') notify('Password reveal is not available', 'error');
    return;
  }

  passwordBusy.value = true;
  try {
    const stored = await fetchStoredPassword(props.category, props.keyName);
    inputValue.value = stored;
    passwordVisible.value = true;
    if (passwordInputEl.value && passwordInputEl.value.dataset) {
      passwordInputEl.value.dataset.revealedValue = String(stored);
    }
  } catch (e) {
    const msg = (e && e.message) ? e.message : 'Failed to reveal password';
    if (typeof notify === 'function') notify(`Reveal failed: ${msg}`, 'error');
  } finally {
    passwordBusy.value = false;
  }
}

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

watch(
  () => props.settingData.value,
  (v) => {
    if (type === 'boolean') {
      inputValue.value = !!v;
      return;
    }
    if (isPassword && (!v || v === '***')) {
      inputValue.value = '';
      passwordVisible.value = false;
      clearRevealedValueMarker();
      return;
    }
    inputValue.value = v;
    if (isPassword) {
      passwordVisible.value = false;
      clearRevealedValueMarker();
    }
  }
);

function onInput(val) {
  inputValue.value = val;
}

function onCheckboxChange(checked) {
  inputValue.value = checked;
  emit('apply', props.keyName, inputValue.value);
}
</script>
