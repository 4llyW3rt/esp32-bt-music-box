/*
  ESP32 + BT201 + ST7735 (1.8", 128x160) Müzik Çalar
  ---------------------------------------------------
  Kütüphaneler (Library Manager'dan kur):
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library
    - AnimatedGIF (Larry Bank)
  GIF dosyası: sketch klasörü içinde "data" klasörü aç, içine "anim.gif" koy
  (128 piksel genişlikte, ~120 piksel yükseklikte, mümkünse <500KB).
  Sonra "ESP32 LittleFS Data Upload" eklentisi ile ESP32'ye yükle.

  !!! PİNLERİ KENDİ BAĞLANTINA GÖRE DÜZENLE (aşağıdaki bölüm) !!!
  !!! BT201 AT komutlarını modülünün datasheet'iyle doğrula !!!
*/

#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <AnimatedGIF.h>

// ======================= PİN AYARLARI =======================
// LCD (donanım SPI: SCK=18, MOSI=23 sabit kullanılıyor)
#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   22

// BT201 UART (Serial2)
#define BT_RX     16   // ESP32 RX  <- BT201 TX
#define BT_TX     17   // ESP32 TX  -> BT201 RX
#define BT_BAUD   115200   // Datasheet'ten kontrol et (bazı modüller 9600)

// Butonlar (GND'ye bağlı, dahili pull-up kullanılır)
// Not: 34-39 pinlerinde dahili pull-up YOKTUR, butonlar için kullanma.
#define BTN_PLAY  13
#define BTN_PREV  25
#define BTN_NEXT  26
#define BTN_MODE  32
#define BTN_GIF   33

// Potansiyometre (ses) - ADC1 pini olmalı
#define POT_PIN   34
// ============================================================

// ================= BT201 AT KOMUTLARI (DOĞRULA!) ============
// Komutların hepsi burada, yanlışsa sadece buradan düzelt.
const char* CMD_PLAYPAUSE = "AT+CB";
const char* CMD_NEXT      = "AT+CC";
const char* CMD_PREV      = "AT+CD";
const char* CMD_MODE      = "AT+CM";      // mod değiştirme (BT/USB/SD)
String cmdVolume(int v) { return "AT+CA" + String(v); }   // 0..30
const int VOL_MAX = 30;
// ============================================================

#define GIF_FILE    "/anim.gif"
#define GIF_AREA_H  120     // ekranın üst kısmı GIF alanı
#define SCREEN_W    128
#define SCREEN_H    160

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;
File gifFile;

bool gifEnabled = true;
bool gifLoaded  = false;
bool isPlaying  = true;
int  volume     = 15;
int  modeIndex  = 0;
const char* modeNames[] = {"BLUETOOTH", "USB/SD"};
String nowPlaying = "Bekleniyor...";

// Buton yapısı
struct Button {
  uint8_t pin;
  bool lastState;
  unsigned long lastChange;
};
Button buttons[5] = {
  {BTN_PLAY, HIGH, 0}, {BTN_PREV, HIGH, 0}, {BTN_NEXT, HIGH, 0},
  {BTN_MODE, HIGH, 0}, {BTN_GIF,  HIGH, 0}
};

// ---------------------- BT201 gönderme -----------------------
void btSend(const String& cmd) {
  Serial2.print(cmd);
  Serial2.print("\r\n");
  Serial.println("BT> " + cmd);
}

// ---------------------- GIF dosya callbackleri ---------------
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

// Her GIF satırını ekrana çizer
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
    if (pDraw->ucHasTransparency && p == pDraw->ucTransparent)
      line[i] = ST77XX_BLACK;
    else
      line[i] = pal[p];
  }
  tft.drawRGBBitmap(x, y, line, w, 1);
}

// ---------------------- Ekran çizimleri ----------------------
void drawStatusBar() {
  tft.fillRect(0, GIF_AREA_H, SCREEN_W, SCREEN_H - GIF_AREA_H, ST77XX_BLACK);
  tft.drawFastHLine(0, GIF_AREA_H, SCREEN_W, ST77XX_CYAN);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(2, GIF_AREA_H + 4);
  tft.print(modeNames[modeIndex]);

  tft.setTextColor(isPlaying ? ST77XX_GREEN : ST77XX_RED);
  tft.setCursor(80, GIF_AREA_H + 4);
  tft.print(isPlaying ? "> CALIYOR" : "|| DURDU");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, GIF_AREA_H + 16);
  tft.print("Ses: ");
  tft.print(volume);
  tft.print("/");
  tft.print(VOL_MAX);

  // ses çubuğu
  int barW = map(volume, 0, VOL_MAX, 0, 124);
  tft.drawRect(2, GIF_AREA_H + 28, 124, 8, ST77XX_WHITE);
  tft.fillRect(3, GIF_AREA_H + 29, barW > 2 ? barW - 2 : 0, 6, ST77XX_GREEN);
}

