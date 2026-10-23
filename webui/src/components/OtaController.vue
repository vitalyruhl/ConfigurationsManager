<template>
  <section class="ota-controller">
    <input
      ref="otaFileInput"
      type="file"
      accept=".bin,.bin.gz"
      class="hidden-file-input"
      @change="onFlashFileSelected"
    />

    <!-- OTA Password Modal -->
    <div v-if="showPasswordModal" class="modal-overlay" @click="cancelPasswordInput">
      <div class="modal-content" @click.stop>
        <h3>OTA Password Required</h3>
        <div class="password-input-container">
          <input
            ref="passwordInput"
            v-model="otaPassword"
            type="password"
            placeholder="Enter OTA password"
            @keyup.enter="confirmPasswordInput"
            @keyup.escape="cancelPasswordInput"
          />
        </div>
        <div class="modal-actions">
          <button @click="cancelPasswordInput" class="cancel-btn">Cancel</button>
          <button @click="confirmPasswordInput" class="confirm-btn">Continue</button>
        </div>
      </div>
    </div>

    <div
      v-if="flashing"
      class="ota-upload-overlay"
      role="alertdialog"
      aria-modal="true"
      aria-label="Firmware upload in progress"
    >
      <div class="ota-upload-card">
        <h3>Firmware Upload</h3>
        <p class="ota-upload-status">{{ uploadStatusText }}</p>
        <div class="ota-progress-track" aria-hidden="true">
          <div
            class="ota-progress-bar"
            :style="{ width: uploadProgressWidth }"
          ></div>
        </div>
        <div class="ota-progress-meta">
          <span>{{ uploadFileName }}</span>
          <span>{{ uploadProgressLabel }}</span>
        </div>
        <p class="ota-upload-detail">{{ uploadStatusDetails }}</p>
      </div>
    </div>

  </section>
</template>
<script setup>
import { ref, computed, watch, inject, nextTick, onMounted, onBeforeUnmount } from 'vue';
import { readJson } from '../jsonHttp.mjs';
const emit = defineEmits(['can-flash-change', 'ota-active-change', 'capabilities-change']);
const notify = inject('notify', () => {});
const updateToast = inject('updateToast', () => {});
const fetchStoredPassword = inject('fetchStoredPassword', null);
const capabilities = ref(null);
let disposed = false, timer, inFlight;
let statusGeneration = 0;
const controller = new AbortController();
const flashing = ref(false);
const uploadProgress = ref(null);
const uploadStatusText = ref("Preparing upload...");
const uploadStatusDetails = ref("Keep this page open until the device accepts the update.");
const uploadFileName = ref("");
const otaFileInput = ref(null);

const otaEndpointAvailable = ref(null); // null = unknown, true = reachable & not 403-disabled, false = definitely disabled/absent

const otaRuntimeActive = computed(() => !!capabilities.value?.otaActive);
const externalOtaActive = computed(() => otaRuntimeActive.value && !flashing.value);
const uploadProgressWidth = computed(() => {
  if (uploadProgress.value === null) return "0%";
  const value = Number(uploadProgress.value);
  if (!Number.isFinite(value)) return "0%";
  return `${Math.max(0, Math.min(100, value))}%`;
});
const uploadProgressLabel = computed(() => {
  if (uploadProgress.value === null) return "Starting...";
  const value = Number(uploadProgress.value);
  if (!Number.isFinite(value)) return "Starting...";
  return `${Math.max(0, Math.min(100, value))}%`;
});
// OTA Password Modal
const showPasswordModal = ref(false);
const otaPassword = ref('');
const passwordInput = ref(null);
const pendingOtaFile = ref(null);
const savedOtaPassword = ref('');


