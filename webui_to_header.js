const fs = require('fs');
const path = require('path');

const distDir = path.join(__dirname, 'webui', 'dist');
const outFile = path.join(__dirname, 'src', 'html_content.h');

/**
 * @param {string} content
 * @returns {string}
 */
function stripBlockComments(content) {
  if (!content) return content;
  return content.replace(/\/\*[\s\S]*?\*\//g, '');
}

async function buildHeader() {
  let indexHtml = fs.readFileSync(path.join(distDir, 'index.html'), 'utf8');


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
  const jsContent = stripBlockComments(
    fs.readFileSync(path.join(assetsDir, jsFile), 'utf8')
  );
  // Inline JS (only first module script tag). Using function form prevents special replacement patterns.
  indexHtml = indexHtml.replace(
    /<script[^>]*src="[^"]+"[^>]*><\/script>/,
    () => `<script type="module">${jsContent}</script>`
  );

  // load favicon svg from webui/logo.svg
  const logoPath = path.join(__dirname, 'webui', 'logo.svg');
  if (!fs.existsSync(logoPath)) {
    throw new Error('cannot find logo.svg! in webui directory');
  }
  const svgContent = fs.readFileSync(logoPath, 'utf8');
  const svgDataUrl = 'data:image/svg+xml;utf8,' + encodeURIComponent(svgContent);
  indexHtml = indexHtml.replace(
    /<link rel="icon"[^>]*href="[^"]+\.svg"[^>]*>/,
    () => `<link rel="icon" type="image/svg+xml" href="${svgDataUrl}" />`
  );

  // make the Header
  let header = `#pragma once\n#include <pgmspace.h>\n\n`;
  header += `const char WEB_HTML[] PROGMEM = R"rawliteral(\n${indexHtml}\n)rawliteral";\n`;
  header += `\nclass WebHTML {\npublic:\n    const char* getWebHTML();\n};\n`;

  fs.writeFileSync(outFile, header);
  console.log('Header generated:', outFile);
}

// async/await
buildHeader();
