import test from 'node:test';
import assert from 'node:assert/strict';
import { runtimeSources, runtimeLayoutGroups, createLatestRuntimeRequest } from '../src/runtimeDemand.mjs';
import { createJsonCache, readJson } from '../src/runtimeDataClient.mjs';

test('active cards form one exact case-sensitive union; hidden views select none', () => {
  const cards=[{runtimeSources:['A','B']},{runtimeSources:['B','a']},{visible:false,runtimeSources:['hidden']}];
  assert.deepEqual(runtimeSources(cards,true),['A','B','a']);
  assert.deepEqual(runtimeSources(cards,false),[]);
  assert.throws(()=>runtimeSources([{runtimeSources:Array.from({length:33},(_,i)=>String(i))}],true));
  assert.throws(()=>runtimeSources([{runtimeSources:['x'.repeat(65)]}],true));
});

test('successful metadata is shared, failure can retry', async () => {
  let calls=0, fail=true;
  const cache=createJsonCache(async()=>{calls++;if(fail) throw new Error('temporary');return [];});
  await assert.rejects(cache.load()); fail=false;
  const [a,b]=await Promise.all([cache.load(),cache.load()]);
  assert.equal(a,b);assert.deepEqual(await cache.load(),[]);assert.equal(calls,2);
});

test('timeout covers a body that stalls after headers, and aborts fetch', async () => {
  let signal;
  const fetcher=async (_url,options)=>{signal=options.signal;return {ok:true,json:()=>new Promise(()=>{})};};
  await assert.rejects(readJson('/test',{},10,fetcher),{name:'AbortError'});
  assert.equal(signal.aborted,true);
});

test('selection changes suppress old responses and serialize requests', async () => {
  const resolvers=[], calls=[], applied=[];
  const client=createLatestRuntimeRequest({request:(groups)=>{calls.push(groups);return new Promise(resolve=>resolvers.push(resolve));},onData:data=>applied.push(data)});
  const first=client.select(['A']);
  client.select(['B']);client.run();
  assert.equal(calls.length,1);
  resolvers[0]('old');await first;await new Promise(resolve=>setImmediate(resolve));
  assert.equal(calls.length,2);assert.deepEqual(applied,[]);
  resolvers[1]('new');await new Promise(resolve=>setImmediate(resolve));
  assert.deepEqual(applied,['new']);
  client.cancel();await client.run();assert.equal(calls.length,2);
});

test('invalid JSON is propagated without exposing response content', async () => {
  await assert.rejects(readJson('/config',{},100,async()=>({ok:true,json:async()=>{throw new SyntaxError('invalid JSON');}})),SyntaxError);
});

test('metadata defines page structure before any provider values arrive', () => {
  const groups=[{name:'A',fields:[{key:'value'}]},{name:'B',fields:[{key:'other'}]}];
  assert.deepEqual(runtimeLayoutGroups(groups,true,()=>false),groups);
  assert.deepEqual(runtimeLayoutGroups(groups,false,()=>false),[]);
});
