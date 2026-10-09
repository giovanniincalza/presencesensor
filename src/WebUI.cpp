#include "WebUI.h"
#include <WebServer.h>
#include "Whitelist.h"
#include "Log.h"
#include "config.h"

namespace {
WebServer server(80);
String lastUnknown[GATE_COUNT];
int8_t manualGate = -1;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Controllo accessi</title>
<style>
body{font-family:system-ui,sans-serif;margin:1rem auto;max-width:46rem;padding:0 1rem}
table{border-collapse:collapse;width:100%}td,th{border-bottom:1px solid #ccc;padding:.35rem;text-align:left}
input,button{font:inherit;padding:.35rem}pre{background:#f4f4f4;padding:.5rem;overflow:auto;max-height:18rem}
.row{display:flex;gap:.5rem;flex-wrap:wrap;align-items:center;margin:.5rem 0}
</style></head><body>
<h1>Controllo accessi</h1>
<h2>Apertura manuale</h2><div class="row" id="open"></div>
<h2>Nuova tessera</h2>
<div class="row"><input id="uid" placeholder="UID (es. 04A1B2C3)" size="20">
<input id="name" placeholder="Nome" size="20"></div>
<div class="row" id="gates"></div>
<div class="row"><button onclick="add()">Salva</button>
<span id="last"></span></div>
<h2>Tessere autorizzate</h2><table id="tbl"></table>
<h2>Registro</h2><pre id="log"></pre>
<script>
const G=__GATES__;
const $=id=>document.getElementById(id);
function cell(tr,t){const td=document.createElement('td');td.textContent=t;tr.appendChild(td);return td}
async function api(p,o){const r=await fetch(p,o);if(!r.ok)throw new Error(await r.text());return r}
function form(o){return {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(o)}}
G.forEach((n,i)=>{
 const b=document.createElement('button');b.textContent='Apri '+n;
 b.onclick=()=>api('/api/open',form({gate:i})).then(refresh);$('open').appendChild(b);
 const l=document.createElement('label');l.innerHTML='<input type="checkbox" id="g'+i+'" checked> ';
 l.appendChild(document.createTextNode(n));$('gates').appendChild(l);
});
async function add(){
 let m=0;G.forEach((n,i)=>{if($('g'+i).checked)m|=1<<i});
 try{await api('/api/add',form({uid:$('uid').value,name:$('name').value,gates:m}));
 $('uid').value='';$('name').value='';refresh()}catch(e){alert(e.message)}
}
async function del(u){if(confirm('Rimuovere '+u+'?')){await api('/api/del',form({uid:u}));refresh()}}
async function refresh(){
 const l=await (await api('/api/list')).json();const t=$('tbl');t.textContent='';
 const h=document.createElement('tr');['UID','Nome','Varchi',''].forEach(x=>{const th=document.createElement('th');th.textContent=x;h.appendChild(th)});t.appendChild(h);
 l.forEach(e=>{const tr=document.createElement('tr');cell(tr,e.uid);cell(tr,e.name);
  cell(tr,G.filter((n,i)=>e.gates&(1<<i)).join(', '));
  const td=document.createElement('td');const b=document.createElement('button');b.textContent='Elimina';b.onclick=()=>del(e.uid);td.appendChild(b);tr.appendChild(td);t.appendChild(tr)});
 const s=await (await api('/api/last')).json();
 const f=s.find(x=>x.uid);$('last').textContent='';
 if(f){const b=document.createElement('button');b.textContent='Usa ultima sconosciuta: '+f.uid+' ('+G[f.gate]+')';b.onclick=()=>{$('uid').value=f.uid};$('last').appendChild(b)}
 $('log').textContent=await (await api('/api/log')).text();
}
refresh();setInterval(refresh,5000);
</script></body></html>)HTML";

bool auth() {
  if (server.authenticate(ADMIN_USER, ADMIN_PASS)) return true;
  server.requestAuthentication();
  return false;
}

String jsonEscape(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    if (c >= 32) o += c;
  }
  return o;
}

void handleRoot() {
  if (!auth()) return;
  String names = "[";
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    if (i) names += ",";
    names += "\"" + jsonEscape(GATES[i].name) + "\"";
  }
  names += "]";
  String page = FPSTR(PAGE);
  page.replace("__GATES__", names);
  server.send(200, "text/html; charset=utf-8", page);
}

void handleList() {
  if (!auth()) return;
  String o = "[";
  bool first = true;
  for (const Entry &e : Whitelist::all()) {
    if (!first) o += ",";
    first = false;
    o += "{\"uid\":\"" + e.uid + "\",\"name\":\"" + jsonEscape(e.name) +
         "\",\"gates\":" + String(e.gates) + "}";
  }
  o += "]";
  server.send(200, "application/json", o);
}

void handleAdd() {
  if (!auth()) return;
  String uid = server.arg("uid");
  if (!Whitelist::normalizeUid(uid)) {
    server.send(400, "text/plain", "UID non valido (8, 14 o 20 cifre esadecimali)");
    return;
  }
  uint8_t mask = (uint8_t)server.arg("gates").toInt() & ((1 << GATE_COUNT) - 1);
  if (!mask) {
    server.send(400, "text/plain", "Selezionare almeno un varco");
    return;
  }
  String name = Whitelist::sanitizeName(server.arg("name"));
  if (!Whitelist::add(uid, name, mask)) {
    server.send(500, "text/plain", "Errore di scrittura");
    return;
  }
  Log::event("web", uid, name, "AGGIUNTA");
  server.send(200, "text/plain", "ok");
}

void handleDel() {
  if (!auth()) return;
  String uid = server.arg("uid");
  if (!Whitelist::normalizeUid(uid) || !Whitelist::remove(uid)) {
    server.send(404, "text/plain", "non trovata");
    return;
  }
  Log::event("web", uid, "", "RIMOSSA");
  server.send(200, "text/plain", "ok");
}

void handleLast() {
  if (!auth()) return;
  String o = "[";
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    if (i) o += ",";
    o += "{\"gate\":" + String(i) + ",\"uid\":\"" + lastUnknown[i] + "\"}";
  }
  o += "]";
  server.send(200, "application/json", o);
}

void handleLog() {
  if (!auth()) return;
  server.send(200, "text/plain; charset=utf-8", Log::tail());
}

void handleOpen() {
  if (!auth()) return;
  int g = server.arg("gate").toInt();
  if (g < 0 || g >= GATE_COUNT) {
    server.send(400, "text/plain", "varco non valido");
    return;
  }
  manualGate = g;
  server.send(200, "text/plain", "ok");
}
}  // namespace

namespace WebUI {

void begin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/list", HTTP_GET, handleList);
  server.on("/api/add", HTTP_POST, handleAdd);
  server.on("/api/del", HTTP_POST, handleDel);
  server.on("/api/last", HTTP_GET, handleLast);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/open", HTTP_POST, handleOpen);
  server.begin();
}

void handle() { server.handleClient(); }

void setLastUnknown(uint8_t gate, const String &uid) {
  if (gate < GATE_COUNT) lastUnknown[gate] = uid;
}

bool takeManualOpen(uint8_t &gate) {
  if (manualGate < 0) return false;
  gate = manualGate;
  manualGate = -1;
  return true;
}

}  // namespace WebUI
