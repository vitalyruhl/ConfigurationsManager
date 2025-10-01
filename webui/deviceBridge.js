// Smart mock bridge: pulls live data from device if reachable; falls back to cached snapshot.
// Usage: set env CM_DEVICE=http://192.168.2.126 (no trailing slash) or defaults to http://192.168.2.126
// Provides minimal endpoints similar to json-server for /runtime.json, /runtime_meta.json, /config.json
// and keeps an on-disk snapshot (db.live.json) updated every POLL_INTERVAL when reachable.

import fs from 'fs';
import { readFile, writeFile } from 'fs/promises';
import http from 'http';
import https from 'https';
import url from 'url';

const DEVICE = process.env.CM_DEVICE || 'http://192.168.2.126';
const STARTUP_TIMEOUT_MS = parseInt(process.env.CM_STARTUP_TIMEOUT || '10000',10); // wait up to 10s for first fetch
const POLL_INTERVAL_MS = parseInt(process.env.CM_POLL_INTERVAL || '30000',10); // 30s poll
const SNAPSHOT_FILE = 'db.live.json';
const FALLBACK_DB = 'db.json'; // existing static mock fallback

let cache = { config:null, runtime:null, runtime_meta:null, version:null, userCss:null, ts:0 };
let reachable = false;

/**
 * Fetch JSON helper with cache buster.
 * @param {string} path
 * @returns {Promise<any>}
 */
async function fetchJson(path){
  const target = DEVICE + path + (path.includes('?')?'':'?') + 't=' + Date.now();
  return new Promise((resolve,reject)=>{
    const mod = target.startsWith('https') ? https : http;
    const req = mod.get(target, res => {
      if(res.statusCode !== 200){ reject(new Error(path+': status '+res.statusCode)); return; }
      let data='';
      res.on('data', c=>data+=c);
      res.on('end', ()=>{
        try { resolve(JSON.parse(data)); } catch(e){ reject(e); }
      });
    });
    req.on('error', reject);
    req.setTimeout(5000, ()=>{ req.destroy(new Error('timeout')); });
  });
}

/**
 * Fetch plain text helper.
 * @param {string} path
 */
async function fetchText(path){
  const target = DEVICE + path + (path.includes('?')?'':'?') + 't=' + Date.now();
  return new Promise((resolve,reject)=>{
    const mod = target.startsWith('https') ? https : http;
    const req = mod.get(target, res => {
      if(res.statusCode !== 200){ reject(new Error(path+': status '+res.statusCode)); return; }
      let data='';
      res.on('data', c=>data+=c);
      res.on('end', ()=> resolve(data));
    });
    req.on('error', reject);
    req.setTimeout(5000, ()=>{ req.destroy(new Error('timeout')); });
  });
}

async function initialAttempt(){
  const deadline = Date.now() + STARTUP_TIMEOUT_MS;
  while(Date.now() < deadline){
    try {
      const [cfg, rt, meta, ver, css] = await Promise.all([
        fetchJson('/config.json'),
        fetchJson('/runtime.json'),
        fetchJson('/runtime_meta.json'),
        fetchText('/version').catch(()=>null),
        fetchText('/user_theme.css').catch(()=>null)
      ]);
      cache.config = cfg.config || cfg;
      cache.runtime = rt;
      cache.runtime_meta = meta;
  cache.version = (ver||'').trim() || cache.version || 'unknown';
  cache.userCss = css || cache.userCss;
      cache.ts = Date.now();
      reachable = true;
      await persist();
      console.log('[bridge] initial live snapshot acquired');
      return;
    } catch(e){
      await new Promise(r=>setTimeout(r,800));
    }
  }
  // fallback to existing snapshot on disk
  try {
    const txt = await readFile(SNAPSHOT_FILE,'utf-8');
    const snap = JSON.parse(txt);
    cache = snap; reachable = false;
    console.log('[bridge] using stored snapshot (device unreachable).');
  } catch(e){
    const base = JSON.parse(await readFile(FALLBACK_DB,'utf-8'));
  cache.config = base.config; cache.runtime = base.runtime; cache.runtime_meta = base.runtime_meta; cache.version = base.version || 'mock'; cache.userCss = base.userCss || null; cache.ts = Date.now();
    console.log('[bridge] fallback to bundled db.json content.');
  }
}