const canFlash = computed(() => otaEndpointAvailable.value === true && !flashing.value && !otaRuntimeActive.value);
watch(canFlash, value => emit('can-flash-change', value), { immediate: true });
watch(() => otaRuntimeActive.value || flashing.value, value => emit('ota-active-change', value), { immediate: true });
function refreshCapabilities() {
  if (inFlight) return inFlight;
  const generation = statusGeneration;
  let retry = false;
  inFlight = readJson('/ui_capabilities', { signal: controller.signal }, 3000)
    .then(value => {
      if (disposed) return;
      if (generation !== statusGeneration) { retry = true; return; }
      if (!value || !['live', 'settings', 'otaEnabled', 'otaActive', 'otaHasPassword', 'settingsAuthRequired']
        .every(key => typeof value[key] === 'boolean')) throw new Error('Invalid device safety status');
      capabilities.value = value;
      otaEndpointAvailable.value = value.otaEnabled === true;
      emit('capabilities-change', value);
    }).catch(() => {
      if (generation === statusGeneration) otaEndpointAvailable.value = false;
      else retry = true;
    })
    .finally(() => {
      inFlight = null;
      if (retry && !disposed) void refreshCapabilities();
    });
  return inFlight;
}
function applyOtaStatus(value) {
  if (value?.type !== 'otaStatus' || typeof value.otaActive !== 'boolean') return;
  // A response started before this event must not clear a newer OTA lock.
  statusGeneration++;
  if (!capabilities.value) { void refreshCapabilities(); return; }
  capabilities.value = { ...capabilities.value, otaActive: value.otaActive,
    otaEnabled: typeof value.otaEnabled === 'boolean' ? value.otaEnabled : capabilities.value.otaEnabled,
    otaHasPassword: typeof value.otaHasPassword === 'boolean' ? value.otaHasPassword : capabilities.value.otaHasPassword };
  otaEndpointAvailable.value = capabilities.value.otaEnabled === true;
  emit('capabilities-change', capabilities.value);
}
function preventNavigationDuringUpload(event) {
  if (!flashing.value) return;
  event.preventDefault();
  event.returnValue = "";
}

watch(flashing, (active) => {
  if (typeof window === "undefined") return;
  if (active) {
    window.addEventListener("beforeunload", preventNavigationDuringUpload);
  } else {
    window.removeEventListener("beforeunload", preventNavigationDuringUpload);
  }
});

function notifySafe(
  message,
  type = "info",
  duration = 3000,
  sticky = false,
  id = null
) {
  return typeof notify === "function"
    ? notify(message, type, duration, sticky, id)
    : null;
}

function updateToastSafe(id, message, type = "info", duration = 2500) {
  if (typeof updateToast === "function") {
    updateToast(id, message, type, duration);
  }
}

function otaPasswordRequired() { return capabilities.value?.otaHasPassword !== false; }

function openOtaFilePicker() {
  if (!otaFileInput.value) {
    notifySafe("Browser file input not ready.", "error");
    return false;
  }
  otaFileInput.value.value = "";
  otaFileInput.value.click();
  return true;
}

async function resolveFlashPassword(options = {}) {
  const passwordRequired = otaPasswordRequired();
  const hasProvidedPassword = options && Object.prototype.hasOwnProperty.call(options, "otaPassword");
  if (hasProvidedPassword) {
    const providedPassword = String(options.otaPassword ?? "");
    if (providedPassword.length || !passwordRequired) {
      return { available: true, password: providedPassword };
    }
  }

  if (typeof fetchStoredPassword === "function") {
    try {
      const storedPassword = String(
        (await fetchStoredPassword("System", "OTAPass")) ?? ""
      );
      if (storedPassword.length || !passwordRequired) {
        return { available: true, password: storedPassword };
      }
    } catch (e) {}
  }

  return { available: false, password: "" };
}

async function startFlash(options = {}) {
  await refreshCapabilities();
  if (!canFlash.value) {
    notifySafe("OTA is disabled", "error");
    return;
  }
  const allowPasswordPrompt = !options || options.allowPasswordPrompt !== false;
  savedOtaPassword.value = '';
  otaPassword.value = '';
  const flashPassword = await resolveFlashPassword(options);
  if (flashPassword.available) {
    savedOtaPassword.value = flashPassword.password;
    openOtaFilePicker();
    return;
  }
  if (!otaPasswordRequired()) {
    openOtaFilePicker();
    return;
  }
  if (!allowPasswordPrompt) {
    notifySafe("Unable to access the stored OTA password. Verify System / OTA Password before flashing.", "error", 8000);
    return;
  }
  showPasswordModal.value = true;
  nextTick(() => {
    if (passwordInput.value) passwordInput.value.focus();
  });
}

