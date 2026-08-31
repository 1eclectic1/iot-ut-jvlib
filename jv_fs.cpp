#if defined(__has_include)
  #if __has_include("jv_config.h")
    #include "jv_config.h"
  #else
    #include "jv_config_defaults.h"
  #endif
#else
  #include "jv_config_defaults.h"
#endif

// jv_fs.cpp - optional LittleFS source-code / file manager

#include "jvlib.h"

#ifdef JV_ENABLE_FS_MANAGER

#include <LittleFS.h>
#ifdef ESP32
  #include <WebServer.h>
  static WebServer fsServer(80);
#else
  #include <ESP8266WebServer.h>
  static ESP8266WebServer fsServer(80);
#endif

static File uploadFile;

static const char FS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>jvlib FS</title>
<style>
body{font-family:sans-serif;margin:16px;background:#f4f4f9}
.card{background:#fff;padding:16px;border-radius:8px;box-shadow:0 1px 3px #0002;margin-bottom:16px}
button{background:#0076a3;color:#fff;border:none;padding:8px 14px;border-radius:4px}
ul{list-style:none;padding:0}li{padding:8px;background:#eee;margin:4px 0;border-radius:4px;display:flex;justify-content:space-between}
a{color:#0076a3;text-decoration:none}.del{color:#c00;margin-left:12px}
</style></head><body>
<div class='card'><h2>Upload</h2>
<form method='POST' action='/littlefs/upload' enctype='multipart/form-data'>
<input type='file' name='name'><button type='submit'>Upload</button></form></div>
<div class='card'><h2>Files</h2><div id='list'>Loading...</div>
<div id='stats'></div></div>
<script>
function load(){fetch('/littlefs/list').then(r=>r.json()).then(d=>{
let h='<ul>';if(!d.files.length)h+='<li>No files</li>';
d.files.forEach(f=>{h+=`<li><a href="/littlefs/view?path=${encodeURIComponent(f.name)}" download>${f.name} (${f.size} KB)</a>
<a class=del href=# onclick="del('${f.name}')">✕</a></li>`});
h+='</ul>';document.getElementById('list').innerHTML=h;
document.getElementById('stats').innerText=`Used ${d.used} / ${d.total} KB`;
})}
function del(p){if(confirm('Delete '+p+'?'))fetch('/littlefs/delete?path='+encodeURIComponent(p),{method:'POST'}).then(load)}
window.onload=load;
</script></body></html>
)rawliteral";

static void handleIndex() { fsServer.send_P(200, "text/html", FS_HTML); }

static void handleList() {
  size_t total = 0, used = 0;
#ifdef ESP32
  total = LittleFS.totalBytes(); used = LittleFS.usedBytes();
#else
  FSInfo info; LittleFS.info(info); total = info.totalBytes; used = info.usedBytes;
#endif
  String json = "{\"total\":" + String(total/1024) + ",\"used\":" + String(used/1024) + ",\"files\":[";
  bool first = true;
#ifdef ESP32
  File root = LittleFS.open("/"); File f = root.openNextFile();
  while (f) {
    if (!first) json += ","; first = false;
    String n = f.name(); if (!n.startsWith("/")) n = "/" + n;
    json += "{\"name\":\"" + n + "\",\"size\":" + String(f.size()/1024.0,1) + "}";
    f = root.openNextFile();
  }
#else
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    if (!first) json += ","; first = false;
    String n = dir.fileName(); if (!n.startsWith("/")) n = "/" + n;
    json += "{\"name\":\"" + n + "\",\"size\":" + String(dir.fileSize()/1024.0,1) + "}";
  }
#endif
  json += "]}";
  fsServer.send(200, "application/json", json);
}

static void handleView() {
  if (!fsServer.hasArg("path")) { fsServer.send(400, "text/plain", "bad path"); return; }
  String path = fsServer.arg("path");
  if (!LittleFS.exists(path)) { fsServer.send(404, "text/plain", "not found"); return; }
  File f = LittleFS.open(path, "r");
  fsServer.streamFile(f, "text/plain");
  f.close();
}

static void handleUpload() {
  HTTPUpload& up = fsServer.upload();
  if (up.status == UPLOAD_FILE_START) {
    String name = up.filename;
    if (!name.startsWith("/")) name = "/" + name;
    uploadFile = LittleFS.open(name, "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    fsServer.sendHeader("Location", "/littlefs");
    fsServer.send(303);
  }
}

static void handleDelete() {
  if (!fsServer.hasArg("path")) { fsServer.send(400, "text/plain", "bad path"); return; }
  String path = fsServer.arg("path");
  if (LittleFS.exists(path)) { LittleFS.remove(path); fsServer.send(200, "text/plain", "deleted"); }
  else fsServer.send(404, "text/plain", "not found");
}

void jvFsManagerBegin() {
#ifdef ESP32
  if (!LittleFS.begin(true)) return;
#else
  if (!LittleFS.begin()) { LittleFS.format(); LittleFS.begin(); }
#endif
  fsServer.on("/littlefs", HTTP_GET, handleIndex);
  fsServer.on("/littlefs/list", HTTP_GET, handleList);
  fsServer.on("/littlefs/view", HTTP_GET, handleView);
  fsServer.on("/littlefs/upload", HTTP_POST, [](){}, handleUpload);
  fsServer.on("/littlefs/delete", HTTP_POST, handleDelete);
  fsServer.begin();
  LOG_INFO("FS manager at http://%s/littlefs", WiFi.localIP().toString().c_str());
}

void jvFsManagerLoop() {
  fsServer.handleClient();
}

#endif // JV_ENABLE_FS_MANAGER
