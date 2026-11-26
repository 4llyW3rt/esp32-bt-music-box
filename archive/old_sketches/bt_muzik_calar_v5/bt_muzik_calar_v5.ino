/*
  ESP32 + BT201 + ILI9341 (2.8" 240x320) + HW-040 Encoder  -  MÜZİK ÇALAR v5
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

  GIF: data/anim.gif dosyasını LittleFS'e yükle (240x268 uyumlu). Yoksa GIF modu atlanır.
*/

#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <AnimatedGIF.h>
#include <driver/gpio.h>
#include <math.h>

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
#define TFT_ROTATION 0
#define TFT_INVERT   false
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
String cmdVolume(int v) {
  char b[12];
  snprintf(b, sizeof(b), "AT+CA%02d", constrain(v, 0, 30));
  return String(b);
}
const int VOL_MAX = 30;
// ======================================================

#define SCREEN_W   240
#define SCREEN_H   320
#define CONTENT_H  268          // üst içerik alanı; altta ortak panel (268-320)
#define GIF_FILE   "/anim.gif"

#define EQ_Y       148
#define EQ_H       112
#define EQ_SEGS    14
#define EQ_BARS    24

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
#define SCR_TEST    3
const char* screenNames[] = {"CALAN", "GIF", "DALGA", "TEST"};
int screenMode = SCR_PLAYING;

bool gifLoaded = false;
bool canvasOK  = true;
bool isPlaying = false;
bool muted     = false;
int  volume    = 15;
int  srcMode   = 1;              // 1=Bluetooth, 2=USB, 3=SD kart, 9=bekleme
int  requestedMode = 0;
unsigned long queryModeAt = 0;
unsigned long elapsedMs = 0;
unsigned long trackLenSec = 0;
unsigned long lastTimeSec = 0xFFFFFFFF;
bool progressDirty = true;

String titleRaw = "";
String titleAscii = "Bekleniyor...";
String lastRx = "-";
String lastTx = "-";

// ---------------------- Canvas'lar (titremesiz çizim) --------
GFXcanvas16 discCanvas(104, 104);
GFXcanvas16 titleCanvas(240, 22);
GFXcanvas16 eqCanvas(240, EQ_H);

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
  Serial2.print(cmd);
  Serial2.print("\r\n");
}

