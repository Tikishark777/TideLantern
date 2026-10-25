#pragma once
#include <Arduino.h>

// Dashboard is intentionally self-contained and served locally by the ESP32.
static const char WEB_UI[] PROGMEM = R"TIDELANTERN(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#061b2c"><title>TideLantern | Control Deck</title>
<style>
:root{color-scheme:dark;--bg:#061724;--panel:#0d2737;--edge:#29516b;--ink:#e8f5fc;--muted:#a6c6d3;--cyan:#6de1e9;--gold:#ffd28b}*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 80% -8%,#23556b 0%,transparent 55%),var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,sans-serif}main{max-width:860px;margin:auto;padding:20px 16px 70px}h1{margin:0;font-size:2rem;letter-spacing:-.05em}h2{font-size:1.18rem;margin:0 0 15px}p{margin:7px 0;color:var(--muted)}small{color:var(--muted)}a{color:var(--cyan)}.mast{display:flex;align-items:center;gap:14px;margin:14px 0 23px}.mark{width:57px;height:57px;border:1px solid #436d85;background:#102e40;border-radius:50%;display:grid;place-items:center;color:var(--gold);font-size:30px}.subtitle{font-size:.77rem;color:var(--gold);letter-spacing:.11em;text-transform:uppercase}.layout{display:grid;grid-template-columns:1fr 1fr;gap:16px}.panel{padding:22px;border:1px solid var(--edge);border-radius:17px;background:linear-gradient(145deg,#113044,#091f2d);margin-bottom:16px;min-width:0}.full{grid-column:1/-1}label{display:block;font-weight:650;font-size:.82rem;color:#d1e9f4;margin:13px 0 6px}input:not([type=checkbox]),select,button{font:inherit}input:not([type=checkbox]),select{width:100%;padding:12px;background:#071a28;border:1px solid #45677b;border-radius:8px;color:#fff;min-width:0}input:focus,select:focus{outline:2px solid var(--cyan);outline-offset:2px}button{background:var(--cyan);border:0;border-radius:9px;padding:12px 16px;font-weight:750;color:#062230;cursor:pointer;min-height:44px}button.alt{background:#1a4255;color:#e9f7fa;border:1px solid #3a7085}button:disabled{opacity:.65;cursor:progress}.row{display:flex;gap:9px;flex-wrap:wrap;align-items:end}.row>*{flex:1}.row button{flex:0 0 auto}.message{padding:12px;border:1px solid #325c6f;border-radius:9px;background:#0a2230;color:#d9ecf5;margin-top:12px;white-space:pre-wrap;overflow-wrap:anywhere}.warn{border-color:#886444;color:#ffdaa5}.option{display:flex;gap:10px;align-items:center;margin:10px 0}.option input{width:auto}.option label{margin:0;font-weight:400}.result{width:100%;text-align:left;background:#133447;color:#e3f6ff;border:1px solid #446980;margin-top:6px}.tag{font-size:.72rem;color:var(--gold);letter-spacing:.08em;text-transform:uppercase;font-weight:700}.tiny{font-size:.77rem}.metrics{display:grid;grid-template-columns:1fr 1fr;gap:10px}.metric{padding:11px;background:#081e2d;border:1px solid #20445a;border-radius:10px}.metric strong{display:block;font-size:1.3rem;color:#eaf8ff}.metric span{font-size:.77rem;color:#9cbac7}canvas{width:100%;height:160px;background:#061722;border-radius:9px;border:1px solid #244e67}[hidden]{display:none!important}.network-list{display:grid;gap:7px;max-height:335px;overflow:auto;margin:11px 0}.network-choice{width:100%;text-align:left;background:#102f41;color:#edfaff;border:1px solid #416e85;padding:10px 12px;display:flex;align-items:center;justify-content:space-between;gap:8px}.network-choice:hover,.network-choice.chosen{border-color:var(--cyan);background:#164356}.network-meta{font-size:.76rem;color:#afd2db;white-space:nowrap}.setup-note{padding:16px;border:1px solid #659fad;background:#113c4c;border-radius:13px;margin-bottom:15px;color:#dceef5}.setup-note strong{color:var(--gold)}.setup-note a{overflow-wrap:anywhere}.setup-mode .panel:not(#networkPanel){display:none}.network-guide{margin:0 0 18px;color:#e2f2f8}.network-guide ol{margin-bottom:8px;padding-left:24px}.network-guide li{margin:7px 0}.success-guide{border:1px solid #5d9d89;background:#103a36;padding:16px;border-radius:10px;margin-top:13px}footer{text-align:center;color:#96b4c4;font-size:.79rem;padding:22px}@media(max-width:670px){.layout{display:block}.panel{padding:17px}.mast{margin-top:4px}h1{font-size:1.8rem}}
</style></head><body><main><header class="mast"><div class="mark" aria-hidden="true">♜</div><div><div class="subtitle">Your window to the water</div><h1>TideLantern</h1></div></header>
<div class="setup-note" id="topNetworkGuide" hidden role="status"></div>
<div class="layout">
<section class="panel"><div class="tag">Live instrument</div><h2 id="siteHeading">Loading…</h2><div class="metrics"><div class="metric"><strong id="nextEvent">—</strong><span>Next NOAA high / low</span></div><div class="metric"><strong id="windMetric">—</strong><span>Forecast wind (knots)</span></div><div class="metric"><strong id="waveMetric">—</strong><span>Forecast waves</span></div><div class="metric"><strong id="waterMetric">—</strong><span>Sea temperature</span></div></div><p class="tiny" id="ageText">Checking source timestamps…</p></section>
<section class="panel"><div class="tag">Predicted tide curve</div><h2>NOAA high / low events</h2><canvas id="curve" width="500" height="160" role="img" aria-label="Schematic curve interpolated from predicted NOAA high and low tides"></canvas><p class="tiny">Schematic interpolation between NOAA high/low events, not a measured water level or navigation chart.</p></section>
<section class="panel full"><div class="tag">1 · Your waterway</div><h2>Search anywhere in the world</h2><p>Search by place name, harbor or marina. NOAA tide stations are available for supported U.S. waters. Marine forecasts can be available globally.</p><div class="row"><div><label for="search">Place, harbor, or waterway</label><input id="search" maxlength="100" placeholder="Friday Harbor, WA · Sydney Harbour · Southampton"></div><button id="searchBtn" type="button">Search</button></div><div id="searchResults"></div><label for="siteName">Display location name</label><input id="siteName" maxlength="64"><div class="row"><div><label for="lat">Latitude</label><input type="number" id="lat" min="-90" max="90" step="0.00001"></div><div><label for="lon">Longitude</label><input type="number" id="lon" min="-180" max="180" step="0.00001"></div></div>
<label for="station">NOAA tide station</label><select id="station"></select><p class="tiny" id="stationHelp">Choose a tide station near your waterway. For international locations without NOAA coverage, the marine display remains available and tides show unavailable.</p><div class="row"><div><label for="manualStation">Or manually enter NOAA station ID</label><input id="manualStation" maxlength="10" placeholder="9447130"></div><button id="manualBtn" class="alt" type="button">Use ID</button></div><button id="savePlace" type="button">Save waterway & station</button><div class="message" id="placeMsg" role="status">Ready.</div><p class="tiny">Place search via © OpenStreetMap contributors / Nominatim (user-initiated, no autocomplete). Tide stations via NOAA CO-OPS.</p></section>
<section class="panel"><div class="tag">2 · Right screen</div><h2>Tides & marine conditions</h2><label for="view">Display mode</label><select id="view"><option value="0">Tide graph</option><option value="1">Marine conditions</option><option value="2">Alternate tide / marine (20 seconds)</option></select><label for="rightTheme">Right screen appearance</label><select id="rightTheme"><option value="0">Classic — original display</option><option value="1">Coastal — ocean accents & tide graph</option><option value="2">Minimal — large readings</option></select><p class="tiny">Change the look without changing your tides, forecasts, or saved waterway.</p><label for="units">Measurements</label><select id="units"><option value="0">Feet / Fahrenheit / knots</option><option value="1">Meters / Celsius / knots</option></select><button id="saveDisplay" type="button">Apply display settings</button><div class="message" id="displayMsg" role="status">Choose Classic, Coastal or Minimal for the right screen. Your waterway data stays the same.</div></section>
<section class="panel"><div class="tag">3 · Left screen</div><h2>Clock & date</h2><label for="clockTheme">Left screen appearance</label><select id="clockTheme"><option value="0">Classic — original clock</option><option value="1">Coastal — sun & ocean waves</option><option value="2">Minimal — clean large time</option></select><p class="tiny">Clock format, dates, time zone, and optional seconds work in every theme.</p><label for="clock24">Time format</label><select id="clock24"><option value="0">12-hour AM/PM</option><option value="1">24-hour</option></select><label for="dateStyle">Date format</label><select id="dateStyle"><option value="0">Full (October 24, 2026)</option><option value="1">U.S. (10/24/2026)</option><option value="2">International (24/10/2026)</option><option value="3">ISO (2026-10-24)</option></select><label for="tzStyle">Time-zone label</label><select id="tzStyle"><option value="0">Regional abbreviation / UTC offset</option><option value="1">Always show UTC offset</option><option value="2">Hide time zone</option></select><div class="option"><input type="checkbox" id="seconds"><label for="seconds">Show seconds (optional)</label></div><div class="option"><input type="checkbox" id="leadingZero"><label for="leadingZero">Leading zero in 12-hour time</label></div><button id="saveClock" type="button">Apply clock settings</button><div class="message" id="clockMsg" role="status">Choose a clock theme independently of the tide screen. The clock follows your waterway's local time zone.</div></section>
<section class="panel full" id="networkPanel"><div class="tag">4 · Network</div><h2 id="wifiTitle">Connect to Wi-Fi</h2>
<div class="network-guide"><p><strong>First-time setup:</strong> Connect your phone to <strong>TideLantern-Setup</strong>, password <strong>Tides2026!</strong>. Open <strong>http://192.168.4.1/setup</strong> in the phone's browser. If your phone says there is no Internet, stay connected to this setup network.</p>
<p>Choose a nearby <strong>2.4 GHz</strong> network below, or manually enter a network name if it is hidden.</p></div>
<div class="row"><button type="button" id="scanWifi" class="alt">Scan nearby Wi-Fi</button><div><label for="networkFilter">Search networks</label><input type="search" id="networkFilter" placeholder="Filter by Wi-Fi name" autocomplete="off"></div></div>
<div class="message" id="scanMsg" role="status">Tap Scan nearby Wi-Fi to list available networks.</div>
<div id="wifiResults" class="network-list" role="list" aria-label="Nearby Wi-Fi networks"></div>
<form id="wifiForm"><label for="ssid">Selected or manually entered network name (SSID)</label><input id="ssid" maxlength="32" autocomplete="off" placeholder="Tap a network above, or type its name" required>
<label for="password">Wi-Fi password</label><input id="password" type="password" maxlength="64" autocomplete="new-password" placeholder="Password (leave blank only for an open network)">
<div class="option"><input id="showWifiPassword" type="checkbox"><label for="showWifiPassword">Show password</label></div>
<button type="submit" id="wifiSave">Save Wi-Fi &amp; restart</button></form>
<div class="message" id="wifiMsg" role="status">This device connects to 2.4 GHz Wi-Fi. Your phone must rejoin your normal Wi-Fi after saving.</div>
<div class="success-guide" id="afterWifi" hidden role="status"><strong>Wi-Fi saved. Here's how to open settings after TideLantern restarts:</strong>
<ol><li>Reconnect your phone to your <strong>home Wi-Fi</strong> (not TideLantern-Setup).</li><li>Look at the <strong>bottom of the left round clock</strong> for the SETTINGS PAGE IP address.</li><li>Type <strong>http://</strong> followed by that address into your phone's browser. Example: <strong>http://192.168.1.123/</strong> (your actual address will be different).</li><li>As a shortcut, try <a href="http://tidelantern.local/">http://tidelantern.local/</a> if your home network supports it.</li></ol>
<p>If the device returns to the TideLantern-Setup network, check the password and confirm you chose a 2.4 GHz network.</p></div></section>
</div><footer>TideLantern preview v0.1 · NOAA CO-OPS · Open-Meteo · OpenStreetMap contributors<br>Information only. Not for navigation, safety decisions, or voyage planning.</footer></main>
<script>
'use strict';
const $ = id=>document.getElementById(id);
const CATALOG='https://tikishark777.github.io/TideLantern/tide-stations.json';
const FALLBACK=[{id:'9447130',name:'Seattle, WA',lat:47.6026,lon:-122.3393,type:'R'},{id:'9446484',name:'Tacoma, WA',lat:47.2667,lon:-122.4133,type:'R'},{id:'9444900',name:'Port Townsend, WA',lat:48.1117,lon:-122.7597,type:'R'},{id:'9449880',name:'Friday Harbor, WA',lat:48.5453,lon:-123.0125,type:'R'},{id:'9414290',name:'San Francisco, CA',lat:37.8063,lon:-122.4659,type:'R'},{id:'8518750',name:'The Battery, NY',lat:40.7006,lon:-74.0142,type:'R'}];
let stations=FALLBACK, selectedStation='', current={},lastLookup=0;
function msg(id,s,bad=false){$(id).textContent=s;$(id).classList.toggle('warn',bad)}
async function jsonApi(url,options={}){let r=await fetch(url,options);let data=await r.json().catch(()=>({error:'Invalid server response'}));if(!r.ok||data.error)throw Error(data.error||'HTTP '+r.status);return data}
function rad(x){return x*Math.PI/180}
function km(a,b,c,d){const r=6371,da=rad(c-a),dl=rad(d-b);const q=Math.sin(da/2)**2+Math.cos(rad(a))*Math.cos(rad(c))*Math.sin(dl/2)**2;return 2*r*Math.asin(Math.min(1,Math.sqrt(q)))}
function populateStations(lat,lon,preferred=''){
  const valid=Number.isFinite(lat)&&Number.isFinite(lon);const near=valid?stations.map(s=>({...s,dist:km(lat,lon,s.lat,s.lon)})).sort((a,b)=>a.dist-b.dist).slice(0,10):[];
  let list=near.filter(s=>s.dist<=200);
  const saved=stations.find(s=>s.id===preferred);if(saved&&!list.some(s=>s.id===saved.id))list.unshift({...saved,dist:valid?km(lat,lon,saved.lat,saved.lon):0});
  const sel=$('station');sel.replaceChildren();const empty=document.createElement('option');empty.value='';empty.textContent='No tide station selected — marine only';sel.appendChild(empty);
  for(const s of list){const opt=document.createElement('option');opt.value=s.id;opt.textContent=s.name+' · '+s.dist.toFixed(0)+' km'+(s.type==='S'?' · high/low only':'');sel.appendChild(opt)}
  if(preferred&&!Array.from(sel.options).some(o=>o.value===preferred)) {const o=document.createElement('option');o.value=preferred;o.textContent=preferred+' · manually selected';sel.appendChild(o)}
  sel.value=preferred||((list.length&&list[0].dist<=80)?list[0].id:'');selectedStation=sel.value;
  $('stationHelp').textContent=list.length?'Check that the chosen station represents your actual waterway; tides vary substantially by inlet and channel.':'No NOAA prediction station within 200 km. Worldwide marine forecasts may still be available.';
}
async function loadCatalog(){try{const r=await fetch(CATALOG,{cache:'default'});if(!r.ok)throw Error('Catalog unavailable');const d=await r.json();if(Array.isArray(d.stations)&&d.stations.length>100)stations=d.stations;}catch(e){console.info('Using compact offline tide-station fallback',e.message)}populateStations(Number($('lat').value),Number($('lon').value),current.stationId||'')}
$('searchBtn').addEventListener('click',async()=>{const q=$('search').value.trim();if(q.length<3){msg('placeMsg','Enter at least three characters.',true);return}if(Date.now()-lastLookup<1500){msg('placeMsg','Please allow a moment between searches.',true);return}lastLookup=Date.now();$('searchBtn').disabled=true;msg('placeMsg','Searching waterways and places…');$('searchResults').replaceChildren();try{const results=await jsonApi('/api/search?q='+encodeURIComponent(q));if(!Array.isArray(results)||!results.length)throw Error('No matching places found. Try a city, harbor name or coordinates.');results.slice(0,5).forEach(p=>{const b=document.createElement('button');b.className='result';b.textContent=p.display_name||'Unnamed place';b.addEventListener('click',()=>{$('siteName').value=(p.display_name||q).split(',').slice(0,2).join(',');$('lat').value=Number(p.lat).toFixed(5);$('lon').value=Number(p.lon).toFixed(5);populateStations(Number(p.lat),Number(p.lon));$('searchResults').replaceChildren();msg('placeMsg','Location selected. Review the nearby NOAA station, then save.');});$('searchResults').appendChild(b)});msg('placeMsg','Choose the desired search result.');}catch(e){msg('placeMsg',e.message,true)}finally{$('searchBtn').disabled=false}});
for(const id of ['lat','lon'])$(id).addEventListener('change',()=>populateStations(Number($('lat').value),Number($('lon').value),$('station').value));
$('station').addEventListener('change',()=>{selectedStation=$('station').value;$('manualStation').value=selectedStation});
$('manualBtn').addEventListener('click',()=>{const id=$('manualStation').value.trim();if(id&&!/^[a-zA-Z0-9]{5,10}$/.test(id)){msg('placeMsg','A NOAA station ID is 5–10 letters/numbers.',true);return}populateStations(Number($('lat').value),Number($('lon').value),id);msg('placeMsg',id?'Manual station ID selected; save the waterway to apply it.':'Station cleared; marine only.');});
async function save(id,fields,msgid){$(id).disabled=true;try{const body=new URLSearchParams();for(const k of fields){let el=$(k);body.set(k,el.type==='checkbox'?(el.checked?'1':'0'):el.value)}const j=await jsonApi('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});msg(msgid,j.message||'Saved.');setTimeout(loadStatus,600)}catch(e){msg(msgid,'Could not save: '+e.message,true)}finally{$(id).disabled=false}}
$('savePlace').addEventListener('click',()=>save('savePlace',['siteName','lat','lon','station'],'placeMsg'));
$('saveDisplay').addEventListener('click',()=>save('saveDisplay',['view','units','rightTheme'],'displayMsg'));
$('saveClock').addEventListener('click',()=>save('saveClock',['clock24','dateStyle','tzStyle','seconds','leadingZero','clockTheme'],'clockMsg'));
let discoveredNetworks=[], chosenSecure=null, chosenSSID='';
function renderWifiChoices(){
  const query=$('networkFilter').value.trim().toLowerCase(),list=$('wifiResults');
  list.replaceChildren();
  const matches=discoveredNetworks.filter(n=>n.ssid.toLowerCase().includes(query));
  for(const n of matches){
    const choice=document.createElement('button');choice.type='button';choice.className='network-choice';
    if(chosenSSID===n.ssid)choice.classList.add('chosen');
    choice.setAttribute('role','listitem');
    const label=document.createElement('span');label.textContent=n.ssid;
    const meta=document.createElement('span');meta.className='network-meta';
    const quality=n.rssi>=-55?'Strong':n.rssi>=-70?'Good':'Weak';
    meta.textContent=quality+' · '+(n.secure?'Secured':'Open');
    choice.append(label,meta);
    choice.addEventListener('click',()=>{
      chosenSSID=n.ssid;chosenSecure=!!n.secure;
      $('ssid').value=n.ssid;$('password').value='';
      $('password').placeholder=n.secure?'Enter password for '+n.ssid:'No password needed (open network)';
      renderWifiChoices();
      msg('wifiMsg','Selected '+n.ssid+(n.secure?'. Enter its Wi-Fi password, then save.':'. This network does not require a password.'));
      if(n.secure)$('password').focus();
    });list.appendChild(choice);
  }
  if(!matches.length&&discoveredNetworks.length)msg('scanMsg','No networks match your search. You can still enter the name manually.');
  else if(discoveredNetworks.length)msg('scanMsg',matches.length+' matching network(s). Select one below.');
}
async function scanWifi(){
  $('scanWifi').disabled=true;msg('scanMsg','Scanning nearby 2.4 GHz Wi-Fi networks…');
  try{
    const data=await jsonApi('/api/wifi/scan',{cache:'no-store'});
    discoveredNetworks=Array.isArray(data.networks)?data.networks:[];
    discoveredNetworks.sort((a,b)=>b.rssi-a.rssi);
    renderWifiChoices();
    if(!discoveredNetworks.length)msg('scanMsg','No networks found. Try another scan, move closer to the router, or enter the name manually.',true);
  }catch(e){msg('scanMsg','Scan unavailable: '+e.message+'. You can type the network name manually.',true)}
  finally{$('scanWifi').disabled=false;}
}
$('scanWifi').addEventListener('click',scanWifi);
$('networkFilter').addEventListener('input',renderWifiChoices);
$('ssid').addEventListener('input',()=>{if($('ssid').value!==chosenSSID){chosenSecure=null;chosenSSID='';}});
$('showWifiPassword').addEventListener('change',()=>{$('password').type=$('showWifiPassword').checked?'text':'password';});
$('wifiForm').addEventListener('submit',async e=>{
  e.preventDefault();
  const pass=$('password').value;
  if(chosenSecure===true&&pass.length<8){msg('wifiMsg','This is a secured network. Enter its password (at least 8 characters).',true);return;}
  if(pass.length>0&&pass.length<8){msg('wifiMsg','The password must have at least 8 characters, or be blank for an open network.',true);return;}
  $('wifiSave').disabled=true;msg('wifiMsg','Saving Wi-Fi. Device will restart in a few seconds…');
  try{
    await jsonApi('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({ssid:$('ssid').value,password:pass})});
    msg('wifiMsg','Wi-Fi saved! Follow the instructions below to return to settings.');
    $('afterWifi').hidden=false;$('afterWifi').scrollIntoView({behavior:'smooth',block:'nearest'});
  }catch(err){msg('wifiMsg',err.message,true);$('wifiSave').disabled=false;}
});
async function showNetworkGuide(){
  try{
    const net=await jsonApi('/api/network',{cache:'no-store'});
    const guide=$('topNetworkGuide');guide.hidden=false;
    if(net.setupMode){
      document.body.classList.add('setup-mode');
      $('wifiTitle').textContent='Step 1 · Connect TideLantern to Wi-Fi';
      guide.textContent='Wi-Fi setup mode: select a nearby 2.4 GHz network, enter its password, and save. After restarting, reconnect your phone to home Wi-Fi and read the SETTINGS PAGE address at the bottom of the clock.';
      scanWifi();
    }else if(net.connected){
      const url='http://'+net.ip+'/';
      const link=document.createElement('a');link.href=url;link.textContent=url;
      guide.replaceChildren(document.createTextNode('TideLantern is connected to '+(net.ssid||'home Wi-Fi')+'. This page is its settings dashboard. To return later, connect your phone to the same home Wi-Fi and open '),link,document.createTextNode(' or try http://tidelantern.local/ (where supported). The IP is also shown at the bottom of the clock.'));
      $('wifiTitle').textContent='Change Wi-Fi network';
    }
  }catch(e){console.info('Network guide unavailable',e.message)}
}
showNetworkGuide();
function renderCurve(events){const c=$('curve'),ctx=c.getContext('2d'),w=c.width,h=c.height;ctx.clearRect(0,0,w,h);ctx.strokeStyle='#265a74';ctx.lineWidth=1;for(let i=1;i<4;i++){ctx.beginPath();ctx.moveTo(0,h*i/4);ctx.lineTo(w,h*i/4);ctx.stroke()}if(events.length<2){ctx.fillStyle='#b2d3df';ctx.font='16px system-ui';ctx.fillText('No tide predictions available',20,80);return}const arr=events.map(e=>({t:Number(e.epoch),v:Number(e.height)})).filter(e=>Number.isFinite(e.t)&&Number.isFinite(e.v));if(arr.length<2)return;const lo=Math.min(...arr.map(a=>a.v)),hi=Math.max(...arr.map(a=>a.v)),delta=Math.max(1,hi-lo),t0=arr[0].t,t1=arr[arr.length-1].t,pt=t=>6+(t-t0)/(t1-t0)*(w-12),py=v=>h-12-(v-lo+delta*.08)/(delta*1.16)*(h-22);ctx.lineWidth=3;ctx.strokeStyle='#70e0f1';ctx.beginPath();for(let j=0;j<arr.length-1;j++){const a=arr[j],b=arr[j+1];for(let k=0;k<=22;k++){const f=k/22,x=pt(a.t+(b.t-a.t)*f),v=a.v+(b.v-a.v)*(1-Math.cos(Math.PI*f))/2,y=py(v);if(j===0&&k===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)}}ctx.stroke();const now=Date.now()/1000;if(now>=t0&&now<=t1){const x=pt(now);ctx.setLineDash([4,4]);ctx.strokeStyle='#ffd388';ctx.beginPath();ctx.moveTo(x,4);ctx.lineTo(x,h-4);ctx.stroke();ctx.setLineDash([]);ctx.fillStyle='#ffd388';ctx.font='13px system-ui';ctx.fillText('NOW',Math.min(w-40,x+5),18)}}
async function loadStatus(){try{const d=await jsonApi('/api/state');current=d;$('siteHeading').textContent=d.siteName||'TideLantern';$('nextEvent').textContent=d.nextEvent||'—';$('windMetric').textContent=d.windKn===null?'—':d.windKn.toFixed(0);$('waveMetric').textContent=d.wave===null?'—':d.wave.toFixed(1)+' '+(d.units===1?'m':'ft');$('waterMetric').textContent=d.water===null?'—':d.water.toFixed(0)+'°'+(d.units===1?'C':'F');$('ageText').textContent=d.status||'Waiting for data';renderCurve(d.events||[]);
 $('siteName').value=d.siteName||'';$('lat').value=d.lat;$('lon').value=d.lon;$('manualStation').value=d.stationId||'';$('view').value=d.view;$('units').value=d.units;$('rightTheme').value=d.rightTheme??0;$('clockTheme').value=d.clockTheme??0;$('clock24').value=d.clock24?'1':'0';$('dateStyle').value=d.dateStyle;$('tzStyle').value=d.tzStyle;$('seconds').checked=!!d.seconds;$('leadingZero').checked=!!d.leadingZero;populateStations(Number(d.lat),Number(d.lon),d.stationId||'');
 }catch(e){$('ageText').textContent='Device status unavailable: '+e.message}}
loadStatus().then(loadCatalog);setInterval(async()=>{try{const d=await jsonApi('/api/state');$('nextEvent').textContent=d.nextEvent||'—';$('windMetric').textContent=d.windKn===null?'—':d.windKn.toFixed(0);$('waveMetric').textContent=d.wave===null?'—':d.wave.toFixed(1)+' '+(d.units===1?'m':'ft');$('waterMetric').textContent=d.water===null?'—':d.water.toFixed(0)+'°'+(d.units===1?'C':'F');$('ageText').textContent=d.status||'';renderCurve(d.events||[])}catch(e){}},45000);
</script></body></html>
)TIDELANTERN";
