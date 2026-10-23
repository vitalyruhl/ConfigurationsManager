// Inspect the real Vite dependency graph and the unminified equivalent without changing built assets.
import { build } from '../../webui/node_modules/vite/dist/node/index.js';
import { writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
const [webui, destination] = process.argv.slice(2);
process.chdir(resolve(webui));
let modules;
const output = await build({ build: { write: false, minify: false, cssMinify: false }, plugins: [{
  name: 'cm-ui-feature-contract',
  generateBundle() { modules = [...this.getModuleIds()].filter(id => !id.startsWith('\0')).sort(); },
}] });
const bundles = Array.isArray(output) ? output : [output];
const entries = bundles.flatMap(bundle => bundle.output);
const bytes = value => Buffer.byteLength(typeof value === 'string' ? value : Buffer.from(value));
writeFileSync(destination, JSON.stringify({ modules,
  unminified_js_bytes: entries.filter(entry => entry.type === 'chunk').reduce((sum, entry) => sum + bytes(entry.code), 0),
  unminified_css_bytes: entries.filter(entry => entry.type === 'asset' && entry.fileName.endsWith('.css')).reduce((sum, entry) => sum + bytes(entry.source), 0),
}, null, 2));
