// WebUI header generator:
// - Reads webui/dist/index.html (already built by the precompile step)
// - Inlines linked CSS/JS assets so the HTML is self-contained
// - Gzips the inlined HTML for minimal footprint
// - Emits a header defining WEB_HTML_GZ, WEB_HTML_GZ_LEN and the WebHTML class
//   expected by ESP32 Configuration Manager (html_content.cpp references these)

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

function findLibDir() {
  const env = process.env.PIOENV || process.env.PLATFORMIO_ENV || 'usb';
  const root = process.cwd();
  // Case 1: Running from project root -> lib is under .pio/libdeps/<env>/...
  let candidate = path.join(root, '.pio', 'libdeps', env, 'ESP32 Configuration Manager');
  if (fs.existsSync(path.join(candidate, 'webui'))) {
    return candidate;
  }
  // Case 2: Running from library directory already
  if (fs.existsSync(path.join(root, 'webui')) && fs.existsSync(path.join(root, 'src'))) {
    return root;
  }
  throw new Error(`Library dir not found (cwd=${root})`);
}

function inlineAssets(html, distDir) {
  // Remove modulepreload links to avoid fetch attempts for removed assets
  html = html.replace(/<link[^>]*rel=["']modulepreload["'][^>]*>\s*/gi, '');

  // Inline CSS links
  html = html.replace(/<link[^>]*rel="stylesheet"[^>]*href=["']([^"']+)["'][^>]*>\s*/gi, (m, href) => {
    const rel = href.replace(/^\/?/, '');
    const cssPath = path.join(distDir, rel);
    if (!fs.existsSync(cssPath)) return m;
    const css = fs.readFileSync(cssPath, 'utf8');
    return `<style>${css}</style>`;
  });

  // Inline script modules (preserve type="module" when present)
  html = html.replace(/<script([^>]*)src=["']([^"']+)["']([^>]*)><\/script>/gi, (m, preAttrs, href, postAttrs) => {
    const rel = href.replace(/^\/?/, '');
    const jsPath = path.join(distDir, rel);
    if (!fs.existsSync(jsPath)) return m;
    const js = fs.readFileSync(jsPath, 'utf8');
    const attrs = `${preAttrs || ''} ${postAttrs || ''}`;
    const isModule = /type\s*=\s*["']module["']/i.test(attrs);
    const typeAttr = isModule ? ' type="module"' : '';
    return `<script${typeAttr}>${js}</script>`;
  });

  return html;
}

function generate() {
  const libDir = findLibDir();
  const distDir = path.join(libDir, 'webui', 'dist');
  const indexHtml = path.join(distDir, 'index.html');
  if (!fs.existsSync(indexHtml)) {
    throw new Error(`dist/index.html not found at ${indexHtml}`);
  }
  let html = fs.readFileSync(indexHtml, 'utf8');
  html = inlineAssets(html, distDir);
  // Gzip the fully inlined HTML
  const gz = zlib.gzipSync(Buffer.from(html, 'utf8'), { level: 9 });

  // Emit C array initializer for gzipped bytes
  const bytesPerLine = 16;
  let byteLines = [];
  for (let i = 0; i < gz.length; i += bytesPerLine) {
    const slice = gz.slice(i, i + bytesPerLine);
    const line = Array.from(slice).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ');
    byteLines.push('    ' + line);
  }

  const header = [
    '#pragma once',
    '#include <pgmspace.h>',
    '',
    '// Auto-generated (gzipped, inlined) from webui/dist by tools/webui_to_header.js',
    'const uint8_t WEB_HTML_GZ[] PROGMEM = {',
    byteLines.join(',\n'),
    '};',
    `const size_t WEB_HTML_GZ_LEN = ${gz.length};`,
    '',
    'class WebHTML {',
    'public:',
    '    const uint8_t* getWebHTMLGz();',
    '    size_t getWebHTMLGzLen();',
    '};',
    ''
  ].join('\n');

  const out = path.join(libDir, 'src', 'html_content.h');
  fs.writeFileSync(out, header, 'utf8');
  console.log(`Header generated: ${out}`);
}

try {
  generate();
} catch (e) {
  console.error(e);
  process.exit(1);
}
