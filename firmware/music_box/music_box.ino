/*
  ESP32 + BT201 + ILI9341 (2.8" 320x240 yatay) + HW-040 Encoder  -  MÜZİK ÇALAR v7
  ---------------------------------------------------------------------------
  Kütüphaneler (Library Manager): Adafruit GFX, Adafruit ILI9341, AnimatedGIF
  (Adafruit BusIO bağımlılık olarak kurulur). LittleFS ESP32 paketinde hazır.

  BAĞLANTILAR
    Ekran : CLK=D18  MOSI=D23  DC=D2  RES=D4  BLK=3V3(220R)  VCC=3V3  GND  (CS yok)
    Encoder: CLK=D32  DT=D33  SW=D25  +=3V3  GND
    Butonlar (diğer uç GND): Önceki=D26  Sonraki=D27  Mod=D14
    BT201 : TX -> D16 (RX2),  RX <- D17 (TX2)

  KONTROLLER
    Encoder dönüşü      : Ses
    Encoder basma       : Oynat/Duraklat      (uzun bas: Sessiz)
    Önceki / Sonraki    : Şarkı değiştir
    Mod butonu          : Ekran modu  CALAN -> GIF -> DALGA -> TEST
                          (uzun bas: BT <-> SD kart)

  YENİ (v6): BAGLANTI ekranı  -> Wi-Fi açılır, telefondan GIF yükle / kumanda et.
    - Ayarlar: data/config.json   - Sabit GIF'ler: data/default/*.gif
    - Kullanıcı GIF'leri: /user/ klasörü (uygulamadan yüklenir)
    - Ek kütüphaneler: ArduinoJson (v7), QRCode (Richard Moore, isteğe bağlı)
*/

#include <Arduino.h>
#include "driver/rtc_io.h"
#include <SPI.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <AnimatedGIF.h>
#include <driver/gpio.h>
#include <math.h>
#include <new>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#define USE_QR 1                  // 0 yaparsan QR kod kullanilmaz (QRCode kutuphanesi gerekmez)
#if USE_QR
#include "qrcode_moore.h"         // QRCode kutuphanesinin proje icine kopyasi (karttaki qrcode.h ile cakismasin diye)
#endif

// ======================= PİN VE EKRAN AYARLARI =======================
#define TFT_DC     2
#define TFT_RST    4
#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_CS     -1          // modülde CS yok

#define BT_RX      16          // ESP32 RX <- BT201 TX
#define BT_TX      17          // ESP32 TX -> BT201 RX
#define BT_BAUD    115200

#define ENC_CLK    32
#define ENC_DT     33
#define ENC_SW     25
#define BTN_PREV   26
#define BTN_NEXT   27
#define BTN_MODE   14

#define SPI_HZ     20000000    // artefakt görürsen 16000000 yap
// Ekran test kodunda hangi ayar düzgündü, onu yaz (sadece 0 veya 2 dikey):
#define LONG_PRESS_MS 1200     // uzun basis suresi (ms): Mod = BT/SD, Encoder = kapat
#define TFT_ROTATION 1        // yatay: 1 veya 3 (USB yonu ters cikarsa digerini dene)
#define TFT_INVERT   true      // IPS panel: renkler ters cikiyordu (siyah=beyaz), bu yuzden true
#define TFT_BGR      1         // kırmızı-mavi ters çıkarsa 0 yap

#define ENC_DIR             1  // ses ters yöne gidiyorsa -1 yap
#define ENC_STEPS_PER_DETENT 4 // bazı encoderlarda 2 olur (tık başına 1 adım olmazsa dene)
// =====================================================================

// ================= BT201 AT KOMUTLARI =================
const char* CMD_PP_BT    = "AT+CB";
const char* CMD_PP_TF    = "AT+AA03";
const char* CMD_NEXT     = "AT+CC";
const char* CMD_PREV     = "AT+CD";
const char* CMD_MODE_BT  = "AT+CM01";
const char* CMD_MODE_TF  = "AT+CM03";
const char* CMD_MUTE     = "AT+CU01";
const char* CMD_UNMUTE   = "AT+CU00";
const char* SD_LOOP_CMD  = "AT+AC00";   // SD/USB: tum dosyalari surekli tekrarla (00=hepsi, 01=bir aygit, 02=tek sarki, 03=karisik)
String cmdVolume(int v) {
  char b[12];
  snprintf(b, sizeof(b), "AT+CA%02d", constrain(v, 0, 30));
  return String(b);
}
const int VOL_MAX = 30;
// ======================================================

#define SCREEN_W   320
#define SCREEN_H   240
#define CONTENT_H  200          // üst içerik alanı; altta ortak panel (200-240). GIF ekranı tam ekran

#define EQ_Y       134
#define EQ_H       64           // ekolayzir yuksekligi
#define CAT_H      100          // kedi agzi yuksekligi (ayni canvas, 320x100)
#define EQ_SEGS    8
#define EQ_BARS    32

class MyTFT : public Adafruit_ILI9341 {
 public:
  MyTFT(SPIClass* s, int8_t dc, int8_t cs, int8_t rst) : Adafruit_ILI9341(s, dc, cs, rst) {}
  void setMadctl(uint8_t v) { sendCommand(ILI9341_MADCTL, &v, 1); }
};

SPIClass tftSPI(VSPI);
MyTFT tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);
AnimatedGIF gif;
File gifFile;

#define SCR_PLAYING 0
#define SCR_GIF     1
#define SCR_WAVE    2
#define SCR_CAT     3
#define SCR_NET     4
const char* screenNames[] = {"CALAN", "GIF", "DALGA", "KEDI", "BAGLANTI"};
int screenMode = SCR_PLAYING;

bool gifLoaded = false;
String songGifPath = "";      // SD'de çalan şarkıya özel GIF (boşsa aktif GIF)
bool canvasOK  = true;
bool isPlaying = false;
bool muted     = false;
int  volume    = 15;
int  srcMode   = 1;              // 1=Bluetooth, 2=USB, 3=SD kart, 9=bekleme
int  requestedMode = 0;
bool wantSD = false;             // kullanici SD sectiyse true: modul kendi BT'ye atlarsa geri SD'ye al
int  sdRevertTries = 0;
unsigned long sdRevertAt = 0;
unsigned long queryModeAt = 0;
unsigned long elapsedMs = 0;
unsigned long trackLenSec = 0;
unsigned long lastTimeSec = 0xFFFFFFFF;
bool progressDirty = true;

