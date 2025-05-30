#pragma once
#include <Arduino.h>

// Version 1.1.2 - 23.05.2025 - viru - Bugfix add forgotten function applyAll() in html

const char WEB_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>ESP32 Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style>
      body {
        font-family: Arial, sans-serif;
        margin: 2rem;
        background-color: #f0f0f0;
      }
      .category {
        margin: 20px 0;
        padding: 20px;
        background: white;
        border-radius: 10px;
        box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
      }
      h1 {
        color: #2c3e50;
        text-align: center;
      }
      h2 {
        color: #3498db;
        margin-top: 0;
        border-bottom: 2px solid #3498db;
        padding-bottom: 10px;
      }
      .setting {
        margin: 15px 0;
        padding: 15px;
        background: #f8f9fa;
        border-radius: 5px;
        display: flex;
        align-items: center;
        gap: 15px;
      }
      label {
        flex: 1;
        font-weight: bold;
        min-width: 120px;
      }
      input,
      select {
        flex: 2;
        padding: 8px;
        border: 1px solid #ddd;
        border-radius: 4px;
        font-size: 14px;
      }
      button {
        padding: 8px 16px;
        font-size: 14px;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        transition: 0.3s;
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
      #status {
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 15px;
        border-radius: 5px;
        display: none;
        max-width: 300px;
      }
      .action-buttons {
        text-align: center;
        margin: 20px 0;
      }
    </style>
  </head>
  <body>
    <h1>Device Configuration</h1>
    <div id="status"></div>
    <div id="settingsContainer"></div>

    <div class="action-buttons">
      <button onclick="applyAll()" class="apply-btn">Apply All</button>
      <button onclick="saveAll()" class="save-btn">Save All</button>
      <button onclick="resetDefaults()" class="reset-btn">
        Reset Defaults
      </button>
      <button onclick="rebootDevice()" class="reset-btn">Reboot</button>
    </div>

    <script>
      async function loadSettings() {
        try {
          const response = await fetch("/config.json");
          if (!response.ok) throw new Error("HTTP Error");
          const config = await response.json();
          renderSettings(config);
        } catch (error) {
          showStatus("Error: " + error.message, "red");
        }
      }

      function renderSettings(config) {
        const container = document.getElementById("settingsContainer");
        container.innerHTML = "";

        for (const [category, settings] of Object.entries(config)) {
          const categoryDiv = document.createElement("div");
          categoryDiv.className = "category";
          categoryDiv.innerHTML = `<h2>${category}</h2>`;

          for (const [key, value] of Object.entries(settings)) {
            const settingDiv = document.createElement("div");
            settingDiv.className = "setting";

            const label = document.createElement("label");
            label.textContent = key + ":";

            const input = createInput(category, key, value);
            const actions = document.createElement("div");
            actions.className = "actions";

            settingDiv.append(label, input, actions);
            categoryDiv.appendChild(settingDiv);
          }
          container.appendChild(categoryDiv);
        }
      }

      function createInput(category, key, value) {
        const wrapper = document.createElement("div");
        wrapper.style.flex = "2";
        wrapper.style.display = "flex";
        wrapper.style.gap = "10px";

        let input;
        if (key.toLowerCase().includes("pass")) {
          input = document.createElement("input");
          input.type = "password";
          input.value = value;
        } else if (typeof value === "boolean") {
          input = document.createElement("input");
          input.type = "checkbox";
          input.checked = value;
        } else if (typeof value === "number") {
          input = document.createElement("input");
          input.type = "number";
          input.value = value;
        } else {
          input = document.createElement("input");
          input.type = "text";
          input.value = value;
        }

        input.name = `${category}.${key}`;
        input.style.flex = "1";

        const applyBtn = document.createElement("button");
        applyBtn.textContent = "Apply";
        applyBtn.className = "apply-btn";
        applyBtn.onclick = () => applySingle(category, key, input);

        const saveBtn = document.createElement("button");
        saveBtn.textContent = "Save";
        saveBtn.className = "save-btn";
        saveBtn.onclick = () => saveSingle(category, key, input);

        wrapper.append(input, applyBtn, saveBtn);
        return wrapper;
      }

      async function applySingle(category, key, input) {
        const value = input.type === "checkbox" ? input.checked : input.value;
        try {
          const response = await fetch(
            `/config/apply?category=${encodeURIComponent(
              category
            )}&key=${encodeURIComponent(key)}`,
            {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({ value: value }),
            }
          );
          if (!response.ok) throw new Error("Apply failed");
        } catch (error) {
          console.error("Error:", error);
        }
      }

      async function saveSingle(category, key, input) {
        const value = input.type === "checkbox" ? input.checked : input.value;
        try {
          const response = await fetch(
            `/config/save?category=${encodeURIComponent(
              category
            )}&key=${encodeURIComponent(key)}`,
            {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({ value: value }),
            }
          );
          if (!response.ok) throw new Error("Apply failed");
        } catch (error) {
          console.error("Error:", error);
        }
      }

      async function saveAll() {
        if (!confirm("Save all settings?")) return;

        const config = {};
        document.querySelectorAll("input").forEach((input) => {
          const [category, key] = input.name.split(".");
          if (!config[category]) config[category] = {};
          config[category][key] =
            input.type === "checkbox" ? input.checked : input.value;
        });

        try {
          const response = await fetch("/config/save_all", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(config),
          });
          showStatus(
            response.ok ? "All settings saved!" : "Error!",
            response.ok ? "green" : "red"
          );
        } catch (error) {
          showStatus("Error: " + error.message, "red");
        }
      }

      async function applyAll() {
        if (!confirm("Apply all settings?")) return;

        const config = {};
        document.querySelectorAll("input").forEach((input) => {
          const [category, key] = input.name.split(".");
          if (!config[category]) config[category] = {};
          config[category][key] =
            input.type === "checkbox" ? input.checked : input.value;
        });

        try {
          const response = await fetch("/config/apply_all", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(config),
          });
          showStatus(
            response.ok ? "All settings Applied!" : "Error!",
            response.ok ? "green" : "red"
          );
        } catch (error) {
          showStatus("Error: " + error.message, "red");
        }
      }

      async function resetDefaults() {
        if (!confirm("Reset to defaults?")) return;
        try {
          const response = await fetch("/config/reset", { method: "POST" });
          if (response.ok) {
            showStatus("Reset successful!", "green");
            setTimeout(() => location.reload(), 1000);
          }
        } catch (error) {
          showStatus("Error: " + error.message, "red");
        }
      }

      function rebootDevice() {
        if (!confirm("Reboot device?")) return;
        fetch("/config/reboot", { method: "POST" })
          .then((response) =>
            response.ok ? showStatus("Rebooting...", "blue") : null
          )
          .catch((error) => showStatus("Error: " + error.message, "red"));
      }

      function showStatus(message, color) {
        const statusDiv = document.getElementById("status");
        statusDiv.textContent = message;
        statusDiv.style.backgroundColor = color;
        statusDiv.style.display = "block";
        setTimeout(() => (statusDiv.style.display = "none"), 3000);
      }

      window.onload = loadSettings;
    </script>
  </body>
</html>

)rawliteral";

class WebHTML {
public:
const char* getWebHTML();
};