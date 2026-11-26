/*
  net.ino  -  Wi-Fi (AP) + web sunucu + REST API + config.json + GIF yönetimi
  --------------------------------------------------------------------------
  Bu sekme ana dosyadan sonra derlenir; ana dosyadaki değişkenleri kullanır.
  Wi-Fi sadece BAGLANTI ekranındayken açıktır (BT201 sesi bozulmasın diye).

  REST API (telefondaki uygulama / tarayıcı bunları kullanır):
    GET  /                      -> web arayüzü
    GET  /api/state             -> çalma durumu, ses, depolama bilgisi
    GET  /api/gifs              -> GIF listesi (sabit + kullanıcı)
    GET  /api/file?f=/user/x.gif-> GIF dosyası (önizleme için)
    POST /api/upload            -> multipart GIF yükleme (alan adı: file)
    POST /api/select?file=...   -> aktif GIF'i değiştir
    POST /api/delete?file=...   -> kullanıcı GIF'ini sil (sabitler silinemez)
    POST /api/cmd?c=playpause|next|prev|mute|source
    POST /api/volume?v=0..30
    POST /api/savevolume        -> mevcut sesi açılış sesi yap
    POST /api/factory           -> kullanıcı GIF'lerini sil
*/

#include "web_ui.h"

WebServer server(80);
bool serverBuilt = false;

#define MAX_GIF_W      320
#define MAX_GIF_H      240
#define MAX_UPLOAD_KB  900

// ---------------------- Kara kutu kaydi (kalici, /log.txt) -------
void trimLog() {
  File f = LittleFS.open("/log.txt", "r");
  if (!f) return;
  size_t sz = f.size();
  if (sz < 12000) { f.close(); return; }
  f.seek(sz - 7000);
  f.readStringUntil('\n');                  // yarim satiri at
  String rest = f.readString();
  f.close();
  File w = LittleFS.open("/log.txt", "w");
  if (w) { w.print(rest); w.close(); }
}

void logInit() {
  Preferences prefs;
  prefs.begin("muzik", false);
  bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", bootCount);
  prefs.end();
  logReady = true;
  trimLog();
}

void logEvent(const String& msg) {
  Serial.println("LOG> " + msg);
  if (!logReady) return;
  File f = LittleFS.open("/log.txt", "a");
  if (!f) return;
  f.printf("#%lu %lus %s\n", bootCount, millis() / 1000, msg.c_str());
  size_t sz = f.size();
  f.close();
  if (sz > 16000) trimLog();
}

String readLogTail(size_t maxBytes) {
  File f = LittleFS.open("/log.txt", "r");
  if (!f) return "(kayit yok)";
  size_t sz = f.size();
  if (sz > maxBytes) { f.seek(sz - maxBytes); f.readStringUntil('\n'); }
  String s = f.readString();
  f.close();
  return s;
}

void clearLog() {
  LittleFS.remove("/log.txt");
}

// Fabrika ayarı: config.json yoksa ya da bozuksa bu yazılır.
const char* FACTORY_CONFIG = R"JSON({
  "device_name": "MuzikCalar",
  "ap_password": "12345678",
  "active_gif": "/default/plak.gif",
  "default_volume": 15,
  "wifi_always_on": false,
  "max_volume": 20,
  "defaults": [
    {"name": "Plak", "file": "/default/plak.gif"}
  ]
})JSON";

// ---------------------- Config ----------------------------------
bool readConfig(JsonDocument& doc) {
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  return !e;
}

void writeFactoryConfig() {
  File f = LittleFS.open("/config.json", "w");
  if (f) { f.print(FACTORY_CONFIG); f.close(); }
}

String firstExistingDefault() {
  JsonDocument cfg;
  if (readConfig(cfg)) {
    for (JsonObject d : cfg["defaults"].as<JsonArray>()) {
      String p = d["file"] | "";
      if (p.length() && LittleFS.exists(p)) return p;
    }
  }
  return String("/default/plak.gif");
}

void configLoad() {
  if (!LittleFS.exists("/user")) LittleFS.mkdir("/user");
  JsonDocument doc;
  if (!readConfig(doc)) {
    Serial.println("config.json yok/bozuk -> fabrika ayari yaziliyor");
    writeFactoryConfig();
    doc.clear();
    readConfig(doc);
  }
  deviceName    = (const char*)(doc["device_name"] | "MuzikCalar");
  apPass        = (const char*)(doc["ap_password"] | "12345678");
  if (apPass.length() < 8) apPass = "12345678";      // WPA2 en az 8 karakter ister
  defaultVolume = constrain((int)(doc["default_volume"] | 15), 0, 30);
  wifiKeepOn    = doc["wifi_always_on"] | false;
  volLimit      = constrain((int)(doc["max_volume"] | 20), 5, 20);
  String act    = (const char*)(doc["active_gif"] | "/default/plak.gif");
  activeGifPath = LittleFS.exists(act) ? act : firstExistingDefault();
}

