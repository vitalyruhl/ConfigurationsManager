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

Normally you don't need to run anything here manually. The PlatformIO prebuild script (`tools/preCompile_script.py`) will:

- Install dependencies if missing
- Build the Vue app with the right feature flags
- Gzip the final HTML and generate `src/html_content.h`

If you want to test a build locally:

```sh
npm run build
```

## Project Structure
- `src/App.vue`: Main application, based on your previous HTML.
- `src/components/Category.vue`: Renders a category and its settings.
- `src/components/Setting.vue`: Renders a single setting.

## Notes
- All logic from the old HTML/JS is ported to Vue methods.
- Styling is preserved.
- The ESP32 firmware serves the built HTML from `html_content.h`.
