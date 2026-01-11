// Centralized frontend debug logging (build-time enabled)
// Default: OFF. Enable by building the WebUI with VITE_CM_DEBUG=1.

export const CM_DEBUG = String(import.meta.env?.VITE_CM_DEBUG || '').trim() === '1';

/**
 * Debug log helper.
 * @param  {...any} args
 */
export function dbg(...args) {
  if (!CM_DEBUG) return;
  // eslint-disable-next-line no-console
  console.log(...args);
}
