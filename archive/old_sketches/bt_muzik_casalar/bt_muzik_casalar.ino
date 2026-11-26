/*
  ESP32 + BT201 + ST7735 (1.8", 128x160) Müzik Çalar  -  v3
  ----------------------------------------------------------
  Ekran modları (4. butonla değişir):
    1) CALAN : şarkı adı (kayan yazı), süre, dönen plak, ekolayzır
    2) GIF   : /anim.gif oynatır (dosya yoksa atlanır)
    3) TEST  : buton/pot/BT201 test ekranı

  Butonlar:
    1 Oynat/Duraklat | 2 Önceki | 3 Sonraki | 4 Ekran modu
    5 Sessiz (kısa bas)  /  BT201 kaynak değiştir (uzun bas)

  Kütüphaneler: Adafruit GFX, Adafruit ST7735 and ST7789, AnimatedGIF
  !!! LCD ve BT201 pinlerini daha önce kendin düzenlediysen aynı değerleri
      aşağıya yaz. BT201 AT komutlarını datasheet ile doğrula. !!!
*/

#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <AnimatedGIF.h>
#include <math.h>

// ======================= PİN AYARLARI =======================
#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   22          // SCK=18, MOSI=23 (donanım SPI)

#define BT_RX     16          // ESP32 RX <- BT201 TX
#define BT_TX     17          // ESP32 TX -> BT201 RX
#define BT_BAUD   115200

// Buton sırası: 0=Oynat/Duraklat, 1=Önceki, 2=Sonraki, 3=Ekran modu, 4=Sessiz
// Şemaya göre butonlar GPIO 26,27,14,12,13'e bağlı.
// 13 ve 26'nın çalıştığını biliyoruz; 27,14,12 sırası TEST ekranıyla doğrulanmalı.
// Yanlış butonu tetikliyorsa bu dizideki sayıların yerini değiştir.
const uint8_t BTN_PINS[5] = {13, 27, 26, 14, 12};

#define POT_PIN   34          // ADC1 pini
// ============================================================

// ================= BT201 AT KOMUTLARI (DOĞRULA!) ============
const char* CMD_PLAYPAUSE = "AT+CB";
const char* CMD_NEXT      = "AT+CC";
const char* CMD_PREV      = "AT+CD";
const char* CMD_MODE      = "AT+CM";
String cmdVolume(int v) { return "AT+CA" + String(v); }   // 0..30
const int VOL_MAX = 30;
// ============================================================

#define GIF_FILE    "/anim.gif"
#define GIF_AREA_H  120
#define SCREEN_W    128
#define SCREEN_H    160

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;
File gifFile;

enum Screen { SCR_PLAYING = 0, SCR_GIF = 1, SCR_TEST = 2 };
const char* screenNames[] = {"CALAN", "GIF", "TEST"};
Screen screenMode = SCR_PLAYING;

bool gifLoaded = false;
bool isPlaying = true;       // BT201 durumu bildirmiyorsa tahmini durum
bool muted     = false;
int  volume    = 15;
unsigned long elapsedMs = 0; // ESP32'nin tuttuğu tahmini geçen süre

String titleRaw = "";
String titleAscii = "Bekleniyor...";
String lastRx = "-";
String lastTx = "-";
int adcRaw = 0, adcFilt = 0;

// ---------------------- Canvas'lar (titremesiz çizim) --------
GFXcanvas16 discCanvas(44, 44);
GFXcanvas16 titleCanvas(128, 16);
GFXcanvas16 eqCanvas(128, 46);
GFXcanvas16 testCanvas(128, 112);

float eqLevel[16], eqTarget[16], eqPeak[16];
float discAngle = 0;
int   scrollX = 0;
unsigned long lastTimeSec = 0xFFFFFFFF;