void configSave() {
  JsonDocument doc;
  if (!readConfig(doc)) deserializeJson(doc, FACTORY_CONFIG);
  doc["active_gif"] = activeGifPath;
  doc["default_volume"] = defaultVolume;
  doc["wifi_always_on"] = wifiKeepOn;
  doc["max_volume"] = volLimit;
  File f = LittleFS.open("/config.json", "w");
  if (f) { serializeJsonPretty(doc, f); f.close(); }
}

// ---------------------- Yardımcılar -----------------------------
bool isBuiltin(const String& p) { return p.startsWith("/default/"); }

bool safePath(const String& p) {
  if (p.indexOf("..") >= 0) return false;
  if (!(p.startsWith("/user/") || p.startsWith("/default/"))) return false;
  return p.endsWith(".gif");
}

String sanitizeName(String fn) {
  int slash = fn.lastIndexOf('/');
  if (slash >= 0) fn = fn.substring(slash + 1);
  int dot = fn.lastIndexOf('.');
  if (dot > 0) fn = fn.substring(0, dot);
  String out;
  for (size_t i = 0; i < fn.length() && out.length() < 20; i++) {
    char c = fn[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_') out += (char)tolower(c);
    else if (c == ' ') out += '_';
  }
  if (out.length() == 0) out = "gif" + String(millis() % 10000);
  if (out == "_tmp") out = "tmp_gif";
  return out + ".gif";
}

void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

String baseName(String n) {
  int sl = n.lastIndexOf('/');
  return (sl >= 0) ? n.substring(sl + 1) : n;
}

String listGifsJson() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  JsonDocument cfg;
  if (readConfig(cfg)) {
    for (JsonObject d : cfg["defaults"].as<JsonArray>()) {
      String p = d["file"] | "";
      if (!p.length() || !LittleFS.exists(p)) continue;
      File f = LittleFS.open(p, "r");
      size_t sz = f ? f.size() : 0;
      if (f) f.close();
      JsonObject o = arr.add<JsonObject>();
      o["name"] = (const char*)(d["name"] | "Sabit");
      o["file"] = p;
      o["builtin"] = true;
      o["size"] = sz;
      o["active"] = (p == activeGifPath);
    }
  }
  const char* extraN[2] = {"Kuzey Yildizi", "Kanunsuzlar"};     // sarki GIF'leri de secilebilsin
  const char* extraF[2] = {"/default/kuzey.gif", "/default/kanunsuz.gif"};
  for (int i = 0; i < 2; i++) {
    if (!LittleFS.exists(extraF[i])) continue;
    File f = LittleFS.open(extraF[i], "r");
    size_t sz = f ? f.size() : 0;
    if (f) f.close();
    JsonObject o = arr.add<JsonObject>();
    o["name"] = extraN[i];
    o["file"] = extraF[i];
    o["builtin"] = true;
    o["size"] = sz;
    o["active"] = (String(extraF[i]) == activeGifPath);
  }

  File dir = LittleFS.open("/user");
  if (dir && dir.isDirectory()) {
    File f = dir.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        String n = baseName(String(f.name()));
        if (n.endsWith(".gif") && n != "_tmp.gif") {
          String p = "/user/" + n;
          JsonObject o = arr.add<JsonObject>();
          o["name"] = n.substring(0, n.length() - 4);
          o["file"] = p;
          o["builtin"] = false;
          o["size"] = (size_t)f.size();
          o["active"] = (p == activeGifPath);
        }
      }
      f = dir.openNextFile();
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
}

String stateJson() {
  JsonDocument d;
  d["name"] = deviceName;
  d["fw"] = "v6";
  d["reset"] = resetName;
  d["wifi_keep"] = wifiKeepOn;
  d["max_volume"] = volLimit;
  d["boots"] = bootCount;
  d["title"] = titleAscii;
  d["playing"] = isPlaying;
  d["muted"] = muted;
  d["volume"] = volume;
  d["source"] = srcLabel();
  d["elapsed"] = elapsedMs / 1000;
  d["length"] = trackLenSec;
  d["active"] = activeGifPath;
  d["fs_total"] = LittleFS.totalBytes();
  d["fs_used"] = LittleFS.usedBytes();
  d["heap"] = ESP.getFreeHeap();
  String o;
  serializeJson(d, o);
  return o;
}

// ---------------------- Yükleme ---------------------------------
File uploadFile;
String uploadTmp = "/user/_tmp.gif";
String uploadFinal = "";
bool uploadOk = true;
String uploadErr = "";
size_t uploadBytes = 0;

