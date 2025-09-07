<template>
  <div class="setting">
    <label :title="keyName">{{ displayName }}:</label>
    <div style="flex:2;display:flex;">
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
      <input
        v-else-if="type === 'boolean'"
        type="checkbox"
        :checked="inputValue"
        @change="onInput($event.target.checked)"
        :name="`${category}.${keyName}`"
        style="flex:1;"
      />
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