// ---------------------- Yardımcılar --------------------------
// Türkçe/UTF-8 karakterleri ekran fontu için ASCII'ye çevirir
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
    if (c >= 0xC0 && c < 0xE0) {            // 2 baytlık karakter
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

void setTitle(const String& s) {
  titleAscii = asciiFold(s);
  if (titleAscii.length() == 0) titleAscii = "...";
  scrollX = 0;
}

// ---------------------- GIF callbackleri ---------------------
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
void GIFDraw(GIFDRAW* pDraw) {
  int y = pDraw->iY + pDraw->y;
  int xOff = (SCREEN_W - gif.getCanvasWidth()) / 2;
  int yOff = (GIF_AREA_H - gif.getCanvasHeight()) / 2;
  if (xOff < 0) xOff = 0;
  if (yOff < 0) yOff = 0;
  y += yOff;
  if (y >= GIF_AREA_H) return;
  int w = pDraw->iWidth;
  int x = pDraw->iX + xOff;
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (w <= 0) return;
  static uint16_t line[SCREEN_W];
  uint8_t* s = pDraw->pPixels;
  uint16_t* pal = pDraw->pPalette;
  for (int i = 0; i < w; i++) {
    uint8_t p = s[i];
    if (pDraw->ucHasTransparency && p == pDraw->ucTransparent) line[i] = ST77XX_BLACK;
    else line[i] = pal[p];
  }
  tft.drawRGBBitmap(x, y, line, w, 1);
}
void gifStart() {
  if (!gifLoaded) return;
  gif.close();
  gif.open(GIF_FILE, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw);
}

// ---------------------- Alt durum çubuğu ---------------------
void drawVolume() {
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(2, GIF_AREA_H + 16);
  char b[24];
  if (muted) snprintf(b, sizeof(b), "Ses: SESSIZ    ");
  else       snprintf(b, sizeof(b), "Ses: %2d/%d     ", volume, VOL_MAX);
  tft.print(b);

  int barW = muted ? 0 : map(volume, 0, VOL_MAX, 0, 122);
  tft.fillRect(3, GIF_AREA_H + 29, barW, 6, muted ? ST77XX_RED : ST77XX_GREEN);
  tft.fillRect(3 + barW, GIF_AREA_H + 29, 122 - barW, 6, ST77XX_BLACK);
}

void drawStatusBar() {
  tft.fillRect(0, GIF_AREA_H, SCREEN_W, SCREEN_H - GIF_AREA_H, ST77XX_BLACK);
  tft.drawFastHLine(0, GIF_AREA_H, SCREEN_W, ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(2, GIF_AREA_H + 4);
  tft.print(screenNames[screenMode]);
  tft.setTextColor(isPlaying ? ST77XX_GREEN : ST77XX_RED);
  tft.setCursor(70, GIF_AREA_H + 4);
  tft.print(isPlaying ? "> CALIYOR" : "|| DURDU");
  tft.drawRect(2, GIF_AREA_H + 28, 124, 8, ST77XX_WHITE);
  drawVolume();
}

// ---------------------- "Çalan" ekranı -----------------------
void drawPlayingStatic() {
  tft.fillRect(0, 0, SCREEN_W, GIF_AREA_H, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(4, 4);
  tft.print("SURE");
  tft.setTextColor(0x7BEF, ST77XX_BLACK);
  tft.setCursor(4, 38);
  tft.print("BT MUZIK");
  lastTimeSec = 0xFFFFFFFF;
}

void drawTime() {
  unsigned long sec = elapsedMs / 1000;
  if (sec == lastTimeSec) return;
  lastTimeSec = sec;
  char b[8];
  snprintf(b, sizeof(b), "%02lu:%02lu", (sec / 60) % 100, sec % 60);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 18);
  tft.print(b);
}

// Albüm kapağı yerine dönen plak animasyonu
void drawDisc() {
  if (isPlaying && !muted) discAngle += 0.22f;
  discCanvas.fillScreen(ST77XX_BLACK);
  discCanvas.fillCircle(22, 22, 21, 0x2104);
  discCanvas.drawCircle(22, 22, 19, 0x4208);
  discCanvas.drawCircle(22, 22, 16, 0x4208);
  discCanvas.drawCircle(22, 22, 13, 0x4208);
  discCanvas.fillCircle(22, 22, 8, ST77XX_MAGENTA);
  for (int k = 0; k < 2; k++) {
    float a = discAngle + k * PI;
    int x0 = 22 + (int)(cosf(a) * 11), y0 = 22 + (int)(sinf(a) * 11);
    int x1 = 22 + (int)(cosf(a) * 19), y1 = 22 + (int)(sinf(a) * 19);
    discCanvas.drawLine(x0, y0, x1, y1, 0x8410);
  }
  discCanvas.fillCircle(22 + (int)(cosf(discAngle) * 5), 22 + (int)(sinf(discAngle) * 5), 1, ST77XX_WHITE);
  discCanvas.fillCircle(22, 22, 2, ST77XX_BLACK);
  tft.drawRGBBitmap(82, 4, discCanvas.getBuffer(), 44, 44);
}

// Şarkı adı: sığmazsa kayar
void drawTitle() {
  titleCanvas.fillScreen(ST77XX_BLACK);
  titleCanvas.setTextSize(2);
  titleCanvas.setTextColor(ST77XX_WHITE);
  titleCanvas.setTextWrap(false);
  int w = titleAscii.length() * 12;
  if (w <= SCREEN_W) {
    titleCanvas.setCursor((SCREEN_W - w) / 2, 0);
    titleCanvas.print(titleAscii);
  } else {
    int cycle = w + 36;
    titleCanvas.setCursor(-scrollX, 0);
    titleCanvas.print(titleAscii);
    titleCanvas.setCursor(-scrollX + cycle, 0);
    titleCanvas.print(titleAscii);
    if (isPlaying) scrollX = (scrollX + 2) % cycle;
  }
  tft.drawRGBBitmap(0, 52, titleCanvas.getBuffer(), 128, 16);
}

// Ekolayzır (gerçek ses analizi DEĞİL, animasyon)
void drawEqualizer() {
  bool active = isPlaying && !muted;
  for (int i = 0; i < 16; i++) {
    if (active) {
      if (random(100) < 22) eqTarget[i] = random(6, 44);
      eqLevel[i] += (eqTarget[i] - eqLevel[i]) * 0.4f;
    } else {
      eqTarget[i] = 2;
      eqLevel[i] += (2 - eqLevel[i]) * 0.2f;
    }
    if (eqLevel[i] > eqPeak[i]) eqPeak[i] = eqLevel[i];
    else eqPeak[i] -= 0.6f;
    if (eqPeak[i] < 2) eqPeak[i] = 2;
  }
  eqCanvas.fillScreen(ST77XX_BLACK);
  for (int i = 0; i < 16; i++) {
    int h = constrain((int)eqLevel[i], 1, 44);
    int x = i * 8;
    uint16_t c = h < 22 ? ST77XX_GREEN : (h < 34 ? ST77XX_YELLOW : ST77XX_RED);
    eqCanvas.fillRect(x + 1, 46 - h, 6, h, c);
    int ph = constrain((int)eqPeak[i], 1, 44);
    eqCanvas.fillRect(x + 1, 46 - ph - 2, 6, 1, ST77XX_WHITE);
  }
  tft.drawRGBBitmap(0, 72, eqCanvas.getBuffer(), 128, 46);
}

// ---------------------- Test ekranı --------------------------
void drawTestFrame() {
  const char* names[5] = {"PLY", "PRV", "NXT", "MOD", "MUT"};
  testCanvas.fillScreen(ST77XX_BLACK);
  testCanvas.setTextSize(1);
  testCanvas.setTextWrap(false);
  testCanvas.setTextColor(ST77XX_CYAN);
  testCanvas.setCursor(2, 0);
  testCanvas.print("TEST EKRANI");

  for (int i = 0; i < 5; i++) {
    int x = 2 + i * 25;
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    testCanvas.fillRect(x, 12, 23, 16, pressed ? ST77XX_GREEN : 0x2104);
    testCanvas.drawRect(x, 12, 23, 16, ST77XX_WHITE);
    testCanvas.setTextColor(pressed ? ST77XX_BLACK : ST77XX_WHITE);
    testCanvas.setCursor(x + 2, 16);
    testCanvas.print(names[i]);
    testCanvas.setTextColor(ST77XX_YELLOW);
    testCanvas.setCursor(x + 4, 31);
    testCanvas.print((int)BTN_PINS[i]);
  }

  char b[32];
  testCanvas.setTextColor(ST77XX_WHITE);
  snprintf(b, sizeof(b), "ADC ham : %4d", adcRaw);
  testCanvas.setCursor(2, 42);  testCanvas.print(b);
  snprintf(b, sizeof(b), "ADC filt: %4d", adcFilt);
  testCanvas.setCursor(2, 52);  testCanvas.print(b);
  snprintf(b, sizeof(b), "Ses adim: %d/%d %s", volume, VOL_MAX, muted ? "MUTE" : "");
  testCanvas.setCursor(2, 62);  testCanvas.print(b);

  testCanvas.drawRect(2, 74, 124, 8, ST77XX_WHITE);
  testCanvas.fillRect(3, 75, map(adcFilt, 0, 4095, 0, 122), 6, ST77XX_GREEN);

  testCanvas.setTextColor(ST77XX_CYAN);
  testCanvas.setCursor(2, 88);
  testCanvas.print("RX:" + lastRx.substring(0, 18));
  testCanvas.setTextColor(ST77XX_ORANGE);
  testCanvas.setCursor(2, 100);
  testCanvas.print("TX:" + lastTx.substring(0, 18));

  tft.drawRGBBitmap(0, 0, testCanvas.getBuffer(), 128, 112);
}

// ---------------------- Ekran geçişi -------------------------
void setScreen(Screen s) {
  screenMode = s;
  tft.fillRect(0, 0, SCREEN_W, GIF_AREA_H, ST77XX_BLACK);
  if (s == SCR_PLAYING) drawPlayingStatic();
  if (s == SCR_GIF) gifStart();
  drawStatusBar();
}

void nextScreen() {
  int n = ((int)screenMode + 1) % 3;
  if (n == SCR_GIF && !gifLoaded) n = (n + 1) % 3;
  setScreen((Screen)n);
}

// ---------------------- Butonlar -----------------------------
bool btnState[5]  = {true, true, true, true, true};
unsigned long btnChange[5] = {0, 0, 0, 0, 0};
unsigned long btnPressT[5] = {0, 0, 0, 0, 0};
bool btnLongFired[5] = {false, false, false, false, false};

void handleButton(int idx, bool longPress) {
  switch (idx) {
    case 0:
      btSend(CMD_PLAYPAUSE);
      isPlaying = !isPlaying;
      drawStatusBar();
      break;
    case 1:
      btSend(CMD_PREV);
      elapsedMs = 0; isPlaying = true;
      setTitle("<< Onceki");
      drawStatusBar();
      break;
    case 2:
      btSend(CMD_NEXT);
      elapsedMs = 0; isPlaying = true;
      setTitle("Sonraki >>");
      drawStatusBar();
      break;
    case 3:
      nextScreen();
      break;
    case 4:
      if (longPress) {
        btSend(CMD_MODE);          // BT201 kaynak modu
      } else {
        muted = !muted;
        btSend(muted ? cmdVolume(0) : cmdVolume(volume));
        drawVolume();
      }
      break;
  }
}

void readButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 5; i++) {
    bool s = digitalRead(BTN_PINS[i]);
    if (s != btnState[i] && now - btnChange[i] > 40) {
      btnChange[i] = now;
      btnState[i] = s;
      if (s == LOW) {
        btnPressT[i] = now;
        btnLongFired[i] = false;
        Serial.printf("BTN %d (GPIO %d) basildi\n", i, BTN_PINS[i]);
        if (i != 4) handleButton(i, false);
      } else if (i == 4 && !btnLongFired[i]) {
        handleButton(4, false);    // kısa basış: bırakınca
      }
    }
    if (i == 4 && btnState[i] == LOW && !btnLongFired[i] && now - btnPressT[i] > 700) {
      btnLongFired[i] = true;
      handleButton(4, true);       // uzun basış
    }
  }
}