// --- Ayarlar (config.json'dan okunur) ---
String activeGifPath = "/default/radyal.gif";
String deviceName = "MuzikCalar";
String apPass = "12345678";
int defaultVolume = 15;

// --- Ağ durumu (net.ino ile paylaşılır) ---
bool netActive = false;
int netClients = 0;
int netTextY = 34;
String netStatus = "Hazir";
String netSsid = "";
unsigned long netBytes = 0;

// --- Ses komutu yeniden deneme + reset nedeni ---
unsigned long lastVolSendMs = 0, volResendAt = 0;
int volRetries = 0;
String resetName = "-";
String lastQuiet = "";
unsigned long bootCount = 0;
bool wifiKeepOn = false;          // config: wifi_always_on
bool logReady = false;
unsigned long lastVolUserMs = 0;
unsigned long wifiStartAt = 0;
int volLimit = 20;               // config: max_volume (guc dusmesini azaltmak icin ses ust siniri)

String resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "Guc acildi";
    case ESP_RST_SW:       return "Yazilim reseti";
    case ESP_RST_PANIC:    return "Cokme (panic)";
    case ESP_RST_INT_WDT:  return "Watchdog (int)";
    case ESP_RST_TASK_WDT: return "Watchdog (task)";
    case ESP_RST_WDT:      return "Watchdog";
    case ESP_RST_BROWNOUT: return "GUC DUSMESI (brownout)";
    case ESP_RST_DEEPSLEEP:return "Deep sleep";
    default:               return "Bilinmiyor";
  }
}

String titleRaw = "";
String titleAscii = "Bekleniyor...";
String lastRx = "-";
String lastTx = "-";

// ---------------------- Canvas'lar (titremesiz çizim) --------
// Bellek bloklari program acildiktan sonra (setup icinde) ayrilir
GFXcanvas16* discCanvas  = nullptr;
GFXcanvas16* titleCanvas = nullptr;
GFXcanvas16* eqCanvas    = nullptr;

float eqLevel[EQ_BARS], eqTarget[EQ_BARS], eqPeak[EQ_BARS];
float discAngle = 0;
float beatPulse = 0, energy = 0;
unsigned long lastBeat = 0, beatInterval = 500;
int scrollX = 0;

// ---------------------- Encoder (kesme ile) ------------------
volatile int encSteps = 0;
volatile int8_t encAccum = 0;
volatile uint8_t encState = 0;
long encTotal = 0;
portMUX_TYPE encMux = portMUX_INITIALIZER_UNLOCKED;
DRAM_ATTR const int8_t ENC_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR encISR() {
  encState = ((encState << 2) | (gpio_get_level((gpio_num_t)ENC_CLK) << 1) |
              gpio_get_level((gpio_num_t)ENC_DT)) & 0x0F;
  encAccum += ENC_TABLE[encState];
  if (encAccum >= ENC_STEPS_PER_DETENT)       { encSteps += ENC_DIR; encAccum = 0; }
  else if (encAccum <= -ENC_STEPS_PER_DETENT) { encSteps -= ENC_DIR; encAccum = 0; }
}

// ---------------------- Yardımcılar --------------------------
uint16_t wheel(uint8_t p) {
  p = 255 - p;
  if (p < 85)  return tft.color565(255 - p * 3, 0, p * 3);
  if (p < 170) { p -= 85; return tft.color565(0, p * 3, 255 - p * 3); }
  p -= 170;
  return tft.color565(p * 3, 255 - p * 3, 0);
}

String fit(const String& s, int n) {
  String r = s;
  if ((int)r.length() > n) r = r.substring(0, n);
  while ((int)r.length() < n) r += ' ';
  return r;
}

String asciiFold(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];
    if (c < 0x80) {
      if (c >= 32 && c < 127) out += (char)c;
      continue;
    }
    uint8_t d = (i + 1 < in.length()) ? (uint8_t)in[i + 1] : 0;
    char r = 0;
    if (c >= 0xC0 && c < 0xE0) {
      if (c == 0xC3) {
        switch (d) {
          case 0xA7: r = 'c'; break; case 0x87: r = 'C'; break;
          case 0xB6: r = 'o'; break; case 0x96: r = 'O'; break;
          case 0xBC: r = 'u'; break; case 0x9C: r = 'U'; break;
        }
      } else if (c == 0xC4) {
        switch (d) {
          case 0x9F: r = 'g'; break; case 0x9E: r = 'G'; break;
          case 0xB1: r = 'i'; break; case 0xB0: r = 'I'; break;
        }
      } else if (c == 0xC5) {
        switch (d) {
          case 0x9F: r = 's'; break; case 0x9E: r = 'S'; break;
        }
      }
      i += 1;
    } else if (c >= 0xE0 && c < 0xF0) { i += 2; }
    else if (c >= 0xF0)               { i += 3; }
    if (r) out += r;
  }
  return out;
}

void btSend(const String& cmd) {
  Serial2.print(cmd);
  Serial2.print("\r\n");
  lastTx = asciiFold(cmd);
  Serial.println("BT> " + cmd);
}

void btSendQuiet(const char* cmd) {
  lastQuiet = cmd;
  Serial2.print(cmd);
  Serial2.print("\r\n");
}

void songGifCheck();
void setTitle(const String& s) {
  String a = asciiFold(s);
  if (a.length() == 0) a = "...";
  if (a == titleAscii) return;
  titleAscii = a;
  scrollX = 0;
  songGifCheck();
}

const char* srcLabel() {
  switch (srcMode) {
    case 1:  return "BLUETOOTH";
    case 2:  return "USB";
    case 3:  return "SD KART";
    default: return "BEKLEME";
  }
}