void uploadFail(const char* why) {
  uploadOk = false;
  uploadErr = why;
}

void handleUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadOk = true; uploadErr = ""; uploadBytes = 0; netBytes = 0;
    uploadFinal = "/user/" + sanitizeName(up.filename);
    if (!LittleFS.exists("/user")) LittleFS.mkdir("/user");
    uploadFile = LittleFS.open(uploadTmp, "w");
    if (!uploadFile) uploadFail("Dosya acilamadi");
    netStatus = "Yukleniyor: " + baseName(uploadFinal);
  }
  else if (up.status == UPLOAD_FILE_WRITE) {
    if (!uploadOk) return;
    if (uploadBytes == 0) {                                   // ilk parça: GIF başlığını doğrula
      if (up.currentSize < 10 || memcmp(up.buf, "GIF8", 4) != 0 ||
          (up.buf[4] != '7' && up.buf[4] != '9') || up.buf[5] != 'a') {
        uploadFail("GIF dosyasi degil");
      } else {
        int w = up.buf[6] | (up.buf[7] << 8);
        int h = up.buf[8] | (up.buf[9] << 8);
        if (w <= 0 || h <= 0 || w > MAX_GIF_W || h > MAX_GIF_H) uploadFail("Boyut en fazla 320x240 olmali");
      }
    }
    if (uploadOk) {
      size_t freeB = LittleFS.totalBytes() - LittleFS.usedBytes();
      if (freeB < up.currentSize + 8192) uploadFail("Depolama dolu");
      else if (uploadBytes + up.currentSize > (size_t)MAX_UPLOAD_KB * 1024) uploadFail("Dosya cok buyuk");
      else {
        size_t wr = uploadFile.write(up.buf, up.currentSize);
        if (wr != up.currentSize) uploadFail("Yazma hatasi");
        uploadBytes += wr;
        netBytes = uploadBytes;
      }
    }
  }
  else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    if (uploadOk) {
      if (LittleFS.exists(uploadFinal)) LittleFS.remove(uploadFinal);
      if (LittleFS.rename(uploadTmp, uploadFinal)) netStatus = "Yuklendi: " + baseName(uploadFinal);
      else uploadFail("Adlandirma hatasi");
    }
    if (!uploadOk) {
      LittleFS.remove(uploadTmp);
      netStatus = "Hata: " + uploadErr;
    }
  }
  else if (up.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    LittleFS.remove(uploadTmp);
    uploadFail("Iptal edildi");
    netStatus = "Yukleme iptal";
  }
}

void handleUploadDone() {
  logEvent(String("web: yukleme ") + (uploadOk ? "tamam " : "HATA ") + baseName(uploadFinal) + " " + uploadErr + " " + String(uploadBytes / 1024) + "KB");
  String body = String("{\"ok\":") + (uploadOk ? "true" : "false") +
                ",\"error\":\"" + uploadErr + "\",\"name\":\"" + baseName(uploadFinal) + "\"}";
  sendJson(uploadOk ? 200 : 400, body);
}

// ---------------------- API işleyicileri ------------------------
void handleFile() {
  String f = server.arg("f");
  if (!safePath(f) || !LittleFS.exists(f)) { server.send(404, "text/plain", "yok"); return; }
  File file = LittleFS.open(f, "r");
  server.sendHeader("Cache-Control", "public, max-age=604800");   // tarayici onbellege alsin (karti mesgul etmesin)
  server.streamFile(file, "image/gif");
  file.close();
}

void handleSelect() {
  String f = server.arg("file");
  if (!safePath(f) || !LittleFS.exists(f)) { sendJson(400, "{\"ok\":false,\"error\":\"Dosya yok\"}"); return; }
  activeGifPath = f;
  gifLoaded = true;
  configSave();
  netStatus = "Aktif GIF: " + baseName(f);
  logEvent("web: GIF secildi " + baseName(f));
  sendJson(200, "{\"ok\":true}");
}

void handleDelete() {
  String f = server.arg("file");
  if (!safePath(f)) { sendJson(400, "{\"ok\":false,\"error\":\"Gecersiz yol\"}"); return; }
  if (isBuiltin(f)) { sendJson(403, "{\"ok\":false,\"error\":\"Sabit GIF silinemez\"}"); return; }
  if (!LittleFS.exists(f)) { sendJson(404, "{\"ok\":false,\"error\":\"Dosya yok\"}"); return; }
  LittleFS.remove(f);
  if (f == activeGifPath) {
    activeGifPath = firstExistingDefault();
    configSave();
  }
  netStatus = "Silindi: " + baseName(f);
  logEvent("web: GIF silindi " + baseName(f));
  sendJson(200, "{\"ok\":true}");
}