// ---------------------- Potansiyometre -----------------------
// Düzeltmeler: medyan+ortalama, EMA filtre, histerezis, gönderme sınırı,
// değişiklikten sonra son değeri bir kez daha doğrulama.
void readVolume() {
  static unsigned long lastRead = 0, lastSend = 0, lastChangeT = 0;
  static float filt = -1;
  static bool dirty = false, confirmPending = false;
  unsigned long now = millis();
  if (now - lastRead < 10) return;
  lastRead = now;

  int s[9];
  for (int i = 0; i < 9; i++) s[i] = analogRead(POT_PIN);
  for (int i = 1; i < 9; i++) {          // sıralama
    int k = s[i], j = i - 1;
    while (j >= 0 && s[j] > k) { s[j + 1] = s[j]; j--; }
    s[j + 1] = k;
  }
  int avg = (s[2] + s[3] + s[4] + s[5] + s[6]) / 5;   // ortadaki 5 değerin ortalaması
  adcRaw = avg;

  bool first = (filt < 0);
  if (first) filt = avg;
  else       filt += 0.12f * (avg - filt);
  adcFilt = (int)filt;

  float pos;
  if (filt < 60)        pos = 0;
  else if (filt > 4030) pos = VOL_MAX;
  else                  pos = (filt - 60) / (4030.0f - 60.0f) * VOL_MAX;

  if (first) {
    volume = (int)roundf(pos);
    dirty = true; lastChangeT = now;
  } else if (fabsf(pos - volume) > 0.75f) {   // histerezis: 0.75 adımdan fazla hareket
    volume = (int)roundf(pos);
    muted = false;
    dirty = true; lastChangeT = now;
    drawVolume();
  }

  if (dirty && now - lastSend >= 60) {
    btSend(cmdVolume(volume));
    dirty = false; lastSend = now; confirmPending = true;
  }
  if (confirmPending && !dirty && now - lastChangeT > 400) {
    btSend(cmdVolume(volume));          // son değeri garantiye al
    confirmPending = false;
  }
}

