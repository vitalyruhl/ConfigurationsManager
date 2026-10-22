import test from 'node:test';
import assert from 'node:assert/strict';
import { createPageDataClient } from '../src/runtimeDataClient.mjs';

const layout = key => ({ pages: [{ key, cards: [] }], placements: {}, defaultPage: key });
test('page data loads layout then metadata, shares a request and caches successful pages', async () => {
  const urls = [];
  const client = createPageDataClient(async url => {
    urls.push(url);
    const key = new URL(url, 'http://cm.test').searchParams.get('page');
    return url.startsWith('/live_layout') ? layout(key) : [{ key: 'value', sourceGroup: key }];
  });
  const [a,b] = await Promise.all([client.load('My page'),client.load('My page')]);
  assert.equal(a,b);
  assert.deepEqual(urls, ['/live_layout.json?page=My%20page','/runtime_meta.json?page=My%20page']);
  await client.load('next');
  await client.load('My page');
  assert.equal(urls.length,4);
});
test('page failures are evicted and a queued cancellation never fetches metadata', async () => {
  let calls=0;
  const client=createPageDataClient(async url => {
    if(++calls===1) throw new Error('temporary');
    return url.startsWith('/live_layout') ? layout('a') : [];
  });
  await assert.rejects(client.load('a'));
  assert.deepEqual((await client.load('a')).metadata,[]);
  assert.equal(calls,3);
  const controller=new AbortController();
  const urls=[];
  const aborted=createPageDataClient(async url => {
    urls.push(url); controller.abort(); return layout('a');
  });
  await assert.rejects(aborted.load('a',controller.signal), { name:'AbortError' });
  assert.deepEqual(urls,['/live_layout.json?page=a']);
});
test('different pages serialize requests and malformed scoped data is not cached', async () => {
  let release;
  const gate=new Promise(resolve=>{release=resolve;});
  const urls=[];
  const client=createPageDataClient(async url=>{
    urls.push(url);
    const key=new URL(url,'http://cm.test').searchParams.get('page');
    if(url.startsWith('/live_layout') && key==='a') await gate;
    return url.startsWith('/live_layout') ? layout(key) : [];
  });
  const a=client.load('a'); const b=client.load('b');
  await new Promise(resolve=>setImmediate(resolve));
  assert.deepEqual(urls,['/live_layout.json?page=a']);
  release(); await Promise.all([a,b]);
  assert.deepEqual(urls,['/live_layout.json?page=a','/runtime_meta.json?page=a','/live_layout.json?page=b','/runtime_meta.json?page=b']);
  let attempts=0;
  const malformed=createPageDataClient(async ()=>{++attempts; return { pages:[{key:'wrong'}] };});
  await assert.rejects(malformed.load('a'));
  await assert.rejects(malformed.load('a'));
  assert.equal(attempts,2);
});