async function pollLoop(){
  if(!reachable){ // try promote to live if previously unreachable
    try {
      const rt = await fetchJson('/runtime.json');
      cache.runtime = rt; cache.ts = Date.now(); reachable = true; console.log('[bridge] device became reachable (runtime only).');
      // try grab meta occasionally when missing
      if(!cache.runtime_meta){ cache.runtime_meta = await fetchJson('/runtime_meta.json'); }
      if(!cache.config){ const cfg = await fetchJson('/config.json'); cache.config = cfg.config || cfg; }
  if(!cache.version){ cache.version = await fetchText('/version').catch(()=>cache.version || 'unknown'); }
  if(!cache.userCss){ cache.userCss = await fetchText('/user_theme.css').catch(()=>cache.userCss); }
      await persist();
    } catch(e){ /* still unreachable */ }
  } else {
    try {
      const rt = await fetchJson('/runtime.json');
      cache.runtime = rt; cache.ts = Date.now();
      // periodic version refresh every ~10 polls
      if(Math.floor(Date.now()/POLL_INTERVAL_MS) % 10 === 0){ // periodic slow refreshes
        fetchText('/version').then(v=>{ if(v) { cache.version = v.trim(); persist(); } }).catch(()=>{});
        fetchText('/user_theme.css').then(c=>{ if(c && c.length>5) { cache.userCss = c; persist(); } }).catch(()=>{});
      }
      await persist();
    } catch(e){
      reachable = false;
      const msg = (e && typeof e === 'object' && 'message' in e) ? /** @type {any} */(e).message : String(e);
      console.warn('[bridge] lost connection to device:', msg);
    }
  }
  setTimeout(pollLoop, POLL_INTERVAL_MS);
}

async function persist(){
  const out = JSON.stringify(cache, null, 2);
  await writeFile(SNAPSHOT_FILE, out, 'utf-8');
}

/**
 * Simple HTTP router
 * @param {import('http').IncomingMessage} req
 * @param {import('http').ServerResponse} res
 */
function serve(req,res){
  const parsed = url.parse(req.url || '/',true);
  /**
   * @param {number} code
   * @param {any} obj
   */
  const send = (code,obj)=>{ const body = JSON.stringify(obj); res.writeHead(code,{ 'Content-Type':'application/json','Cache-Control':'no-store' }); res.end(body); };
  if(parsed.pathname === '/runtime.json') return send(200, cache.runtime || {});
  if(parsed.pathname === '/runtime_meta.json') return send(200, cache.runtime_meta || []);
  if(parsed.pathname === '/config.json') return send(200, { config: cache.config || {} });
  if(parsed.pathname === '/bridge/status') return send(200, { reachable, lastUpdate: cache.ts, device: DEVICE, version: cache.version, hasCss: !!cache.userCss });
  if(parsed.pathname === '/version'){ res.writeHead(200,{ 'Content-Type':'text/plain','Cache-Control':'no-store' }); return res.end(cache.version || ''); }
  if(parsed.pathname === '/user_theme.css') { res.writeHead(200,{ 'Content-Type':'text/css','Cache-Control':'no-store' }); return res.end(cache.userCss || ''); }
  res.writeHead(404); res.end('not found');
}

(async()=>{
  await initialAttempt();
  pollLoop();
  const port = parseInt(process.env.CM_BRIDGE_PORT || '33000',10);
  http.createServer(serve).listen(port, ()=>{
    console.log(`[bridge] listening on :${port} (reachable=${reachable})`);
  });
})();