// ---------------------- GIF ----------------------------------
void* GIFOpenFile(const char* fname, int32_t* pSize) {
  gifFile = LittleFS.open(fname, "r");
  if (gifFile) { *pSize = gifFile.size(); return (void*)&gifFile; }
  return NULL;
}
void GIFCloseFile(void* pHandle) {
  File* f = (File*)pHandle;
  if (f) f->close();
}
int32_t GIFReadFile(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  File* f = (File*)pFile->fHandle;
  int32_t n = f->read(pBuf, iLen);
  pFile->iPos = f->position();
  return n;
}
int32_t GIFSeekFile(GIFFILE* pFile, int32_t iPosition) {
  File* f = (File*)pFile->fHandle;
  f->seek(iPosition);
  pFile->iPos = f->position();
  return pFile->iPos;
}
// Şeffaf pikselleri atlar (delta kareli GIF'ler için doğru çalışır)
void GIFDraw(GIFDRAW* pDraw) {
  int xOff = (SCREEN_W - gif.getCanvasWidth()) / 2;
  int yOff = (SCREEN_H - gif.getCanvasHeight()) / 2;
  if (xOff < 0) xOff = 0;
  if (yOff < 0) yOff = 0;
  int y = pDraw->iY + pDraw->y + yOff;
  if (y >= SCREEN_H) return;
  int w = pDraw->iWidth;
  int x = pDraw->iX + xOff;
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (w <= 0) return;

  static uint16_t line[SCREEN_W];
  uint8_t* s = pDraw->pPixels;
  uint16_t* pal = pDraw->pPalette;

  if (!pDraw->ucHasTransparency) {
    for (int i = 0; i < w; i++) line[i] = pal[s[i]];
    tft.drawRGBBitmap(x, y, line, w, 1);
    return;
  }
  int i = 0;
  while (i < w) {
    while (i < w && s[i] == pDraw->ucTransparent) i++;
    int start = i;
    while (i < w && s[i] != pDraw->ucTransparent) { line[i - start] = pal[s[i]]; i++; }
    if (i > start) tft.drawRGBBitmap(x + start, y, line, i - start, 1);
  }
}
// Acilis animasyonu: GIF'i bir kez oynat (bir tusa basilirsa atla)
bool introSkip() {
  return digitalRead(BTN_PREV) == LOW || digitalRead(BTN_NEXT) == LOW ||
         digitalRead(BTN_MODE) == LOW || digitalRead(ENC_SW) == LOW;
}
bool gifSkippable = true;      // kapanışta enkoder basılı olduğu için atlama kapalı
void playGifOnce(const char* path) {
  if (!LittleFS.exists(path)) return;
  if (!gif.open(path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) return;
  while (gif.playFrame(true, NULL) > 0) {
    yield();
    if (gifSkippable && introSkip()) break;
  }
  gif.close();
}
void gifStart() {
  if (!gifLoaded) return;
  gif.close();
  const String& gp = songGifPath.length() ? songGifPath : activeGifPath;
  if (!gif.open(gp.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    Serial.println("GIF acilamadi: " + gp);
    gifLoaded = false;
  }
}

// ---------------------- Alt panel (tüm modlarda ortak) -------
#define VOL_SEGS 20
void drawVolume() {
  int lit = muted ? 0 : constrain(volume, 0, VOL_SEGS);
  for (int i = 0; i < VOL_SEGS; i++) {                        // segmentli ses çubuğu: yeşil-sarı-kırmızı
    uint16_t col = (i >= lit) ? 0x2104 : (i < 12 ? 0x07E0 : (i < 17 ? 0xFFE0 : 0xF800));
    tft.fillRect(130 + i * 9, 225, 8, 13, col);
  }
  tft.fillRect(112, 224, 16, 16, ILI9341_BLACK);              // hoparlör simgesi
  tft.fillRect(114, 229, 4, 7, ILI9341_WHITE);
  tft.fillTriangle(118, 229, 124, 225, 124, 239, ILI9341_WHITE);
  tft.fillTriangle(118, 229, 118, 235, 124, 239, ILI9341_WHITE);
  if (muted) {
    tft.drawLine(113, 226, 125, 238, ILI9341_RED);
    tft.drawLine(113, 238, 125, 226, ILI9341_RED);
  }
}

void drawStatusRow() {
  tft.fillRect(0, 224, 112, 16, ILI9341_BLACK);
  if (isPlaying) tft.fillTriangle(8, 226, 8, 237, 18, 231, ILI9341_GREEN);   // oynuyor
  else { tft.fillRect(8, 226, 4, 11, ILI9341_RED); tft.fillRect(14, 226, 4, 11, ILI9341_RED); }   // duraklatıldı
  const char* src = srcMode == 1 ? "BT" : (srcMode == 3 ? "SD" : (srcMode == 2 ? "USB" : "--"));
  int bw = strlen(src) * 6 + 10;                              // kaynak rozeti
  tft.fillRoundRect(26, 225, bw, 13, 3, ILI9341_CYAN);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK, ILI9341_CYAN);
  tft.setCursor(31, 228);
  tft.print(src);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(26 + bw + 6, 228);
  tft.print(fit(screenNames[screenMode], 8));
}

void drawBottomStatic() {
  tft.fillRect(0, CONTENT_H, SCREEN_W, SCREEN_H - CONTENT_H, ILI9341_BLACK);
  tft.drawFastHLine(0, CONTENT_H, SCREEN_W, ILI9341_CYAN);
  drawStatusRow();
  drawVolume();
}

// Şarkı adı: sığmazsa kayar
void drawTitleBar() {
  if (!canvasOK) return;
  titleCanvas->fillScreen(ILI9341_BLACK);
  titleCanvas->setTextSize(2);
  titleCanvas->setTextColor(ILI9341_WHITE);
  titleCanvas->setTextWrap(false);
  int w = titleAscii.length() * 12;
  if (w <= SCREEN_W) {
    titleCanvas->setCursor((SCREEN_W - w) / 2, 1);
    titleCanvas->print(titleAscii);
  } else {
    int cycle = w + 48;
    titleCanvas->setCursor(-scrollX, 1);
    titleCanvas->print(titleAscii);
    titleCanvas->setCursor(-scrollX + cycle, 1);
    titleCanvas->print(titleAscii);
    if (isPlaying) scrollX = (scrollX + 2) % cycle;
  }
  tft.drawRGBBitmap(0, 203, titleCanvas->getBuffer(), 320, 18);
}

// ---------------------- Animasyon motoru ---------------------
void updateBeat() {
  unsigned long now = millis();
  bool active = isPlaying && !muted;
  if (active && now - lastBeat > beatInterval) {
    lastBeat = now;
    beatInterval = random(380, 640);
    beatPulse = 1.0f;
  }
  beatPulse *= active ? 0.86f : 0.7f;
  float target = active ? (0.25f + 0.75f * beatPulse + random(0, 20) / 100.0f) : 0.0f;
  energy += (target - energy) * 0.25f;
}

// ---------------------- "Çalan" ekranı -----------------------
void drawPlayingStatic() {
  tft.fillRect(0, 0, SCREEN_W, CONTENT_H, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(122, 8);
  tft.print("SURE");
  tft.drawRect(8, 120, 304, 8, 0x4208);       // ilerleme çubuğu çerçevesi
  lastTimeSec = 0xFFFFFFFF;
  progressDirty = true;
}

void drawTime() {
  unsigned long sec = elapsedMs / 1000;
  if (sec == lastTimeSec) return;
  lastTimeSec = sec;
  progressDirty = true;
  char b[24];
  snprintf(b, sizeof(b), "%02lu:%02lu", (sec / 60) % 100, sec % 60);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(122, 20);
  tft.print(b);

  tft.setTextSize(2);
  tft.setTextColor(0x7BEF, ILI9341_BLACK);
  tft.setCursor(122, 52);
  if (trackLenSec > 0) snprintf(b, sizeof(b), "/%02lu:%02lu ", (trackLenSec / 60) % 100, trackLenSec % 60);
  else                 snprintf(b, sizeof(b), "--:--  ");
  tft.print(b);

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(122, 78);
  tft.print(fit(srcLabel(), 12));
}

void drawProgress() {
  const int X = 8, Y = 120, W = 304, H = 8;
  unsigned long sec = elapsedMs / 1000;
  bool active = isPlaying && !muted;
  if (trackLenSec > 0) {
    static unsigned long lastS = 0xFFFFFFFF;
    if (sec == lastS && !progressDirty) return;
    lastS = sec;
    progressDirty = false;
    int w = constrain((int)(sec * (W - 2) / trackLenSec), 0, W - 2);
    tft.fillRect(X + 1, Y + 1, w, H - 2, ILI9341_CYAN);
    tft.fillRect(X + 1 + w, Y + 1, W - 2 - w, H - 2, 0x1082);
  } else {
    tft.fillRect(X + 1, Y + 1, W - 2, H - 2, 0x1082);   // süre bilinmiyorsa kayan ışık
    if (active) {
      int pos = (int)((millis() / 6) % (W + 60)) - 30;
      int x0 = max(pos, 0), x1 = min(pos + 30, W - 2);
      if (x1 > x0) tft.fillRect(X + 1 + x0, Y + 1, x1 - x0, H - 2, wheel((millis() / 12) & 255));
    }
  }
}

// Albüm kapağı yerine dönen, ritimle nefes alan plak
void drawDisc() {
  if (!canvasOK) return;
  bool active = isPlaying && !muted;
  if (active) discAngle += 0.20f;
  const int C = 52;
  discCanvas->fillScreen(ILI9341_BLACK);
  if (active) {
    int ring = 47 + (int)(beatPulse * 4);
    discCanvas->drawCircle(C, C, ring, wheel((millis() / 10) & 255));
  }
  discCanvas->fillCircle(C, C, 44, 0x2104);
  discCanvas->drawCircle(C, C, 40, 0x39E7);
  discCanvas->drawCircle(C, C, 35, 0x39E7);
  discCanvas->drawCircle(C, C, 30, 0x39E7);
  discCanvas->drawCircle(C, C, 25, 0x39E7);
  discCanvas->fillCircle(C, C, 18, wheel((millis() / 15) & 255));
  for (int k = 0; k < 2; k++) {
    float a = discAngle + k * PI;
    for (int j = 0; j < 2; j++) {
      float aa = a + j * 0.07f;
      discCanvas->drawLine(C + (int)(cosf(aa) * 22), C + (int)(sinf(aa) * 22),
                          C + (int)(cosf(aa) * 43), C + (int)(sinf(aa) * 43), 0x8410);
    }
  }
  discCanvas->fillCircle(C + (int)(cosf(discAngle) * 10), C + (int)(sinf(discAngle) * 10), 2, ILI9341_WHITE);
  discCanvas->fillCircle(C, C, 3, ILI9341_BLACK);
  tft.drawRGBBitmap(8, 8, discCanvas->getBuffer(), 104, 104);
}

// Segmentli ekolayzır (gerçek ses analizi DEĞİL, ritimli animasyon)
void drawEqualizer() {
  if (!canvasOK) return;
  bool active = isPlaying && !muted;
  for (int i = 0; i < EQ_BARS; i++) {
    if (active) {
      if (random(100) < 26) {
        float base = random(18, 100) / 100.0f;
        float boost = (i < 7) ? (0.55f + 0.45f * beatPulse)
                              : (0.35f + 0.65f * (1.0f - (float)i / EQ_BARS));
        eqTarget[i] = base * boost * EQ_SEGS;
      }
      if (beatPulse > 0.8f && i < 7) eqTarget[i] = EQ_SEGS * (0.7f + random(30) / 100.0f);
      eqLevel[i] += (eqTarget[i] - eqLevel[i]) * 0.45f;
    } else {
      eqTarget[i] = 0;
      eqLevel[i] += (0 - eqLevel[i]) * 0.15f;
    }
    if (eqLevel[i] > eqPeak[i]) eqPeak[i] = eqLevel[i];
    else eqPeak[i] -= 0.12f;
    if (eqPeak[i] < 0) eqPeak[i] = 0;
  }
  eqCanvas->fillScreen(ILI9341_BLACK);
  for (int i = 0; i < EQ_BARS; i++) {
    int n = constrain((int)(eqLevel[i] + 0.5f), 0, EQ_SEGS);
    int x = i * 10 + 1;
    for (int s = 0; s < n; s++) {
      uint16_t col = (s < 5) ? 0x07E0 : (s < 7 ? 0xFFE0 : 0xF800);
      eqCanvas->fillRect(x, EQ_H - (s + 1) * 8, 8, 6, col);
    }
    int p = constrain((int)eqPeak[i], 0, EQ_SEGS - 1);
    if (eqPeak[i] > 0.5f) eqCanvas->fillRect(x, EQ_H - (p + 1) * 8, 8, 2, ILI9341_WHITE);
  }
  tft.drawRGBBitmap(0, EQ_Y, eqCanvas->getBuffer(), 320, EQ_H);
}

// ---------------------- "Dalga" ekranı -----------------------
#define WAVE_N 81
int16_t wavePrev[3][WAVE_N];
bool waveFresh = true;
float wavePhase[3] = {0.0f, 1.7f, 3.4f};

void strokeWave(int k, int16_t* arr, uint16_t col) {
  for (int i = 0; i < WAVE_N - 1; i++) {
    tft.drawLine(i * 4, arr[i], (i + 1) * 4, arr[i + 1], col);
    tft.drawLine(i * 4, arr[i] + 1, (i + 1) * 4, arr[i + 1] + 1, col);
  }
}

void drawWave() {
  const int CY = 100;
  bool active = isPlaying && !muted;
  if (waveFresh) {
    for (int k = 0; k < 3; k++) for (int i = 0; i < WAVE_N; i++) wavePrev[k][i] = CY;
    waveFresh = false;
  }
  int16_t cur[3][WAVE_N];
  for (int k = 0; k < 3; k++) {
    wavePhase[k] += (0.10f + 0.05f * k) * (active ? 1.0f : 0.2f);
    float amp = 80.0f * energy * (1.0f - 0.28f * k);
    for (int i = 0; i < WAVE_N; i++) {
      float x = i * 4;
      float env = 0.6f + 0.4f * sinf(x * 0.02f + wavePhase[0] * 0.5f);
      cur[k][i] = CY + (int)(sinf(x * (0.030f + 0.011f * k) + wavePhase[k]) * amp * env);
    }
  }
  for (int k = 0; k < 3; k++) strokeWave(k, wavePrev[k], ILI9341_BLACK);
  for (int k = 0; k < 3; k++) {
    strokeWave(k, cur[k], wheel(((millis() / 12) + k * 85) & 255));
    for (int i = 0; i < WAVE_N; i++) wavePrev[k][i] = cur[k][i];
  }
}

// ---------------------- "Kedi" ekranı: ekran = kedinin ağzı ----
// Ağız, müziğin enerjisine göre açılıp kapanır (ekolayzırla aynı canvas'ı kullanır).
void drawCatStatic() {
  tft.fillRect(0, 0, SCREEN_W, CONTENT_H, ILI9341_BLACK);
}

void drawCatFrame() {
  if (!canvasOK) return;
  static float b = 3;
  bool active = isPlaying && !muted;
  float tgt = active ? 6 + 40 * energy : 3;
  b += (tgt - b) * 0.5f;
  int bi = constrain((int)(b + 0.5f), 3, CAT_H / 2 - 2);
  const int cy = CAT_H / 2, A = 150;
  eqCanvas->fillScreen(ILI9341_BLACK);
  for (int dy = -bi; dy <= bi; dy++) {
    float r = sqrtf(1.0f - (float)(dy * dy) / (float)(bi * bi));
    int w = (int)(A * r);
    eqCanvas->drawFastHLine(160 - w, cy + dy, 2 * w, 0x7800);          // koyu kırmızı iç
    float ty = (dy - bi * 0.55f) / (bi * 0.45f);                       // dil (alt yarı)
    if (bi > 10 && ty > -1 && ty < 1) {
      int tw = min(w, (int)(60 * sqrtf(1.0f - ty * ty)));
      if (tw > 0) eqCanvas->drawFastHLine(160 - tw, cy + dy, 2 * tw, 0xFC10);   // pembe
    }
  }
  if (bi > 10) {                                                       // iki köpek diş
    int ytop = cy - (int)(bi * 0.866f);
    for (int t = 0; t < 12; t++) {
      int tw = 6 - t / 2;
      if (tw > 0) {
        eqCanvas->drawFastHLine(160 - 75 - tw, ytop + t, 2 * tw, ILI9341_WHITE);
        eqCanvas->drawFastHLine(160 + 75 - tw, ytop + t, 2 * tw, ILI9341_WHITE);
      }
    }
  }
  tft.drawRGBBitmap(0, (CONTENT_H - CAT_H) / 2, eqCanvas->getBuffer(), 320, CAT_H);
}

// ---------------------- Bağlantı ekranı ----------------------
void drawNetStatic() {
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(82, 4);
  tft.print("BAGLANTI MODU");
  int tx = 4, ty = 32;
  netTextY = 160;
#if USE_QR
  char payload[96];
  snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;", netSsid.c_str(), apPass.c_str());
  if (strlen(payload) < 72) {
    static QRCode qrcode;
    static uint8_t qrData[512];
    qrcode_initText(&qrcode, qrData, 4, ECC_LOW, payload);
    const int mod = 3;
    int total = (qrcode.size + 8) * mod;
    tft.fillRect(4, 28, total, total, ILI9341_WHITE);
    for (int yy = 0; yy < qrcode.size; yy++)
      for (int xx = 0; xx < qrcode.size; xx++)
        if (qrcode_getModule(&qrcode, xx, yy))
          tft.fillRect(4 + (xx + 4) * mod, 28 + (yy + 4) * mod, mod, mod, ILI9341_BLACK);
    tx = 4 + total + 8;                    // metin QR'in sagina
    netTextY = 28 + total + 6;
  }
#endif
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(tx, ty);      tft.print("1) Wi-Fi'ye baglan (QR):");
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(tx + 8, ty + 12); tft.print(netSsid);
  tft.setCursor(tx + 8, ty + 24); tft.print("sifre: " + apPass);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(tx, ty + 46); tft.print("2) Tarayici:");
  tft.setCursor(tx + 8, ty + 58); tft.print("http://" + WiFi.softAPIP().toString());
}

void drawNetFrame() {
  char b[48];
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  snprintf(b, sizeof(b), "Bagli cihaz: %d   Alinan: %lu KB   ", netClients, netBytes / 1024);
  tft.setCursor(4, netTextY);      tft.print(b);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(4, netTextY + 11); tft.print("Durum: " + fit(netStatus, 32));
  String gname = activeGifPath;
  int sl = gname.lastIndexOf('/');
  if (sl >= 0) gname = gname.substring(sl + 1);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(4, netTextY + 22); tft.print("Aktif GIF: " + fit(gname, 26));
}

// ---------------------- Ekran geçişi -------------------------
void setScreen(int s) {
  if (screenMode == SCR_NET && s != SCR_NET && !wifiKeepOn) netStop("ekran degisti");   // Wi-Fi'yi kapat (ses bozulmasın)
  screenMode = s;
  logEvent(String("ekran: ") + screenNames[s]);
  tft.fillRect(0, 0, SCREEN_W, (s == SCR_GIF) ? SCREEN_H : CONTENT_H, ILI9341_BLACK);
  if (s == SCR_PLAYING) drawPlayingStatic();
  if (s == SCR_GIF)     gifStart();
  if (s == SCR_WAVE)    waveFresh = true;
  if (s == SCR_CAT)     drawCatStatic();
  if (s == SCR_NET)     { netStart(); drawNetStatic(); }
  if (s != SCR_GIF) drawBottomStatic();                  // GIF ekrani tam ekran: alt panel yok
}

const int SCREEN_ORDER[4] = {SCR_CAT, SCR_GIF, SCR_PLAYING, SCR_NET};
void nextScreen() {
  int i = 0;
  while (i < 3 && SCREEN_ORDER[i] != screenMode) i++;
  int n = SCREEN_ORDER[(i + 1) % 4];
  if (n == SCR_GIF && !gifLoaded) n = SCREEN_ORDER[(i + 2) % 4];
  setScreen(n);
}

// SD şarkı adına göre özel GIF: eşleşirse tam ekran aç, şarkı değişince KEDI'ye dön
void songGifCheck() {
  String t = titleAscii;
  t.toLowerCase();
  String p = "";
  if (t.indexOf("kuzey") >= 0) p = "/default/kuzey.gif";
  else if (t.indexOf("kanunsuz") >= 0) p = "/default/kanunsuz.gif";
  if (srcMode != 3 || requestedMode) p = "";             // sadece SD'de ve mod degisirken degil
  if (p.length() && !LittleFS.exists(p)) p = "";
  if (p == songGifPath) return;
  songGifPath = p;
  if (p.length()) {
    gifLoaded = true;
    setScreen(SCR_GIF);
  } else {
    gif.close();
    gifLoaded = LittleFS.exists(activeGifPath);
    if (screenMode == SCR_GIF) setScreen(SCR_CAT);
  }
}

// ---------------------- Eylemler -----------------------------
void playPause() {
  btSend(srcMode == 1 ? CMD_PP_BT : CMD_PP_TF);
  isPlaying = !isPlaying;       // modül durumu yarım saniye içinde doğrular
  drawStatusRow();
}
// Kapanış: müziği durdur, kapanış animasyonunu oynat, ekranı kapat, deep sleep (enkoder tuşu uyandırır)
RTC_DATA_ATTR bool offWake = false;   // hafif uykudan uyandıktan sonra açılış animasyonu için
void powerOff() {
  logEvent("KAPANIS");
  if (isPlaying) btSend(srcMode == 1 ? CMD_PP_BT : CMD_PP_TF);
  gifSkippable = false;
  playGifOnce("/default/outro.gif");
  while (digitalRead(ENC_SW) == LOW) delay(10);           // tuş bırakılsın
  delay(300);
  rtc_gpio_pullup_en((gpio_num_t)ENC_SW);
  rtc_gpio_pulldown_dis((gpio_num_t)ENC_SW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)ENC_SW, 0);    // tuşa basınca (LOW) uyan
  if (netActive) netStop("kapanis");
  WiFi.mode(WIFI_OFF);
  offWake = true;
  esp_light_sleep_start();                                // hafif uyku: pinler korunur, son kare ekranda kalır
  ESP.restart();                                          // tuşla uyanınca baştan başla (açılış animasyonu oynar)
}
void toggleMute() {
  muted = !muted;
  btSend(muted ? CMD_MUTE : CMD_UNMUTE);
  drawVolume();
}
void switchSource() {
  requestedMode = (srcMode == 1) ? 3 : 1;     // BT <-> SD kart
  wantSD = (requestedMode == 3);
  sdRevertTries = 0;
  btSend(requestedMode == 3 ? CMD_MODE_TF : CMD_MODE_BT);
  setTitle("Mod degisiyor...");   // songGifCheck requestedMode'a bakip sarki GIF'ini kapatir
  queryModeAt = millis() + 800;
}
void trackChanged(const char* cmd) {
  btSend(cmd);
  elapsedMs = 0; trackLenSec = 0; titleRaw = "";
  lastTimeSec = 0xFFFFFFFF;
  progressDirty = true;
}

// ---------------------- Butonlar -----------------------------
const uint8_t BTN_PINS[4] = {BTN_PREV, BTN_NEXT, BTN_MODE, ENC_SW};
bool btnState[4] = {true, true, true, true};
unsigned long btnChange[4] = {0, 0, 0, 0};
unsigned long btnPressT[4] = {0, 0, 0, 0};
bool btnRaw[4] = {true, true, true, true};
unsigned long btnRawT[4] = {0, 0, 0, 0};
bool btnLongFired[4] = {false, false, false, false};

void handleButton(int idx, bool longPress) {
  switch (idx) {
    case 0: trackChanged(CMD_PREV); break;
    case 1: trackChanged(CMD_NEXT); break;
    case 2: if (longPress) switchSource(); else nextScreen(); break;
    case 3: if (longPress) powerOff();      else playPause();  break;
  }
}

void readButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    bool r = digitalRead(BTN_PINS[i]);
    bool longCapable = (i >= 2);
    if (r != btnRaw[i]) { btnRaw[i] = r; btnRawT[i] = now; }       // ham okuma degisti: sayaci sifirla
    bool s = btnState[i];
    if (r != btnState[i] && now - btnRawT[i] > 35) {                // 35 ms kararli ise gecerli say (parazit filtresi)
      s = r;
      btnChange[i] = now;
      btnState[i] = s;
      if (s == LOW) {
        btnPressT[i] = now;
        btnLongFired[i] = false;
        Serial.printf("BTN %d (GPIO %d) basildi\n", i, BTN_PINS[i]);
        logEvent("BTN " + String(i) + " (GPIO " + String(BTN_PINS[i]) + ")");
        if (!longCapable) handleButton(i, false);
      } else if (longCapable && !btnLongFired[i]) {
        handleButton(i, false);          // kısa basış: bırakınca
      }
    }
    if (longCapable && r == LOW && btnState[i] == LOW && !btnLongFired[i] && now - btnPressT[i] > LONG_PRESS_MS) {   // r==LOW: buton ancak GERCEKTEN basiliysa uzun basis say
      btnLongFired[i] = true;
      handleButton(i, true);             // uzun basış
    }
  }
}