// ---------------------- BT201'den gelen veri -----------------
void readBT() {
  static String buf;
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || c == '\r') {
      buf.trim();
      if (buf.length() > 0) {
        Serial.println("BT< " + buf);
        lastRx = asciiFold(buf);
        if (!buf.startsWith("OK") && !buf.startsWith("ERR") && buf.length() > 3) {
          if (buf != titleRaw) {
            titleRaw = buf;
            setTitle(buf);
            elapsedMs = 0;   // yeni şarkı: süreyi sıfırla
          }
        }
      }
      buf = "";
    } else {
      buf += c;
      if (buf.length() > 100) buf = "";
    }
  }
}

// ---------------------- SETUP / LOOP -------------------------
void setup() {
  Serial.begin(115200);
  Serial2.begin(BT_BAUD, SERIAL_8N1, BT_RX, BT_TX);
  randomSeed(micros());

  for (int i = 0; i < 5; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(POT_PIN, ADC_11db);

  for (int i = 0; i < 16; i++) { eqLevel[i] = 2; eqTarget[i] = 2; eqPeak[i] = 2; }

  tft.initR(INITR_BLACKTAB);     // renk/kayma sorunu olursa INITR_GREENTAB / INITR_REDTAB dene
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  if (LittleFS.begin(true) && LittleFS.exists(GIF_FILE)) {
    gif.begin(LITTLE_ENDIAN_PIXELS);
    gifLoaded = true;
  } else {
    Serial.println("GIF bulunamadi, GIF modu atlanacak.");
  }

  setScreen(SCR_PLAYING);
  Serial.println("Hazir. Serial Monitor'dan yazdigin komutlar BT201'e gider.");
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastTick = 0, lastFrame = 0;
  if (isPlaying) elapsedMs += now - lastTick;
  lastTick = now;

  readButtons();
  readVolume();
  readBT();

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length()) btSend(s);
  }

  if (screenMode == SCR_GIF) {
    if (gifLoaded && gif.playFrame(true, NULL) == 0) gif.reset();
  } else if (now - lastFrame >= 40) {      // ~25 kare/sn
    lastFrame = now;
    if (screenMode == SCR_PLAYING) {
      drawTime();
      drawDisc();
      drawTitle();
      drawEqualizer();
    } else {
      drawTestFrame();
    }
  }
}
