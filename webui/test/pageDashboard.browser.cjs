// Run against a local Vite dev server; all device endpoints and application sockets are mocked.
// PLAYWRIGHT_MODULE and PLAYWRIGHT_BROWSER_PATH may select an existing local runtime.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const testUrl=process.env.CM_WEBUI_TEST_URL || 'http://127.0.0.1:5179';
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.PLAYWRIGHT_BROWSER_PATH || undefined,headless:true});
 const page=await browser.newPage();const errors=[],requests=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
  const NativeSocket=window.WebSocket;window.__subscriptions=[];window.__sockets=[];
  class Socket {
   static CONNECTING=0;static OPEN=1;static CLOSED=3;
   constructor(url,protocol){if(protocol==='vite-hmr')return new NativeSocket(url,protocol);this.url=url;this.readyState=0;window.__sockets.push(this);setTimeout(()=>{this.readyState=1;this.onopen?.({});},10);}
   send(text){if(text==='__pong')return;const frame=JSON.parse(text);window.__subscriptions.push(frame.groups);const payload={uptime:123,system:{otaActive:false,otaHasPassword:true},_runtimeGroups:frame.groups};if(frame.groups.includes('bms'))payload.bms={reason:'adapter_missing',availability:'offline'};if(frame.groups.includes('victron'))payload.victron={reason:'waiting_for_advertisement',availability:'online'};setTimeout(()=>this.onmessage?.({data:JSON.stringify(payload)}),5);}
   close(){this.readyState=3;this.onclose?.({});}
  }
  window.WebSocket=Socket;
  const originalFetch=window.fetch;window.__retryMetadataCalls=0;
  window.fetch=(resource,options)=>{
   if(String(resource).includes('/runtime_meta.json?page=retry') && ++window.__retryMetadataCalls===1) return Promise.resolve({ok:true,json:()=>new Promise(()=>{})});
   return originalFetch(resource,options);
  };
 });
 const entries=[{key:'bms',name:'BMS',title:'BMS',order:1},{key:'victron page!',name:'Victron',title:'Victron',order:2},{key:'empty',name:'Empty',title:'Empty',order:3},{key:'slow',name:'Slow',title:'Slow',order:4},{key:'retry',name:'Retry',title:'Retry',order:5}];
 await page.route(`${testUrl}/**`,async route=>{
  const u=new URL(route.request().url());
  if(u.pathname==='/'||u.pathname.startsWith('/src/')||u.pathname.startsWith('/node_modules/')||u.pathname.startsWith('/@')||u.pathname.endsWith('.svg'))return route.continue();
  requests.push(u.pathname+u.search);
  const json=value=>route.fulfill({contentType:'application/json',body:JSON.stringify(value)});
  if(u.pathname==='/live_pages.json')return json({pageScoped:true,hasLiveContent:true,pages:entries,defaultPage:'bms'});
  const key=u.searchParams.get('page');
  if(u.pathname==='/live_layout.json'){
   assert.ok(key,'full layout forbidden');const entry=entries.find(x=>x.key===key);assert.ok(entry);
   if(key==='slow')await new Promise(r=>setTimeout(r,300));
   try{return await json({pages:[{...entry,cards:key==='empty'?[]:[{name:'Status',title:'Status',items:['reason','availability'],groups:[]}]}],placements:{},defaultPage:entry.name});}catch{return;}
  }
  if(u.pathname==='/runtime_meta.json'){
   assert.ok(key,'full metadata forbidden');if(key==='empty')return json([]);
   const source=key==='bms'?'bms':'victron';return json(['reason','availability'].map((field,i)=>({key:field,label:field==='reason'?'Source reason':'Availability',group:'Status',sourceGroup:source,page:entries.find(x=>x.key===key).name,card:'Status',isString:true,order:i})));
  }
  if(u.pathname==='/runtime.json'){
   const groups=JSON.parse(u.searchParams.get('groups')||'null');assert.ok(groups,'full runtime forbidden');
   const data={uptime:123,system:{otaActive:false,otaHasPassword:true}};if(groups.includes('bms'))data.bms={reason:'adapter_missing',availability:'offline'};if(groups.includes('victron'))data.victron={reason:'waiting_for_advertisement',availability:'online'};return json(data);
  }
  if(u.pathname==='/config/auth')return json({status:'ok',token:'mock-token',ttlSec:3600});
  if(u.pathname==='/config.json')return json({config:{System:{Version:{value:'mock',displayName:'Version',showIf:true}}}});
  if(u.pathname==='/user_theme.css')return route.fulfill({contentType:'text/css',body:''});
  if(u.pathname==='/ota_update')return route.fulfill({status:403,body:''});
  return json({});
 });
 await page.goto(`${testUrl}/`);
 await page.getByText('adapter_missing',{exact:true}).waitFor();
 assert.equal(requests.filter(x=>x==='/config.json').length,0);
 for(const entry of entries)await page.getByRole('button',{name:entry.title,exact:true}).waitFor();
 await page.getByRole('button',{name:'Victron',exact:true}).click();
 await page.getByText('waiting_for_advertisement',{exact:true}).waitFor();
 assert.equal(await page.getByText('adapter_missing',{exact:true}).count(),0);
 await page.getByRole('button',{name:'Settings',exact:true}).click();
 await page.waitForFunction(()=>JSON.stringify(window.__subscriptions.at(-1))==='[]');
 await page.getByRole('button',{name:'Live',exact:true}).click();
 await page.getByText('waiting_for_advertisement',{exact:true}).waitFor();
 assert.equal(await page.getByText('adapter_missing',{exact:true}).count(),0);
 await page.getByRole('button',{name:'Empty',exact:true}).click();
 await page.waitForFunction(()=>JSON.stringify(window.__subscriptions.at(-1))==='[]');
 for(const entry of entries)await page.getByRole('button',{name:entry.title,exact:true}).waitFor();
 await page.getByRole('button',{name:'Slow',exact:true}).click();
 await page.waitForTimeout(50);
 await page.getByRole('button',{name:'BMS',exact:true}).click();
 await page.getByText('adapter_missing',{exact:true}).waitFor();
 await page.waitForTimeout(400);
 assert.equal(await page.getByText('waiting_for_advertisement',{exact:true}).count(),0);
 await page.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'));});
 await page.waitForFunction(()=>JSON.stringify(window.__subscriptions.at(-1))==='[]');
 await page.evaluate(()=>window.__sockets.find(s=>s.url.includes('runtime=selected')).onmessage({data:JSON.stringify({_runtimeGroups:[],system:{otaActive:true,otaHasPassword:true}})}));
 await page.waitForFunction(()=>Array.from(document.querySelectorAll('button')).find(b=>b.textContent.trim()==='Settings').disabled);
 await page.evaluate(()=>window.__sockets.find(s=>s.url.includes('runtime=selected')).onmessage({data:JSON.stringify({_runtimeGroups:[],system:{otaActive:false,otaHasPassword:true}})}));
 await page.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'));});
 await page.waitForFunction(()=>JSON.stringify(window.__subscriptions.at(-1))==='["bms"]');
 await page.getByRole('button',{name:'Retry',exact:true}).click();
 await page.getByText('waiting_for_advertisement',{exact:true}).waitFor({timeout:16000});
 assert.equal(await page.evaluate(()=>window.__retryMetadataCalls),2,'active body timeout must retry');
 assert.equal(requests.filter(x=>x==='/live_pages.json').length,1);
 assert.equal(requests.filter(x=>x==='/live_layout.json?page=bms').length,1);
 assert.equal(requests.filter(x=>x==='/runtime_meta.json?page=bms').length,1);
 assert.equal(requests.filter(x=>x==='/runtime_meta.json?page=victron%20page!').length,1);
 assert.ok(!(await page.evaluate(()=>window.__subscriptions)).some(x=>x.includes('bms')&&x.includes('victron')));
 assert.deepEqual(errors,[]);
 console.log(JSON.stringify({result:'PASS scoped mounted Vue: exact keys, sequential caches, source collision, empty tabs, stale abort, settings/visibility, OTA safety, active timeout retry',requests,subscriptions:await page.evaluate(()=>window.__subscriptions)},null,2));
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