void handleCmd() {
  String c = server.arg("c");
  if      (c == "playpause") playPause();
  else if (c == "next")      trackChanged(CMD_NEXT);
  else if (c == "prev")      trackChanged(CMD_PREV);
  else if (c == "mute")      toggleMute();
  else if (c == "source")    switchSource();
  else if (c == "screen")    nextScreen();
  else { sendJson(400, "{\"ok\":false,\"error\":\"Bilinmeyen komut\"}"); return; }
  netStatus = "Komut: " + c;
  logEvent("web: komut " + c);
  sendJson(200, "{\"ok\":true}");
}

void handleVolumeApi() {
  int v = constrain(server.arg("v").toInt(), 0, volLimit);
  volume = v;
  lastVolUserMs = millis();
  if (muted) { muted = false; btSend(CMD_UNMUTE); }
  btSend(cmdVolume(volume));
  drawVolume();
  sendJson(200, "{\"ok\":true}");
}

void handleSaveVolume() {
  defaultVolume = volume;
  configSave();
  netStatus = "Acilis sesi kaydedildi";
  sendJson(200, "{\"ok\":true}");
}

void handleFactory() {
  for (int guard = 0; guard < 64; guard++) {         // /user içindeki her şeyi sil
    File dir = LittleFS.open("/user");
    if (!dir) break;
    File f = dir.openNextFile();
    if (!f) break;
    String p = "/user/" + baseName(String(f.name()));
    f.close();
    dir.close();
    LittleFS.remove(p);
  }
  activeGifPath = firstExistingDefault();
  configSave();
  netStatus = "Kullanici GIF'leri silindi";
  sendJson(200, "{\"ok\":true}");
}

void handleLog() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", readLogTail(6000));
}

void handleLogClear() {
  clearLog();
  logEvent("kayit temizlendi");
  sendJson(200, "{\"ok\":true}");
}

void handleWifiKeep() {
  wifiKeepOn = (server.arg("on") == "1");
  configSave();
  logEvent(String("Wi-Fi hep acik: ") + (wifiKeepOn ? "evet" : "hayir"));
  sendJson(200, "{\"ok\":true}");
}

void buildRoutes() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/state", HTTP_GET, []() { sendJson(200, stateJson()); });
  server.on("/api/gifs", HTTP_GET, []() { sendJson(200, listGifsJson()); });
  server.on("/api/file", HTTP_GET, handleFile);
  server.on("/api/select", HTTP_POST, handleSelect);
  server.on("/api/delete", HTTP_POST, handleDelete);
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/volume", HTTP_POST, handleVolumeApi);
  server.on("/api/savevolume", HTTP_POST, handleSaveVolume);
  server.on("/api/factory", HTTP_POST, handleFactory);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/logclear", HTTP_POST, handleLogClear);
  server.on("/api/wifikeep", HTTP_POST, handleWifiKeep);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "yok"); });
}

// ---------------------- Başlat / durdur / döngü -----------------
void netStart() {
  if (netActive) return;
  char ssid[40];
  snprintf(ssid, sizeof(ssid), "%s-%04X", deviceName.c_str(), (unsigned)(ESP.getEfuseMac() >> 32) & 0xFFFF);
  netSsid = ssid;

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(netSsid.c_str(), apPass.c_str());
  delay(100);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);     // akım tepelerini düşürür (güç düşmesi/brownout riskini azaltır)
  Serial.printf("Wi-Fi AP: %s  sifre: %s  IP: %s  (%s)\n", netSsid.c_str(), apPass.c_str(),
                WiFi.softAPIP().toString().c_str(), ok ? "OK" : "HATA");

  if (MDNS.begin("muzikcalar")) {                      // muzikcalar.local + uygulama için servis ilanı
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("muzikcalar", "tcp", 80);
    MDNS.addServiceTxt("muzikcalar", "tcp", "fw", "v6");
    MDNS.addServiceTxt("muzikcalar", "tcp", "name", deviceName.c_str());
  }
  if (!serverBuilt) { buildRoutes(); serverBuilt = true; }
  server.begin();
  netActive = true;
  netBytes = 0;
  netStatus = ok ? "Hazir, bekleniyor" : "Wi-Fi baslatilamadi";
  logEvent(String("Wi-Fi acildi: ") + netSsid + (ok ? " (OK)" : " (HATA)") + " heap=" + String(ESP.getFreeHeap()));
}

void netStop(const char* why) {
  if (!netActive) return;
  logEvent(String("Wi-Fi kapandi: ") + why);
  server.stop();
  MDNS.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  netActive = false;
  netClients = 0;
  Serial.println("Wi-Fi kapatildi.");
}

void netLoop() {
  if (!netActive) return;
  server.handleClient();
  netClients = WiFi.softAPgetStationNum();
}
