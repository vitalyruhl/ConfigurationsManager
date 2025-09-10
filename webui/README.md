# ESP32 ConfigurationsManager Web UI (Vue)

This folder contains the Vue 3 project for the ESP32 configuration web interface.

## Development

1. Install dependencies:
   ```sh
   npm install
   ```
2. Start development server:
   ```sh
   npm run dev
   ```

## Build for ESP32

1. Build the project:
   ```sh
   npm run build
   ```
2. Copy the output `dist/index.html` into `src/html_content.h` as a C++ string literal (replace the old HTML).
   - You can use a script or do this manually.

## Project Structure
- `src/App.vue`: Main application, based on your previous HTML.
- `src/components/Category.vue`: Renders a category and its settings.
- `src/components/Setting.vue`: Renders a single setting.

## Notes
- All logic from the old HTML/JS is ported to Vue methods.
- Styling is preserved.
- The ESP32 firmware serves the built HTML from `html_content.h`.