// ---------------------- Ses (encoder) ------------------------
void handleVolume() {
  static unsigned long lastSend = 0, lastChangeT = 0;
  static bool dirty = true, confirmPending = false;     // açılışta ses senkronlanır
  int steps;
  portENTER_CRITICAL(&encMux);
  steps = encSteps;
  encSteps = 0;
  portEXIT_CRITICAL(&encMux);
  unsigned long now = millis();

  if (steps != 0) {
    encTotal += steps;
    lastVolUserMs = now;
    int nv = constrain(volume + steps, 0, volLimit);
    if (nv != volume || muted) {
      volume = nv;
      if (muted) { muted = false; btSend(CMD_UNMUTE); }
      dirty = true; lastChangeT = now;
      drawVolume();
    }
  }
  if (dirty && now > 1500 && now - lastSend >= 60) {
    btSend(cmdVolume(volume));
    dirty = false; lastSend = now; confirmPending = true;
    lastVolSendMs = now; volRetries = 0;
  }
  if (confirmPending && !dirty && now - lastChangeT > 400) {
    btSend(cmdVolume(volume));           // son değeri garantiye al
    confirmPending = false;
    lastVolSendMs = now;
    logEvent("ses: " + String(volume));
  }
  if (volResendAt && now >= volResendAt) {   // modül komutu reddettiyse (ER+) tekrar dene
    volResendAt = 0;
    btSend(cmdVolume(volume));
    lastVolSendMs = now;
  }
}

