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
  const NativeSocket=window.WebSocket;window.__subscriptions=[];window.__sockets=[];window.__startupEvents=[];window.__startupSafety=[];
  class Socket {
   static CONNECTING=0;static OPEN=1;static CLOSED=3;
   constructor(url,protocol){if(protocol==='vite-hmr')return new NativeSocket(url,protocol);this.url=url;this.readyState=0;window.__sockets.push(this);window.__startupEvents.push("socket:"+url.split("runtime=")[1]);window.__startupSafety.push({url,settingsDisabled:Array.from(document.querySelectorAll("button")).find(button=>button.textContent.trim()==="Settings")?.disabled,otaOverlay:!!document.querySelector(".ota-active-overlay")});setTimeout(()=>{this.readyState=1;this.onopen?.({});window.__startupEvents.push("socket-open:"+url.split("runtime=")[1]);},10);}
   send(text){if(text==='__pong')return;const frame=JSON.parse(text);window.__subscriptions.push(frame.groups);const payload={uptime:123,system:{otaActive:false,otaHasPassword:true},_runtimeGroups:frame.groups};if(frame.groups.includes('bms'))payload.bms={reason:'adapter_missing',availability:'offline'};if(frame.groups.includes('victron'))payload.victron={reason:'waiting_for_advertisement',availability:'online'};setTimeout(()=>this.onmessage?.({data:JSON.stringify(payload)}),5);}
   close(){this.readyState=3;this.onclose?.({});}
  }
  window.WebSocket=Socket;
  const originalFetch=window.fetch;window.__retryMetadataCalls=0;window.__runtimeSafetyReads=0;
  window.fetch=(resource,options)=>{
   if(String(resource).includes('/runtime_meta.json?page=retry')) {
    window.__retryMetadataCalls++;
    if(window.__retryMetadataCalls===1) return Promise.resolve({ok:true,json:()=>new Promise(()=>{})});
    if(window.__retryMetadataCalls===2) {
     window.__retrySafetyBefore=window.__runtimeSafetyReads;
     return Promise.resolve({ok:false,status:503});
    }
   }
   if(String(resource).startsWith('/ui_capabilities'))window.__runtimeSafetyReads++;
   const path=new URL(String(resource),location.href).pathname;
   window.__startupEvents.push(path+":start");
   return originalFetch(resource,options).then(response=>{
    for(const kind of ["json","text"]){const read=response[kind].bind(response);response[kind]=async()=>{
     // Headers alone are insufficient: each startup stage must await its body.
     if(path==='/user_theme.css') await new Promise(resolve=>{window.__releaseInitialTheme=resolve;});
     await new Promise(resolve=>setTimeout(resolve,60));
     const value=await read();window.__startupEvents.push(path+":end");return value;
    };}
    return response;
   });
  };
 });
 let firstRuntime=true, otaBusy=true;
 const entries=[{key:'bms',name:'BMS',title:'BMS',order:1},{key:'victron page!',name:'Victron',title:'Victron',order:2},{key:'empty',name:'Empty',title:'Empty',order:3},{key:'slow',name:'Slow',title:'Slow',order:4},{key:'retry',name:'Retry',title:'Retry',order:5}];
 await page.route(`${testUrl}/**`,async route=>{
  const u=new URL(route.request().url());
  if(u.pathname==='/'||u.pathname.startsWith('/src/')||u.pathname.startsWith('/node_modules/')||u.pathname.startsWith('/@')||u.pathname.endsWith('.svg'))return route.continue();
  requests.push(u.pathname+u.search);
  const json=value=>route.fulfill({contentType:'application/json',body:JSON.stringify(value)});
  if(u.pathname==='/ui_capabilities')return json({live:true,settings:true,otaEnabled:false,otaActive:otaBusy,otaHasPassword:true,settingsAuthRequired:false});
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
   const data={uptime:123,system:{otaActive:firstRuntime,otaHasPassword:true}};firstRuntime=false;if(groups.includes('bms'))data.bms={reason:'adapter_missing',availability:'offline'};if(groups.includes('victron'))data.victron={reason:'waiting_for_advertisement',availability:'online'};return json(data);
  }
  if(u.pathname==='/config/auth')return json({status:'ok',token:'mock-token',ttlSec:3600});
  if(u.pathname==='/config.json')return json({config:{System:{Version:{value:'mock',displayName:'Version',showIf:true}}}});
  if(u.pathname==='/appinfo')return json({appName:'Test',appTitle:'Test',version:'1.0.0',guiLogging:false});
  if(u.pathname==='/user_theme.css')return route.fulfill({contentType:'text/css',body:''});
  if(u.pathname==='/ota_update')return route.fulfill({status:403,body:''});
  return json({});
 });
 await page.goto(`${testUrl}/`);
 await page.waitForFunction(()=>typeof window.__releaseInitialTheme==='function');
 assert.equal(await page.getByRole('button',{name:'Settings',exact:true}).isDisabled(),true,'Settings must wait for bounded bootstrap/OTA status');
 assert.equal(await page.getByRole('button',{name:'Flash',exact:true}).isDisabled(),true);
 await page.evaluate(()=>window.__releaseInitialTheme());
 await page.getByText('adapter_missing',{exact:true}).waitFor();
 await page.waitForFunction(()=>window.__sockets.some(socket=>socket.url.includes('runtime=none')));
 const startup=await page.evaluate(()=>window.__startupEvents);
 const before=(first,last)=>assert.ok(startup.indexOf(first)>=0 && startup.indexOf(first)<startup.indexOf(last), `${first} must finish before ${last}: ${JSON.stringify(startup)}`);
 before('/user_theme.css:end','/appinfo:start');
 before('/appinfo:end','/live_pages.json:start');
 before('/live_pages.json:end','/live_layout.json:start');
 before('/live_layout.json:end','/runtime_meta.json:start');
 before('/ui_capabilities:end','/live_pages.json:start');
 before('/runtime_meta.json:end','/runtime.json:start');
 before('/runtime.json:end','socket:selected');
 before('/ui_capabilities:end','socket:none');
 const startupSafety=await page.evaluate(()=>window.__startupSafety.find(item=>item.url.includes('runtime=selected')));
 assert.equal(startupSafety.settingsDisabled,true,'initial OTA-active snapshot must guard Settings before sockets');
 assert.equal(startupSafety.otaOverlay,true);
 otaBusy=false;
 await page.evaluate(()=>window.__sockets.find(s=>s.url.includes('runtime=none')).onmessage({data:JSON.stringify({type:'otaStatus',otaActive:false,otaEnabled:false,otaHasPassword:true})}));
 await page.waitForFunction(()=>!Array.from(document.querySelectorAll('button')).find(b=>b.textContent.trim()==='Settings').disabled);
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
 otaBusy=true;
 await page.evaluate(()=>window.__sockets.find(s=>s.url.includes('runtime=none')).onmessage({data:JSON.stringify({type:'otaStatus',otaActive:true,otaEnabled:false,otaHasPassword:true})}));
 await page.waitForFunction(()=>Array.from(document.querySelectorAll('button')).find(b=>b.textContent.trim()==='Settings').disabled);
 otaBusy=false;
 await page.evaluate(()=>window.__sockets.find(s=>s.url.includes('runtime=none')).onmessage({data:JSON.stringify({type:'otaStatus',otaActive:false,otaEnabled:false,otaHasPassword:true})}));
 await page.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'));});
 await page.waitForFunction(()=>JSON.stringify(window.__subscriptions.at(-1))==='["bms"]');
 await page.getByRole('button',{name:'Retry',exact:true}).click();
 await page.waitForFunction(()=>window.__retryMetadataCalls>=2,{},{timeout:16000});
 await page.waitForFunction(()=>window.__runtimeSafetyReads>window.__retrySafetyBefore,{},{timeout:3000});
 await page.getByText('waiting_for_advertisement',{exact:true}).waitFor({timeout:10000});
 assert.equal(await page.evaluate(()=>window.__retryMetadataCalls),3,'body timeout and repeated unavailable schema must retry while safety remains active');
 assert.equal(requests.filter(x=>x==='/live_pages.json').length,1);
 assert.equal(requests.filter(x=>x==='/live_layout.json?page=bms').length,1);
 assert.equal(requests.filter(x=>x==='/runtime_meta.json?page=bms').length,1);
 assert.equal(requests.filter(x=>x==='/runtime_meta.json?page=victron%20page!').length,1);
 assert.ok(!(await page.evaluate(()=>window.__subscriptions)).some(x=>x.includes('bms')&&x.includes('victron')));
 assert.deepEqual(errors,[]);
 console.log(JSON.stringify({result:'PASS scoped mounted Vue: exact keys, sequential caches, source collision, empty tabs, stale abort, settings/visibility, OTA safety, active timeout retry',requests,subscriptions:await page.evaluate(()=>window.__subscriptions)},null,2));
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