async function onFlashFileSelected(event) {
  const files = event.target?.files;
  if (!files || !files.length) {
    return;
  }
  const file = files[0];
  if (!file) {
    return;
  }
  await refreshCapabilities();
  if (!canFlash.value) {
    notifySafe("OTA is disabled", "error");
    return;
  }
  if (file.size === 0) {
    notifySafe("Selected file is empty.", "error");
    otaFileInput.value.value = "";
    return;
  }

  // We already asked for password at button press; use saved one
  const pwd = savedOtaPassword.value || '';
  pendingOtaFile.value = null; // not needed anymore
  await performOtaUpdate(file, pwd);
  if (otaFileInput.value) otaFileInput.value.value = "";
}

// Handle password modal confirmation
function confirmPasswordInput() {
  const password = otaPassword.value.trim();
  if (otaPasswordRequired() && !password.length) {
    notifySafe("OTA password is required to start flashing.", "error");
    nextTick(() => {
      if (passwordInput.value) passwordInput.value.focus();
    });
    return;
  }
  showPasswordModal.value = false;

  // If a file was already selected (unlikely in new flow), upload now; otherwise open picker
  if (pendingOtaFile.value) {
    performOtaUpdate(pendingOtaFile.value, password);
    pendingOtaFile.value = null;
    otaPassword.value = '';
    return;
  }

  // Store password and then prompt for file selection
  savedOtaPassword.value = password;
  if (!otaFileInput.value) {
    notifySafe("Browser file input not ready.", "error");
    return;
  }
  otaFileInput.value.value = "";
  otaFileInput.value.click();
  otaPassword.value = '';
}

// Handle password modal cancellation
function cancelPasswordInput() {
  showPasswordModal.value = false;
  otaPassword.value = '';
  savedOtaPassword.value = '';
  pendingOtaFile.value = null;
  if (otaFileInput.value) {
    otaFileInput.value.value = "";
  }
}

function handleUploadProgress(event) {
  if (!event || !event.lengthComputable || event.total <= 0) {
    uploadProgress.value = null;
    uploadStatusText.value = "Uploading firmware...";
    return;
  }

  const percent = Math.max(0, Math.min(100, Math.round((event.loaded / event.total) * 100)));
  uploadProgress.value = percent;
  uploadStatusText.value = percent >= 100 ? "Finalizing update..." : "Uploading firmware...";
}

function uploadFirmware(file, password) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    const form = new FormData();
    form.append("firmware", file, file.name);

    xhr.open("POST", "/ota_update");
    if (password.length) {
      xhr.setRequestHeader("X-OTA-PASSWORD", password);
    }

    xhr.upload.onprogress = handleUploadProgress;
    xhr.onload = () => {
      resolve({
        ok: xhr.status >= 200 && xhr.status < 300,
        status: xhr.status,
        statusText: xhr.statusText,
        text: xhr.responseText || "",
      });
    };
    xhr.onerror = () => reject(new Error("Network error during upload"));
    xhr.onabort = () => reject(new Error("Upload aborted"));
    xhr.send(form);
  });
}

// Perform the actual OTA update
async function performOtaUpdate(file, password) {
  flashing.value = true;
  uploadFileName.value = file.name;
  uploadProgress.value = 0;
  uploadStatusText.value = "Uploading firmware...";
  uploadStatusDetails.value = "Keep this page open until the device accepts the update.";
  const toastId = notifySafe(`Uploading ${file.name}...`, "info", 15000, true);
  try {
    const response = await uploadFirmware(file, password);
    uploadProgress.value = 100;
    uploadStatusText.value = "Finalizing update...";
    let payload = {};
    const text = response.text;
    try {
      payload = text ? JSON.parse(text) : {};
    } catch (e) {
      payload = { status: "error", reason: text || "invalid_response" };
    }
    if (response.status === 401) {
      updateToastSafe(
        toastId,
        "Unauthorized: wrong OTA password.",
        "error",
        6000
      );
    } else if (response.status === 403 || payload.reason === "ota_disabled") {
      updateToastSafe(toastId, "OTA disabled", "error", 6000);
      otaEndpointAvailable.value = false;
    } else if (!response.ok || payload.status !== "ok") {
      const reason = payload.reason || response.statusText || "Upload failed";
      updateToastSafe(toastId, `Flash failed: ${reason}`, "error", 6000);
    } else {
      uploadStatusText.value = "Upload complete";
      uploadStatusDetails.value = "Waiting for the ESP32 to reboot.";
      updateToastSafe(toastId, "Flash done!", "success", 9000);
      notifySafe("Flash done!", "success", 6000);
      notifySafe("Waiting for reboot...", "info", 8000);
    }
  } catch (error) {
    uploadStatusText.value = "Upload failed";
    uploadStatusDetails.value = error.message;
    updateToastSafe(toastId, `Flash failed: ${error.message}`, "error", 6000);
  } finally {
    savedOtaPassword.value = "";
    await refreshCapabilities();
    flashing.value = false;
    uploadProgress.value = null;
    uploadFileName.value = "";
    if (otaFileInput.value) otaFileInput.value.value = "";
  }
}


