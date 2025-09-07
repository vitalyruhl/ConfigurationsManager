<template>
  <div class="setting">
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
          @change="onInput($event.target.checked)"
          :name="`${category}.${keyName}`"
        />
        <span class="slider"></span>
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
      <button class="apply-btn" @click="emit('apply', keyName, inputValue)">Apply</button>
      <button class="save-btn" @click="emit('save', keyName, inputValue)">Save</button>
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
    position: relative;
    display: inline-block;
    width: 32px;
    height: 20px;
    margin: 0 8px 0 0;
    vertical-align: middle;
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
    border-radius: 20px;
  }
  .slider:before {
    position: absolute;
    content: "";
    height: 14px;
    width: 14px;
    left: 3px;
    bottom: 3px;
    background-color: white;
    transition: .4s;
    border-radius: 50%;
    box-shadow: 0 1px 3px rgba(0,0,0,0.2);
  }
  .switch input:checked + .slider {
    background-color: #4caf50;
  }
  .switch input:checked + .slider:before {
    transform: translateX(12px);
  }
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
    border-radius: 24px;
  }
  .slider:before {
    position: absolute;
    content: "";
    height: 18px;
    width: 18px;
    left: 3px;
    bottom: 3px;
    background-color: white;
    transition: .4s;
    border-radius: 50%;
    box-shadow: 0 1px 3px rgba(0,0,0,0.2);
  }
  .switch input:checked + .slider {
    background-color: #4caf50;
  }
  .switch input:checked + .slider:before {
    transform: translateX(16px);
  }
label {
  font-weight: bold;
  min-width: 120px;
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
  settingData: Object
});
const emit = defineEmits(['apply', 'save']);
const displayName = props.settingData.displayName || props.keyName;
const isPassword = props.keyName.toLowerCase().includes('pass') || props.settingData.isPassword === true;
const type = typeof props.settingData.value === 'boolean' ? 'boolean' : typeof props.settingData.value === 'number' ? 'number' : 'string';
const inputValue = ref(isPassword && (!props.settingData.value || props.settingData.value === '***') ? '' : props.settingData.value);
watch(() => props.settingData.value, v => { inputValue.value = v; });
function onInput(val) {
  inputValue.value = val;
}
</script>