// GIF kapalıyken üst alanda şarkı bilgisi göster
void drawNowPlayingPanel() {
  tft.fillRect(0, 0, SCREEN_W, GIF_AREA_H, ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(4, 6);
  tft.print("Su an caliyor:");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(4, 28);
  tft.setTextWrap(true);
  String t = nowPlaying;
  if (t.length() > 40) t = t.substring(0, 40);
  tft.print(t);
  tft.setTextSize(1);
}

void redrawAll() {
  if (gifEnabled && gifLoaded) {
    tft.fillRect(0, 0, SCREEN_W, GIF_AREA_H, ST77XX_BLACK);
  } else {
    drawNowPlayingPanel();
  }
  drawStatusBar();
}

// ---------------------- GIF kontrol --------------------------
void gifStart() {
  if (!gifLoaded) return;
  gif.close();
  gif.open(GIF_FILE, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw);
}

// ---------------------- Butonlar -----------------------------
void handleButton(int idx) {
  switch (idx) {
    case 0:  // Play/Pause
      btSend(CMD_PLAYPAUSE);
      isPlaying = !isPlaying;
      drawStatusBar();
      break;
    case 1:  // Önceki
      btSend(CMD_PREV);
      nowPlaying = "<< Onceki";
      if (!(gifEnabled && gifLoaded)) drawNowPlayingPanel();
      break;
    case 2:  // Sonraki
      btSend(CMD_NEXT);
      nowPlaying = ">> Sonraki";
      if (!(gifEnabled && gifLoaded)) drawNowPlayingPanel();
      break;
    case 3:  // Mod değiştir
      btSend(CMD_MODE);
      modeIndex = (modeIndex + 1) % 2;
      drawStatusBar();
      break;
    case 4:  // GIF aç/kapat
      gifEnabled = !gifEnabled;
      if (gifEnabled && gifLoaded) gifStart();
      redrawAll();
      break;
  }
}

void readButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 5; i++) {
    bool s = digitalRead(buttons[i].pin);
    if (s != buttons[i].lastState && now - buttons[i].lastChange > 40) {
      buttons[i].lastChange = now;
      buttons[i].lastState = s;
      if (s == LOW) handleButton(i);   // basıldığında
    }
  }
}

// ---------------------- Potansiyometre -----------------------
void readVolume() {
  static unsigned long last = 0;
  if (millis() - last < 100) return;
  last = millis();

  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(POT_PIN);
  int raw = sum / 8;                       // 0..4095
  int v = map(raw, 0, 4095, 0, VOL_MAX);
  if (abs(v - volume) >= 1) {
    // küçük titreşimleri engelle (histerezis)
    static int lastRaw = -1000;
    if (abs(raw - lastRaw) > 60) {
      lastRaw = raw;
      volume = v;
      btSend(cmdVolume(volume));
      drawStatusBar();
    }
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
        // Modül şarkı adı gönderiyorsa burada yakalanır.
        // Çıktı biçimini Serial Monitor'dan görüp filtreyi uyarla.
        if (!buf.startsWith("OK") && !buf.startsWith("ERR")) {
          nowPlaying = buf;
          if (!(gifEnabled && gifLoaded)) drawNowPlayingPanel();
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

  for (int i = 0; i < 5; i++) pinMode(buttons[i].pin, INPUT_PULLUP);
  analogReadResolution(12);

  tft.initR(INITR_BLACKTAB);     // Ekran renkleri/kayma bozuksa INITR_GREENTAB veya INITR_REDTAB dene
  tft.setRotation(0);            // 128x160 dikey
  tft.fillScreen(ST77XX_BLACK);

  if (LittleFS.begin(true) && LittleFS.exists(GIF_FILE)) {
    gif.begin(LITTLE_ENDIAN_PIXELS);
    gifLoaded = true;
    gifStart();
  } else {
    Serial.println("GIF bulunamadi, sadece metin modu.");
    gifEnabled = false;
  }

  redrawAll();
  Serial.println("Hazir. Serial Monitor'dan yazdigin komutlar BT201'e gider.");
}

void loop() {
  readButtons();
  readVolume();
  readBT();

  // Serial Monitor -> BT201 (komut denemek için)
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length()) btSend(s);
  }

  // GIF karesi oynat
  if (gifEnabled && gifLoaded) {
    int r = gif.playFrame(true, NULL);
    if (r == 0) {          // GIF bitti, başa sar
      gif.reset();
    }
  }
}