// ---------------------- BT201'den gelen veri -----------------
void setPlaying(bool p) {
  if (p != isPlaying) {
    isPlaying = p;
    drawStatusRow();
    logEvent(String("calma: ") + (p ? "basladi" : "durdu") + " (" + String(elapsedMs / 1000) + "/" + String(trackLenSec) + " sn)");
  }
}

void handleBTLine(const String& line) {
  lastRx = asciiFold(line);

  if (line.startsWith("ER+")) {
    static unsigned long lastErLog = 0;
    if (millis() - lastErLog > 1500) {                 // ayni anda gelen hata yagmurunu tek satira indir
      lastErLog = millis();
      logEvent("BT201 hata: " + line + " (komut: " + lastTx + ", sorgu: " + lastQuiet + ")");
    }
  }
  if (line.startsWith("ER+") && millis() - lastVolSendMs < 1500 && volRetries < 3) {
    volRetries++;                               // ses komutu reddedildi: 1,5 sn sonra tekrar
    volResendAt = millis() + 1500;
  }

  if (line.startsWith("QM+")) {                 // çalışma modu
    int m = line.substring(3).toInt();
    if (m != srcMode) {
      srcMode = m;
      logEvent("BT201 mod: QM+" + String(m));
      if (m == 3 || m == 2) btSendQuiet(SD_LOOP_CMD);          // SD/USB: surekli tekrar
      elapsedMs = 0; trackLenSec = 0; titleRaw = "";
      lastTimeSec = 0xFFFFFFFF;
      setTitle(m == 1 ? "Bluetooth" : (m == 9 ? "Bekleme" : "Yukleniyor..."));
      drawStatusRow();
    }
    if (requestedMode && m != requestedMode) { setTitle("SD kart yok?"); wantSD = false; }
    else if (m == 3) sdRevertTries = 0;
    else if (wantSD && m == 1 && !requestedMode && sdRevertTries < 3 && millis() - sdRevertAt > 5000) {
      sdRevertTries++; sdRevertAt = millis();                 // telefon bildirimi vb. modulu BT'ye atti
      logEvent("SD'ye geri donuluyor (" + String(sdRevertTries) + ")");
      btSend(CMD_MODE_TF);
      queryModeAt = millis() + 800;
    }
    requestedMode = 0;
  }
  else if (line.startsWith("QA+")) {            // modulun gercek ses seviyesi
    int qv = constrain(line.substring(3).toInt(), 0, VOL_MAX);
    unsigned long nowQ = millis();
    if (qv > volLimit) {
      volume = volLimit; volResendAt = nowQ + 200; drawVolume();
      logEvent("ses siniri: " + String(qv) + " -> " + String(volLimit));
    } else if (qv != volume && !muted && nowQ - lastVolUserMs > 4000 && nowQ - lastVolSendMs > 4000) {
      volume = qv;
      drawVolume();
      logEvent("ses moduldan alindi: " + String(qv));
    }
  }
  else if (line.startsWith("MP+")) {            // SD/USB oynatma durumu
    setPlaying(line.substring(3).toInt() == 1);
  }
  else if (line.startsWith("TS+")) {            // Bluetooth durumu
    int v = line.substring(3).toInt();
    static int lastTs = -1;
    if (v != lastTs) { lastTs = v; logEvent("BT durumu: TS+" + String(v)); }
    if (srcMode == 1) {
      setPlaying(v == 2);
      setTitle(v == 0 ? "Eslesme bekliyor" : (v == 1 ? "Bagli" : (v == 2 ? "Telefondan caliyor" : "Arama")));
    }
  }
  else if (line.startsWith("MK+")) {            // geçen süre (hex saniye)
    elapsedMs = strtoul(line.substring(3).c_str(), NULL, 16) * 1000UL;
  }
  else if (line.startsWith("MT+")) {            // toplam süre (hex saniye)
    trackLenSec = strtoul(line.substring(3).c_str(), NULL, 16);
    lastTimeSec = 0xFFFFFFFF;
    progressDirty = true;
  }
  else if (line.startsWith("MF+")) {            // çalan dosya adı
    String name = line.substring(3);
    name.trim();
    if (name.startsWith("/")) name.remove(0, 1);
    int dot = name.lastIndexOf('.');
    if (dot > 0 && (int)name.length() - dot <= 5) name.remove(dot);
    else {                                          // 8.3 biçimi: "003     MP3" -> "003"
      String up = name;
      up.toUpperCase();
      if (up.endsWith("MP3") || up.endsWith("WAV") || up.endsWith("WMA")) name.remove(name.length() - 3);
      name.trim();
    }
    if (name != titleRaw) {
      titleRaw = name;
      setTitle(name);
      btSendQuiet("AT+MT");
    }
  }
}