onMounted(async () => { await refreshCapabilities(); if (!disposed) timer = setInterval(refreshCapabilities, 2000); });
onBeforeUnmount(() => {
  disposed = true;
  controller.abort();
  clearInterval(timer);
  window.removeEventListener('beforeunload', preventNavigationDuringUpload);
});
defineExpose({ startFlash, refreshCapabilities, applyOtaStatus });
</script>
<style scoped>
.hidden-file-input { display: none; }
.ota-upload-overlay {
  position: fixed;
  inset: 0;
  z-index: 2200;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
  background: rgba(0, 0, 0, 0.72);
}

.ota-upload-card {
  width: min(420px, 92vw);
  padding: 22px;
  border: 1px solid #30363d;
  border-radius: 8px;
  background: #0d1117;
  color: #f0f6fc;
  box-shadow: 0 14px 36px rgba(0, 0, 0, 0.48);
}

.ota-upload-card h3 {
  margin: 0 0 10px;
  font-size: 18px;
}

.ota-upload-status {
  margin: 0 0 14px;
  color: #c9d1d9;
  font-size: 14px;
}

.ota-progress-track {
  width: 100%;
  height: 12px;
  overflow: hidden;
  border-radius: 999px;
  background: #21262d;
}

.ota-progress-bar {
  height: 100%;
  border-radius: inherit;
  background: #238636;
  transition: width 0.18s ease;
}

.ota-progress-meta {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  margin-top: 8px;
  color: #8b949e;
  font-size: 12px;
}

.ota-progress-meta span:first-child {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.ota-upload-detail {
  margin: 14px 0 0;
  color: #8b949e;
  font-size: 13px;
  line-height: 1.35;
}

/* OTA Password Modal */
.modal-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.7);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}

.modal-content {
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 8px;
  padding: 24px;
  max-width: 400px;
  width: 90%;
  color: #f0f6fc;
}

.modal-content h3 {
  margin: 0 0 16px 0;
  color: #f0f6fc;
  font-size: 18px;
}

.password-input-container {
  display: flex;
  gap: 8px;
  margin-bottom: 16px;
}

.password-input-container input {
  flex: 1;
  padding: 8px 12px;
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 4px;
  color: #f0f6fc;
  font-size: 14px;
}

.password-input-container input:focus {
  outline: none;
  border-color: #1f6feb;
  box-shadow: 0 0 0 2px rgba(31, 111, 235, 0.2);
}

.pwd-toggle {
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 4px;
  padding: 8px 12px;
  cursor: pointer;
  color: #8b949e;
  font-size: 14px;
  transition: all 0.2s;
  white-space: nowrap;
}

.pwd-toggle:hover {
  background: #30363d;
  color: #f0f6fc;
}

.modal-actions {
  display: flex;
  gap: 8px;
  justify-content: flex-end;
}

.modal-actions button {
  padding: 8px 16px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 14px;
  transition: all 0.2s;
}

.cancel-btn {
  background: #6e7681;
  color: #f0f6fc;
}

.cancel-btn:hover {
  background: #8b949e;
}

.confirm-btn {
  background: #238636;
  color: #f0f6fc;
}

.confirm-btn:hover {
  background: #2ea043;
}
</style>
