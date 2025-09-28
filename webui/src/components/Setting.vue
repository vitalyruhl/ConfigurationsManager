<template>
  <div class="setting" v-if="visible">
    <label :title="keyName">{{ displayName }}:</label>
  <div style="flex:2;display:flex;justify-content:flex-start;">
      <input
        v-if="isPassword"
        type="password"
        :placeholder="'***'"
        :value="inputValue"
        @input="onInput($event.target.value)"
        :name="`${category}.${keyName}`"
        data-is-password="1"
        style="flex:1;"
      />
      <label v-else-if="type === 'boolean'" class="switch">
        <input
          type="checkbox"
          :checked="inputValue"
          @change="onToggle($event.target.checked)"
          :name="`${category}.${keyName}`"
        />
  <span class="slider round"></span>
      </label>
      <input
        v-else-if="type === 'number'"
        type="number"
        :value="inputValue"
        @input="onInput($event.target.value)"
        :name="`${category}.${keyName}`"
        style="flex:1;"
      />
      <input
        v-else
        type="text"
        :value="inputValue"
        @input="onInput($event.target.value)"
        :name="`${category}.${keyName}`"
        style="flex:1;"
      />
    </div>
    <div class="actions">
      <button
        v-if="type !== 'boolean'"
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

  /* Android-Style Toggle Switch */
  .switch {
    display: inline-block;
    height: 34px;
    position: relative;
    width: 30px;
  }

  .switch input {
    display:none;
  }

  .slider {
    background-color: #ccc;
    bottom: 0;
    cursor: pointer;
    left: 0;
    position: absolute;
    right: 0;
    top: 0;
    transition: .4s;
  }

  .slider:before {
    background-color: #fff;
    bottom: 4px;
    content: "";
    height: 26px;
    left: 4px;
    position: absolute;
    transition: .4s;
    width: 26px;
  }

  input:checked + .slider {
    background-color: #66bb6a;
  }

  input:checked + .slider:before {
    transform: translateX(26px);
  }

  .slider.round {
    border-radius: 34px;
  }

  .slider.round:before {
    border-radius: 50%;
  }
/* end of toggle switch */

label {
  font-weight: bold;
  min-width: 70px;
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
button {
  padding: 8px 16px;
  font-size: 14px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: 0.3s;
  width: 100%;
  margin-bottom: 5px;
}
.apply-btn {
  background-color: #3498db;
  color: white;
}
.save-btn {
  background-color: #27ae60;
  color: white;
}
.reset-btn {
  background-color: #e74c3c;
  color: white;
}
.actions {
  display: flex;
  flex-direction: column;
  gap: 5px;
  width: 100%;
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
  button {
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
import { ref, watch } from 'vue';
const props = defineProps({
  category: String,
  keyName: String,
  settingData: Object,
  busy: { type: Boolean, default: false }
});
const emit = defineEmits(['apply', 'save']);
const displayName = props.settingData.displayName || props.keyName;
const isPassword = props.keyName.toLowerCase().includes('pass') || props.settingData.isPassword === true;
const type = typeof props.settingData.value === 'boolean' ? 'boolean' : typeof props.settingData.value === 'number' ? 'number' : 'string';
const inputValue = ref(isPassword && (!props.settingData.value || props.settingData.value === '***') ? '' : props.settingData.value);
// Visibility: the server can deliver a resolved showIf as boolean
const visible = ref(true);
if (Object.prototype.hasOwnProperty.call(props.settingData, 'showIf') && typeof props.settingData.showIf === 'boolean') {
  visible.value = props.settingData.showIf;
} else if (Object.prototype.hasOwnProperty.call(props.settingData, 'showIfResolved') && typeof props.settingData.showIfResolved === 'boolean') {
  visible.value = props.settingData.showIfResolved;
}
watch(() => props.settingData.value, v => { inputValue.value = v; });
function onInput(val) {
  inputValue.value = val;
}
function onToggle(val){
  // Ensure boolean stored
  inputValue.value = !!val;
  // Auto-apply so dependent showIf fields refresh quickly
  emit('apply', props.keyName, inputValue.value);
}
</script>