void readBT() {
  static String buf;
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || c == '\r') {
      buf.trim();
      if (buf.length() > 0) {
        Serial.println("BT< " + buf);
        handleBTLine(buf);
      }
      buf = "";
    } else {
      buf += c;
      if (buf.length() > 120) buf = "";
    }
  }
}

void pollBT() {
  static unsigned long last = 0;
  static uint8_t step = 0;
  unsigned long now = millis();
  if (now < 1500) return;
  if (queryModeAt && now >= queryModeAt) {
    btSendQuiet("AT+QM");
    queryModeAt = 0;
  }
  if (now - last < 350) return;
  last = now;
  if (srcMode == 1) {
    btSendQuiet((step % 4 == 0) ? "AT+QM" : ((step % 4 == 2) ? "AT+QA" : "AT+TS"));
  } else {
    static const char* seq[8] = {"AT+MP", "AT+MK", "AT+MF", "AT+MK", "AT+MP", "AT+MT", "AT+QM", "AT+QA"};
    btSendQuiet(seq[step % 8]);
  }
  step++;
}

// ---------------------- SETUP / LOOP -------------------------
void setup() {
  Serial.begin(115200);
  resetName = resetReasonName();
  Serial.println("Reset nedeni: " + resetName);
  Serial2.begin(BT_BAUD, SERIAL_8N1, BT_RX, BT_TX);
  randomSeed(micros());

  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(ENC_SW,   INPUT_PULLUP);
  pinMode(ENC_CLK,  INPUT_PULLUP);
  pinMode(ENC_DT,   INPUT_PULLUP);
  encState = ((gpio_get_level((gpio_num_t)ENC_CLK) << 1) | gpio_get_level((gpio_num_t)ENC_DT)) & 0x03;
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT),  encISR, CHANGE);

  for (int i = 0; i < EQ_BARS; i++) { eqLevel[i] = 0; eqTarget[i] = 0; eqPeak[i] = 0; }

  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
  tft.begin(SPI_HZ);
  tft.setRotation(TFT_ROTATION);
  {
    static const uint8_t base[4] = {0x40, 0x20, 0x80, 0xE0};
    tft.setMadctl(base[TFT_ROTATION] | (TFT_BGR ? 0x08 : 0x00));
  }
  tft.invertDisplay(TFT_INVERT);

  Serial.printf("Canvas oncesi heap: %u B, en buyuk blok: %u B\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  eqCanvas    = new (std::nothrow) GFXcanvas16(320, CAT_H);
  discCanvas  = new (std::nothrow) GFXcanvas16(104, 104);
  titleCanvas = new (std::nothrow) GFXcanvas16(320, 18);
  canvasOK = eqCanvas && discCanvas && titleCanvas &&
             eqCanvas->getBuffer() && discCanvas->getBuffer() && titleCanvas->getBuffer();
  Serial.printf("Canvas: %s | eq=%p disc=%p title=%p | sonrasi heap: %u B, en buyuk blok: %u B\n",
                canvasOK ? "OK" : "HATA", eqCanvas, discCanvas, titleCanvas,
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  tft.fillScreen(ILI9341_BLACK);

  if (LittleFS.begin(true)) {
    configLoad();                         // net.ino: config.json okur (yoksa fabrika ayarini yazar)
    volume = defaultVolume;
    logInit();                            // net.ino: kara kutu kaydi + acilis sayaci
    logEvent("ACILIS: reset=" + resetName + " heap=" + String(ESP.getFreeHeap()) + " gif=" + activeGifPath);
    gif.begin(LITTLE_ENDIAN_PIXELS);
    gifLoaded = LittleFS.exists(activeGifPath);
    Serial.printf("Aktif GIF: %s (%s)\n", activeGifPath.c_str(), gifLoaded ? "var" : "YOK");
  } else {
    Serial.println("LittleFS baslatilamadi.");
  }

  esp_reset_reason_t rr = esp_reset_reason();
  bool fromOff = offWake; offWake = false;
  if (rr == ESP_RST_DEEPSLEEP || fromOff) { while (digitalRead(ENC_SW) == LOW) delay(10); delay(200); }   // uyandıran tuş bırakılsın
  if ((rr == ESP_RST_POWERON || rr == ESP_RST_DEEPSLEEP || fromOff) && !introSkip()) {   // sadece normal acilista (reset dongusunde tekrarlanmasin)
    tft.fillScreen(ILI9341_BLACK);
    playGifOnce("/default/intro1.gif");
  }
  setScreen(SCR_CAT);
  if (wifiKeepOn) {
    if (esp_reset_reason() == ESP_RST_BROWNOUT) logEvent("Wi-Fi acilista atlandi: onceki reset guc dusmesiydi");
    else wifiStartAt = millis() + 10000;     // 10 sn sonra baslat (acilis akim tepesinden kacin)
  }             // config: wifi_always_on
  Serial.println("Hazir. Serial Monitor'dan yazdigin komutlar BT201'e gider.");
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastTick = 0, lastFrame = 0;
  if (isPlaying) elapsedMs += now - lastTick;
  lastTick = now;

  readButtons();
  handleVolume();
  readBT();
  pollBT();
  netLoop();
  if (wifiStartAt && millis() >= wifiStartAt) { wifiStartAt = 0; if (!netActive) netStart(); }

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.startsWith("!screen ")) {                       // test komutu: !screen 0..4
      int n = constrain(s.substring(8).toInt(), 0, 4);
      setScreen(n);
      Serial.printf("Ekran: %s | heap: %u B | en buyuk blok: %u B\n", screenNames[n],
                    (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    } else if (s == "!log") {
      Serial.println(readLogTail(6000));
    } else if (s == "!state") {
      Serial.printf("ekran=%s gif=%s(%s) vol=%d muted=%d src=%d playing=%d net=%d heap=%u\n",
                    screenNames[screenMode], activeGifPath.c_str(), gifLoaded ? "var" : "YOK",
                    volume, muted, srcMode, isPlaying, netActive, (unsigned)ESP.getFreeHeap());
    } else if (s.length()) btSend(s);
  }

  if (songGifPath.length() && srcMode != 3 && !requestedMode) songGifCheck();   // SD'den cikildiysa sarki GIF'ini birak
  if (screenMode == SCR_GIF) {
    if (gifLoaded && gif.playFrame(true, NULL) == 0) gif.reset();
  } else if (screenMode == SCR_NET) {
    if (now - lastFrame >= 250) { lastFrame = now; drawNetFrame(); }
  } else if (now - lastFrame >= 40) {
    lastFrame = now;
    updateBeat();
    if (screenMode == SCR_PLAYING) {
      drawTime();
      drawProgress();
      drawDisc();
      drawEqualizer();
    } else if (screenMode == SCR_WAVE) {
      drawWave();
    } else {
      drawCatFrame();
    }
    drawTitleBar();
  }
}
