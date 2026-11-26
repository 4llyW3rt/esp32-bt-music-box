// web_ui.h - ESP32'nin sunduğu tek sayfalık arayüz (telefon tarayıcısında uygulama gibi çalışır)
#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html><html lang="tr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b0b14">
<title>Müzik Çalar</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0 auto;padding:14px;background:#0b0b14;color:#eee;font:15px system-ui,sans-serif;max-width:760px}
h1{font-size:20px;margin:6px 0 2px}
h2{font-size:15px;margin:0 0 10px;color:#9fb4ff}
.sub{color:#8a8aa5;font-size:13px}
.card{background:#161626;border-radius:14px;padding:14px;margin:12px 0}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
button{background:#2b2b4a;color:#fff;border:0;border-radius:10px;padding:10px 14px;font-size:15px}
button.p{background:#2ee6a6;color:#04221a;font-weight:600}
button.d{background:#5a2330}
button:disabled{opacity:.4}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.g{background:#0f0f1c;border:2px solid #2a2a44;border-radius:12px;padding:8px}
.g.a{border-color:#2ee6a6}
.g img{width:100%;border-radius:8px;background:#000;display:block}
.g b{display:block;margin:6px 0 2px;font-size:13px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.g small{color:#8a8aa5}
.bar{height:8px;background:#2a2a44;border-radius:6px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:#2ee6a6}
input[type=range]{width:100%}
#msg{min-height:22px;font-size:14px;color:#ffd166;padding:4px 2px}
.tag{font-size:11px;background:#2b2b4a;border-radius:6px;padding:2px 6px;margin-left:4px}
</style></head><body>
<h1>🎵 Müzik Çalar</h1>
<div class="sub" id="dev">Bağlanıyor…</div>

<div class="card"><h2>Şu an</h2>
 <div id="np" style="font-size:17px;font-weight:600">-</div>
 <div class="sub" id="st">-</div>
 <div class="row" style="margin:10px 0">
  <button onclick="act('/api/cmd?c=prev')">⏮</button>
  <button class="p" onclick="act('/api/cmd?c=playpause')">⏯</button>
  <button onclick="act('/api/cmd?c=next')">⏭</button>
  <button onclick="act('/api/cmd?c=mute')">🔇</button>
  <button onclick="act('/api/cmd?c=source')">BT / SD</button>
 </div>
 <div class="row"><span>Ses</span><input id="vol" type="range" min="0" max="30" style="flex:1" oninput="volIn(this.value)"><b id="vv">-</b></div>
 <div class="row" style="margin-top:8px"><button onclick="act('/api/savevolume')">Açılış sesi olarak kaydet</button></div>
</div>

<div class="card"><h2>GIF yükle</h2>
 <div class="sub">En fazla 240×268 piksel ve 900 KB (.gif)</div>
 <div class="row" style="margin:10px 0"><input type="file" id="f" accept=".gif,image/gif"><button class="p" id="ub" onclick="upload()">Yükle</button></div>
 <img id="pv" style="max-width:100%;display:none;border-radius:8px;background:#000">
 <div class="bar" style="margin-top:8px"><i id="pb"></i></div>
</div>

<div class="card"><h2>GIF'lerim</h2>
 <div class="grid" id="gl"></div>
 <div class="sub" id="fs" style="margin-top:10px"></div>
 <div class="row" style="margin-top:8px"><button class="d" onclick="factory()">Benim GIF'lerimi sil</button></div>
</div>
<div class="card"><h2>Cihaz</h2>
 <label class="row"><input type="checkbox" id="wk" onchange="act('/api/wifikeep?on='+(this.checked?1:0))"> Wi-Fi'&#305; hep a&#231;&#305;k tut (ses bozulursa kapat)</label>
 <div class="sub" id="rs" style="margin:8px 0"></div>
 <div class="row"><button onclick="loadLog()">Kayd&#305; yenile</button><button class="d" onclick="clearLog()">Kayd&#305; temizle</button></div>
 <pre id="lg" style="white-space:pre-wrap;font-size:12px;max-height:260px;overflow:auto;background:#0f0f1c;border-radius:8px;padding:8px;margin:8px 0 0"></pre>
</div>
<div id="msg"></div>

<script>
const $=id=>document.getElementById(id);
const msg=t=>{$('msg').textContent=t};
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const fmt=s=>Math.floor(s/60)+':'+String(s%60).padStart(2,'0');

async function api(path,opt){
  const r=await fetch(path,opt);
  const t=await r.text();
  let j={};try{j=JSON.parse(t)}catch(e){}
  if(!r.ok||j.ok===false)throw new Error(j.error||('Hata '+r.status));
  return j;
}
async function act(path){
  try{await api(path,{method:'POST'});msg('Tamam')}catch(e){msg(e.message)}
  if(/select|delete|factory/.test(path))gifs();
  state();
}
let vt;
function volIn(v){
  $('vv').textContent=v;
  clearTimeout(vt);
  vt=setTimeout(()=>act('/api/volume?v='+v),150);
}
async function state(){
  try{
    const s=await api('/api/state');
    $('dev').textContent=s.name+' · '+s.fw+' · boş bellek '+Math.round(s.heap/1024)+' KB';
    $('np').textContent=s.title||'-';
    $('st').textContent=s.source+' · '+(s.playing?'Çalıyor':'Durdu')+(s.muted?' · Sessiz':'')+' · '+fmt(s.elapsed)+(s.length?' / '+fmt(s.length):'');
    $('vol').max=s.max_volume||30;
    if(document.activeElement!==$('vol')){$('vol').value=s.volume;$('vv').textContent=s.volume}
    $('fs').textContent='Depolama: '+Math.round(s.fs_used/1024)+' / '+Math.round(s.fs_total/1024)+' KB';
    if(document.activeElement!==$('wk'))$('wk').checked=!!s.wifi_keep;
    $('rs').textContent='Son a\u00e7\u0131l\u0131\u015f nedeni: '+s.reset+' \u00b7 a\u00e7\u0131l\u0131\u015f say\u0131s\u0131: '+s.boots;
  }catch(e){$('dev').textContent='Bağlantı yok'}
}
async function gifs(){
  try{
    const l=await api('/api/gifs');
    $('gl').innerHTML=l.map(g=>
      '<div class="g'+(g.active?' a':'')+'"><img loading="lazy" src="/api/file?f='+encodeURIComponent(g.file)+'&s='+g.size+'">'
      +'<b>'+esc(g.name)+(g.builtin?'<span class="tag">sabit</span>':'')+'</b>'
      +'<small>'+Math.round(g.size/1024)+' KB</small>'
      +'<div class="row" style="margin-top:6px">'
      +(g.active?'<span class="tag">Aktif</span>':'<button class="p" onclick="sel(\''+g.file+'\')">Seç</button>')
      +(g.builtin?'':'<button class="d" onclick="del(\''+g.file+'\')">Sil</button>')
      +'</div></div>').join('');
  }catch(e){msg(e.message)}
}
function sel(f){act('/api/select?file='+encodeURIComponent(f))}
async function loadLog(){try{const r=await fetch('/api/log');$('lg').textContent=await r.text();$('lg').scrollTop=1e9}catch(e){msg('Kay\u0131t al\u0131namad\u0131')}}
function clearLog(){if(confirm('Kay\u0131t temizlensin mi?'))act('/api/logclear').then(loadLog)}
function del(f){if(confirm('Bu GIF silinsin mi?'))act('/api/delete?file='+encodeURIComponent(f))}
function factory(){if(confirm('Yüklediğin tüm GIF\'ler silinsin mi? (Sabit GIF\'ler kalır)'))act('/api/factory')}

async function check(f){
  if(f.size>900*1024)throw new Error('Dosya çok büyük ('+Math.round(f.size/1024)+' KB). En fazla 900 KB.');
  const b=new Uint8Array(await f.slice(0,10).arrayBuffer());
  const sig=String.fromCharCode(b[0],b[1],b[2],b[3],b[4],b[5]);
  if(sig!=='GIF87a'&&sig!=='GIF89a')throw new Error('Bu bir GIF dosyası değil.');
  const w=b[6]|(b[7]<<8),h=b[8]|(b[9]<<8);
  if(w>240||h>268)throw new Error('Boyut '+w+'×'+h+'. En fazla 240×268 olmalı.');
  return 'Uygun: '+w+'×'+h+', '+Math.round(f.size/1024)+' KB';
}
$('f').onchange=async()=>{
  const f=$('f').files[0];if(!f)return;
  $('pv').src=URL.createObjectURL(f);$('pv').style.display='block';
  try{msg(await check(f))}catch(e){msg(e.message)}
};
function upload(){
  const f=$('f').files[0];
  if(!f){msg('Önce bir GIF seç');return}
  check(f).then(()=>{
    const fd=new FormData();fd.append('file',f,f.name);
    const x=new XMLHttpRequest();
    x.open('POST','/api/upload');
    x.upload.onprogress=e=>{if(e.lengthComputable)$('pb').style.width=(e.loaded/e.total*100)+'%'};
    x.onload=()=>{
      let j={};try{j=JSON.parse(x.responseText)}catch(e){}
      $('pb').style.width='0';$('ub').disabled=false;
      msg(j.ok?'Yüklendi: '+j.name:'Hata: '+(j.error||x.status));
      gifs();state();
    };
    x.onerror=()=>{$('ub').disabled=false;msg('Bağlantı hatası')};
    $('ub').disabled=true;msg('Yükleniyor…');
    x.send(fd);
  }).catch(e=>msg(e.message));
}
gifs();state();loadLog();setInterval(state,2000);
</script></body></html>)rawliteral";
