const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

// This script lives in tools/, so derive project root one level up
const rootDir = path.join(__dirname, '..');
const distDir = path.join(rootDir, 'webui', 'dist');
const outFile = path.join(rootDir, 'src', 'html_content.h');

/**
 * @param {string} content
 * @returns {string}
 */
function stripBlockComments(content) {
  if (!content) return content;
  return content.replace(/\/\*[\s\S]*?\*\//g, '');
}

async function buildHeader() {
  const htmlPath = path.join(distDir, 'index.html');
  const htmlGzPath = path.join(distDir, 'index.html.gz');
  let indexHtml;
  if (fs.existsSync(htmlPath)) {
    indexHtml = fs.readFileSync(htmlPath, 'utf8');
  } else if (fs.existsSync(htmlGzPath)) {
    const gz = fs.readFileSync(htmlGzPath);
    indexHtml = zlib.gunzipSync(gz).toString('utf8');
  } else {
    throw new Error('cannot find dist/index.html or dist/index.html.gz');
  }


  // add css
  const assetsDir = path.join(distDir, 'assets');
  const cssFile = fs.readdirSync(assetsDir).find(f => f.endsWith('.css'));
  if (!cssFile) {
    throw new Error('cannot find CSS file in assets directory!');
  }
  const cssContent = stripBlockComments(
    fs.readFileSync(path.join(assetsDir, cssFile), 'utf8')
  );
  // Inline CSS (only first stylesheet link). Use a replacer function to avoid accidental $-group substitutions.
  indexHtml = indexHtml.replace(
    /<link rel="stylesheet"[^>]*href="[^"]+"[^>]*>/,
    () => `<style>${cssContent}</style>`
  );

  // add js
  const jsFile = fs.readdirSync(assetsDir).find(f => f.endsWith('.js'));
  if (!jsFile) {
    throw new Error('cannot find JS file in assets directory!');
  }
  let jsContent = stripBlockComments(
    fs.readFileSync(path.join(assetsDir, jsFile), 'utf8')
  );
  
  // Replace __ENCRYPTION_SALT__ placeholder with actual salt from environment
  const encryptionSalt = process.env.CM_ENCRYPTION_SALT || process.env.ENCRYPTION_SALT || '__ENCRYPTION_SALT__';
  if (encryptionSalt && encryptionSalt !== '__ENCRYPTION_SALT__') {
    // Escape special regex characters in the salt for safe replacement
    const saltPattern = /__ENCRYPTION_SALT__/g;
    jsContent = jsContent.replace(saltPattern, encryptionSalt);
    console.log(`[webui_to_header] Injected encryption salt (length: ${encryptionSalt.length})`);
  } else {
    console.log('[webui_to_header] No encryption salt provided - passwords will be transmitted in plaintext.');
    console.log('[webui_to_header] To enable encryption, create src/salt.h in your project with:');
    console.log('[webui_to_header]   #define ENCRYPTION_SALT "your-unique-salt"');
  }
  
  // Inline JS (only first module script tag). Using function form prevents special replacement patterns.
  indexHtml = indexHtml.replace(
    /<script[^>]*src="[^"]+"[^>]*><\/script>/,
    () => `<script type="module">${jsContent}</script>`
  );

  // load favicon svg from webui/logo.svg
  const logoPath = path.join(rootDir, 'webui', 'logo.svg');
  if (!fs.existsSync(logoPath)) {
    throw new Error('cannot find logo.svg! in webui directory');
  }
  const svgContent = fs.readFileSync(logoPath, 'utf8');
  const svgDataUrl = 'data:image/svg+xml;utf8,' + encodeURIComponent(svgContent);
  indexHtml = indexHtml.replace(
    /<link rel="icon"[^>]*href="[^"]+\.svg"[^>]*>/,
    () => `<link rel="icon" type="image/svg+xml" href="${svgDataUrl}" />`
  );

  // Compress fully inlined HTML to gzip to save flash when embedding
  const gz = zlib.gzipSync(Buffer.from(indexHtml, 'utf8'), { level: 9 });

  // Format as C array (hex) for PROGMEM
  /**
   * @param {Buffer} buf
   * @returns {string}
   */
  function toCArray(buf) {
    const parts = [];
    for (let i = 0; i < buf.length; i++) {
      const b = buf[i];
      parts.push('0x' + b.toString(16).padStart(2, '0'));
    }
    // Wrap to reasonable line length
    const lines = [];
    const perLine = 24;
    for (let i = 0; i < parts.length; i += perLine) {
      lines.push(parts.slice(i, i + perLine).join(', '));
    }
    return lines.join(',\n    ');
  }

  const gzArray = toCArray(gz);

  // make the Header (gzipped content)
  let header = `#pragma once\n#include <pgmspace.h>\n\n`;
  header += `// Gzipped embedded Web UI (index.html with inlined CSS/JS)\n`;
  header += `const uint8_t WEB_HTML_GZ[] PROGMEM = {\n    ${gzArray}\n};\n`;
  header += `const size_t WEB_HTML_GZ_LEN = ${gz.length};\n`;
  header += `\nclass WebHTML {\npublic:\n    const uint8_t* getWebHTMLGz();\n    size_t getWebHTMLGzLen();\n};\n`;

  fs.writeFileSync(outFile, header);
  console.log('Header generated:', outFile);
}

// async/await
buildHeader();