void setTitle(const String& s) {
  String a = asciiFold(s);
  if (a.length() == 0) a = "...";
  if (a == titleAscii) return;
  titleAscii = a;
  scrollX = 0;
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
  int yOff = (CONTENT_H - gif.getCanvasHeight()) / 2;
  if (xOff < 0) xOff = 0;
  if (yOff < 0) yOff = 0;
  int y = pDraw->iY + pDraw->y + yOff;
  if (y >= CONTENT_H) return;
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
void gifStart() {
  if (!gifLoaded) return;
  gif.close();
  gif.open(GIF_FILE, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw);
}

// ---------------------- Alt panel (tüm modlarda ortak) -------
void drawVolume() {
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(160, 298);
  char b[24];
  if (muted) snprintf(b, sizeof(b), "SESSIZ     ");
  else       snprintf(b, sizeof(b), "SES %2d/%d   ", volume, VOL_MAX);
  tft.print(b);
  int barW = muted ? 0 : map(volume, 0, VOL_MAX, 0, 228);
  tft.fillRect(5, 310, barW, 6, muted ? ILI9341_RED : ILI9341_GREEN);
  tft.fillRect(5 + barW, 310, 228 - barW, 6, ILI9341_BLACK);
}

void drawStatusRow() {
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(4, 298);
  tft.print(fit(srcMode == 1 ? "[BT]" : (srcMode == 3 ? "[SD]" : (srcMode == 2 ? "[USB]" : "[--]")), 5));
  tft.setTextColor(isPlaying ? ILI9341_GREEN : ILI9341_RED, ILI9341_BLACK);
  tft.setCursor(38, 298);
  tft.print(isPlaying ? "> CALIYOR " : "|| DURDU  ");
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(104, 298);
  tft.print(fit(screenNames[screenMode], 7));
}

void drawBottomStatic() {
  tft.fillRect(0, CONTENT_H, SCREEN_W, SCREEN_H - CONTENT_H, ILI9341_BLACK);
  tft.drawFastHLine(0, CONTENT_H, SCREEN_W, ILI9341_CYAN);
  tft.drawRect(4, 309, 232, 8, ILI9341_WHITE);
  drawStatusRow();
  drawVolume();
}

// Şarkı adı: sığmazsa kayar
void drawTitleBar() {
  if (!canvasOK) return;
  titleCanvas.fillScreen(ILI9341_BLACK);
  titleCanvas.setTextSize(2);
  titleCanvas.setTextColor(ILI9341_WHITE);
  titleCanvas.setTextWrap(false);
  int w = titleAscii.length() * 12;
  if (w <= SCREEN_W) {
    titleCanvas.setCursor((SCREEN_W - w) / 2, 3);
    titleCanvas.print(titleAscii);
  } else {
    int cycle = w + 48;
    titleCanvas.setCursor(-scrollX, 3);
    titleCanvas.print(titleAscii);
    titleCanvas.setCursor(-scrollX + cycle, 3);
    titleCanvas.print(titleAscii);
    if (isPlaying) scrollX = (scrollX + 2) % cycle;
  }
  tft.drawRGBBitmap(0, 272, titleCanvas.getBuffer(), 240, 22);
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
  tft.drawRect(8, 120, 224, 8, 0x4208);       // ilerleme çubuğu çerçevesi
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
  const int X = 8, Y = 120, W = 224, H = 8;
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
  discCanvas.fillScreen(ILI9341_BLACK);
  if (active) {
    int ring = 47 + (int)(beatPulse * 4);
    discCanvas.drawCircle(C, C, ring, wheel((millis() / 10) & 255));
  }
  discCanvas.fillCircle(C, C, 44, 0x2104);
  discCanvas.drawCircle(C, C, 40, 0x39E7);
  discCanvas.drawCircle(C, C, 35, 0x39E7);
  discCanvas.drawCircle(C, C, 30, 0x39E7);
  discCanvas.drawCircle(C, C, 25, 0x39E7);
  discCanvas.fillCircle(C, C, 18, wheel((millis() / 15) & 255));
  for (int k = 0; k < 2; k++) {
    float a = discAngle + k * PI;
    for (int j = 0; j < 2; j++) {
      float aa = a + j * 0.07f;
      discCanvas.drawLine(C + (int)(cosf(aa) * 22), C + (int)(sinf(aa) * 22),
                          C + (int)(cosf(aa) * 43), C + (int)(sinf(aa) * 43), 0x8410);
    }
  }
  discCanvas.fillCircle(C + (int)(cosf(discAngle) * 10), C + (int)(sinf(discAngle) * 10), 2, ILI9341_WHITE);
  discCanvas.fillCircle(C, C, 3, ILI9341_BLACK);
  tft.drawRGBBitmap(8, 8, discCanvas.getBuffer(), 104, 104);
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
  eqCanvas.fillScreen(ILI9341_BLACK);
  for (int i = 0; i < EQ_BARS; i++) {
    int n = constrain((int)(eqLevel[i] + 0.5f), 0, EQ_SEGS);
    int x = i * 10 + 1;
    for (int s = 0; s < n; s++) {
      uint16_t col = (s < 8) ? 0x07E0 : (s < 11 ? 0xFFE0 : 0xF800);
      eqCanvas.fillRect(x, EQ_H - (s + 1) * 8, 8, 6, col);
    }
    int p = constrain((int)eqPeak[i], 0, EQ_SEGS - 1);
    if (eqPeak[i] > 0.5f) eqCanvas.fillRect(x, EQ_H - (p + 1) * 8, 8, 2, ILI9341_WHITE);
  }
  tft.drawRGBBitmap(0, EQ_Y, eqCanvas.getBuffer(), 240, EQ_H);
}

// ---------------------- "Dalga" ekranı -----------------------
#define WAVE_N 61
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
  const int CY = 134;
  bool active = isPlaying && !muted;
  if (waveFresh) {
    for (int k = 0; k < 3; k++) for (int i = 0; i < WAVE_N; i++) wavePrev[k][i] = CY;
    waveFresh = false;
  }
  int16_t cur[3][WAVE_N];
  for (int k = 0; k < 3; k++) {
    wavePhase[k] += (0.10f + 0.05f * k) * (active ? 1.0f : 0.2f);
    float amp = 100.0f * energy * (1.0f - 0.28f * k);
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

// ---------------------- Test ekranı --------------------------
void drawTestStatic() {
  tft.fillRect(0, 0, SCREEN_W, CONTENT_H, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(4, 4);
  tft.print("TEST EKRANI");
}

void drawTestFrame() {
  static unsigned long last = 0;
  static int8_t shown[4] = {-1, -1, -1, -1};
  if (millis() - last < 100) return;
  last = millis();

  const char* names[4] = {"ONCE", "SONRA", "MOD", "SW"};
  const uint8_t pins[4] = {BTN_PREV, BTN_NEXT, BTN_MODE, ENC_SW};
  for (int i = 0; i < 4; i++) {
    bool pressed = (digitalRead(pins[i]) == LOW);
    if (shown[i] == (int8_t)pressed) continue;
    shown[i] = pressed;
    int x = 4 + i * 58;
    tft.fillRect(x, 32, 54, 28, pressed ? ILI9341_GREEN : 0x2104);
    tft.drawRect(x, 32, 54, 28, ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setTextColor(pressed ? ILI9341_BLACK : ILI9341_WHITE);
    tft.setCursor(x + 6, 42);
    tft.print(names[i]);
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft.setCursor(x + 14, 64);
    tft.printf("D%d", pins[i]);
  }

  char b[48];
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  snprintf(b, sizeof(b), "Encoder toplam: %-8ld", encTotal);
  tft.setCursor(4, 86);  tft.print(b);
  snprintf(b, sizeof(b), "Ses: %2d/%d %-6s Kaynak: %-10s", volume, VOL_MAX, muted ? "MUTE" : "", srcLabel());
  tft.setCursor(4, 100); tft.print(b);
  snprintf(b, sizeof(b), "Heap: %u KB  En buyuk blok: %u KB   ",
           (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
  tft.setCursor(4, 114); tft.print(b);
  snprintf(b, sizeof(b), "Calisma: %lu sn   Sure: %lu/%lu sn   ", millis() / 1000, elapsedMs / 1000, trackLenSec);
  tft.setCursor(4, 128); tft.print(b);

  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(4, 150); tft.print("RX: " + fit(lastRx, 34));
  tft.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  tft.setCursor(4, 164); tft.print("TX: " + fit(lastTx, 34));
  tft.setTextColor(0x7BEF, ILI9341_BLACK);
  tft.setCursor(4, 190); tft.print("Encoder: don -> ses, bas -> oynat/duraklat");
  tft.setCursor(4, 202); tft.print("Butonlar basinca kutu YESIL olmali.");
}

// ---------------------- Ekran geçişi -------------------------
void setScreen(int s) {
  screenMode = s;
  tft.fillRect(0, 0, SCREEN_W, CONTENT_H, ILI9341_BLACK);
  if (s == SCR_PLAYING) drawPlayingStatic();
  if (s == SCR_GIF)     gifStart();
  if (s == SCR_WAVE)    waveFresh = true;
  if (s == SCR_TEST)    drawTestStatic();
  drawBottomStatic();
}

void nextScreen() {
  int n = (screenMode + 1) % 4;
  if (n == SCR_GIF && !gifLoaded) n = (n + 1) % 4;
  setScreen(n);
}

// ---------------------- Eylemler -----------------------------
void playPause() {
  btSend(srcMode == 1 ? CMD_PP_BT : CMD_PP_TF);
  isPlaying = !isPlaying;       // modül durumu yarım saniye içinde doğrular
  drawStatusRow();
}
void toggleMute() {
  muted = !muted;
  btSend(muted ? CMD_MUTE : CMD_UNMUTE);
  drawVolume();
}
void switchSource() {
  requestedMode = (srcMode == 1) ? 3 : 1;     // BT <-> SD kart
  btSend(requestedMode == 3 ? CMD_MODE_TF : CMD_MODE_BT);
  setTitle("Mod degisiyor...");
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
bool btnLongFired[4] = {false, false, false, false};

void handleButton(int idx, bool longPress) {
  switch (idx) {
    case 0: trackChanged(CMD_PREV); break;
    case 1: trackChanged(CMD_NEXT); break;
    case 2: if (longPress) switchSource(); else nextScreen(); break;
    case 3: if (longPress) toggleMute();   else playPause();  break;
  }
}

void readButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    bool s = digitalRead(BTN_PINS[i]);
    bool longCapable = (i >= 2);
    if (s != btnState[i] && now - btnChange[i] > 40) {
      btnChange[i] = now;
      btnState[i] = s;
      if (s == LOW) {
        btnPressT[i] = now;
        btnLongFired[i] = false;
        Serial.printf("BTN %d (GPIO %d) basildi\n", i, BTN_PINS[i]);
        if (!longCapable) handleButton(i, false);
      } else if (longCapable && !btnLongFired[i]) {
        handleButton(i, false);          // kısa basış: bırakınca
      }
    }
    if (longCapable && btnState[i] == LOW && !btnLongFired[i] && now - btnPressT[i] > 700) {
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
    int nv = constrain(volume + steps, 0, VOL_MAX);
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
  }
  if (confirmPending && !dirty && now - lastChangeT > 400) {
    btSend(cmdVolume(volume));           // son değeri garantiye al
    confirmPending = false;
  }
}

// ---------------------- BT201'den gelen veri -----------------
void setPlaying(bool p) {
  if (p != isPlaying) { isPlaying = p; drawStatusRow(); }
}

void handleBTLine(const String& line) {
  lastRx = asciiFold(line);

  if (line.startsWith("QM+")) {                 // çalışma modu
    int m = line.substring(3).toInt();
    if (m != srcMode) {
      srcMode = m;
      elapsedMs = 0; trackLenSec = 0; titleRaw = "";
      lastTimeSec = 0xFFFFFFFF;
      setTitle(m == 1 ? "Bluetooth" : (m == 9 ? "Bekleme" : "Yukleniyor..."));
      drawStatusRow();
    }
    if (requestedMode && m != requestedMode) setTitle("SD kart yok?");
    requestedMode = 0;
  }
  else if (line.startsWith("MP+")) {            // SD/USB oynatma durumu
    setPlaying(line.substring(3).toInt() == 1);
  }
  else if (line.startsWith("TS+")) {            // Bluetooth durumu
    int v = line.substring(3).toInt();
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
    int dot = name.lastIndexOf('.');
    if (dot > 0 && (int)name.length() - dot <= 5) name.remove(dot);
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
    btSendQuiet((step % 4 == 0) ? "AT+QM" : "AT+TS");
  } else {
    static const char* seq[8] = {"AT+MP", "AT+MK", "AT+MF", "AT+MK", "AT+MP", "AT+MT", "AT+QM", "AT+MK"};
    btSendQuiet(seq[step % 8]);
  }
  step++;
}

// ---------------------- Açılış ekranı ------------------------
void drawSplash() {
  tft.fillScreen(ILI9341_BLACK);
  for (int i = 0; i < 24; i++) {
    tft.setTextSize(4);
    tft.setTextColor(wheel(i * 11));
    tft.setCursor(30, 110);
    tft.print("MUZIK");
    tft.setTextSize(2);
    tft.setTextColor(wheel(i * 11 + 90));
    tft.setCursor(40, 160);
    tft.print("ESP32 + BT201");
    int bw = (i + 1) * 9;
    tft.fillRect(12, 220, bw, 6, wheel(i * 11));
    delay(35);
  }
}

// ---------------------- SETUP / LOOP -------------------------
void setup() {
  Serial.begin(115200);
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

  canvasOK = (discCanvas.getBuffer() != NULL) && (titleCanvas.getBuffer() != NULL) && (eqCanvas.getBuffer() != NULL);
  Serial.printf("Heap: %u B, en buyuk blok: %u B, canvas: %s\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), canvasOK ? "OK" : "HATA");

  drawSplash();

  if (LittleFS.begin(true) && LittleFS.exists(GIF_FILE)) {
    gif.begin(LITTLE_ENDIAN_PIXELS);
    gifLoaded = true;
    Serial.println("GIF bulundu.");
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
  handleVolume();
  readBT();
  pollBT();

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length()) btSend(s);
  }

  if (screenMode == SCR_GIF) {
    if (gifLoaded && gif.playFrame(true, NULL) == 0) gif.reset();
    if (now - lastFrame >= 40) { lastFrame = now; drawTitleBar(); }
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
      drawTestFrame();
    }
    drawTitleBar();
  }
}
