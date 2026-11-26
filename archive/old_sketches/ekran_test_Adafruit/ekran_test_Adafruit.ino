/*
  ST7735S 1.8" (128x160) EKRAN TEST - ADAFRUIT KÜTÜPHANESİ  -  v4
  ------------------------------------------------------------
  Pinler: CS=15  RESET=4  A0(DC)=2  SCK=18  SDA(MOSI)=23

  Serial Monitor: 115200 baud, alt satır ayarı: "Newline" (Yeni satır)

  Bu sürüm CETVEL deseniyle açılır. Ekranda dikey/yatay çizgiler ve piksel
  numaraları (0, 16, 32, ...) görünür. Sol üst köşede KIRMIZI, sağ alt köşede
  YESIL küçük kare olmalı. Soldaki şerit beyaz kalıyorsa ofseti büyüt.

  KOMUTLAR (tek harf, sonra Enter):
    X = kolon ofseti +8      Y = satır ofseti +8
    x = kolon ofseti +1      y = satır ofseti +1
    k32 = kolon ofseti tam olarak 32   s0 = satır ofseti 0   (sayıyı sen yaz)
    z = ofsetleri sıfırla
    a = X yönünü çevir (ayna)    b = Y yönünü çevir    c = RGB/BGR sırası
    n = sonraki desen    p = otomatik değişimi aç/kapa
    1/2/3 = BLACKTAB/GREENTAB/REDTAB    4 = BLACKTAB + ofset(2,1)
    i = renkleri ters çevir   r = döndür   h = yardım
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_SCK   18
#define TFT_MOSI  23

// 0 = donanım SPI, 1 = yazılım SPI (yavaş ama kablodan etkilenmez)
#define SOFT_SPI 0

class MyTFT : public Adafruit_ST7735 {
 public:
#if SOFT_SPI
  MyTFT(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst)
      : Adafruit_ST7735(cs, dc, mosi, sclk, rst) {}
#else
  MyTFT(SPIClass* s, int8_t cs, int8_t dc, int8_t rst)
      : Adafruit_ST7735(s, cs, dc, rst) {}
#endif
  void setOffsets(uint8_t col, uint8_t row) { _colstart = col; _rowstart = row; }
  void setMadctl(uint8_t v) { sendCommand(ST77XX_MADCTL, &v, 1); }
};

#if SOFT_SPI
MyTFT tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);
#else
SPIClass tftSPI(VSPI);
MyTFT tft(&tftSPI, TFT_CS, TFT_DC, TFT_RST);
#endif

int  tabType   = 1;       // 1=BLACK, 2=GREEN, 3=RED
int  rotation  = 0;
bool inverted  = false;
bool useCustomOffset = false;
int  colOff = 0, rowOff = 0;
bool rawMode = false;     // true: MADCTL'i elle (a/b/c tuşlarıyla) belirliyoruz
bool mx = true, my = true, bgr = false;
bool autoCycle = false;
int  pattern   = 8;       // cetvelle başla
unsigned long lastSwitch = 0;

const int PATTERN_COUNT = 9;
const char* patternNames[PATTERN_COUNT] = {
  "1) Kirmizi dolgu", "2) Yesil dolgu", "3) Mavi dolgu", "4) Beyaz dolgu",
  "5) Renk cubuklari", "6) Cerceve + kose renkleri", "7) Yazi / yon testi",
  "8) Gradyan", "9) CETVEL"
};

uint8_t madctlValue() { return (my ? 0x80 : 0) | (mx ? 0x40 : 0) | (bgr ? 0x08 : 0); }

void printHelp() {
  Serial.println();
  Serial.println("=== EKRAN TEST v4 (Adafruit) ===");
  Serial.println("X/Y = ofset +8 | x/y = +1 | k32 / s0 = tam deger | z = sifirla");
  Serial.println("a=X ayna  b=Y ayna  c=RGB/BGR | n=desen p=otomatik");
  Serial.println("1/2/3=tab  4=BLACKTAB+(2,1)  i=invert  r=dondur  h=yardim");
}

void printInfo() {
  const char* tabNames[] = {"", "BLACKTAB", "GREENTAB", "REDTAB"};
  Serial.printf("[Tab:%s | Ofset:%s kolon=%d satir=%d | Rot:%d | Invert:%s | ",
                tabNames[tabType], useCustomOffset ? "OZEL" : "vars.", colOff, rowOff,
                rotation, inverted ? "ACIK" : "kapali");
  if (rawMode) Serial.printf("MADCTL=0x%02X (MX=%d MY=%d BGR=%d) | ", madctlValue(), mx, my, bgr);
  Serial.printf("Desen: %s]\n", patternNames[pattern]);
}

void applyGeometry() {
  if (useCustomOffset) tft.setOffsets(colOff, rowOff);
  tft.setRotation(rotation);
  if (rawMode) tft.setMadctl(madctlValue());
}

void initDisplay() {
  if (tabType == 1)      tft.initR(INITR_BLACKTAB);
  else if (tabType == 2) tft.initR(INITR_GREENTAB);
  else                   tft.initR(INITR_REDTAB);
  applyGeometry();
  tft.invertDisplay(inverted);
  tft.fillScreen(ST77XX_BLACK);
}

void fillLabel(uint16_t bg, const char* text, uint16_t fg) {
  int w = tft.width(), h = tft.height();
  tft.fillScreen(bg);
  tft.setTextSize(2);
  tft.setTextColor(fg);
  int tw = strlen(text) * 12;
  tft.setCursor((w - tw) / 2, h / 2 - 8);
  tft.print(text);
}

void drawPattern(int p) {
  int w = tft.width(), h = tft.height();
  tft.setTextWrap(false);

  switch (p) {
    case 0: fillLabel(ST77XX_RED,   "KIRMIZI", ST77XX_WHITE); break;
    case 1: fillLabel(ST77XX_GREEN, "YESIL",   ST77XX_BLACK); break;
    case 2: fillLabel(ST77XX_BLUE,  "MAVI",    ST77XX_WHITE); break;
    case 3: fillLabel(ST77XX_WHITE, "BEYAZ",   ST77XX_BLACK); break;

    case 4: {
      uint16_t cols[8] = {ST77XX_WHITE, ST77XX_YELLOW, ST77XX_CYAN, ST77XX_GREEN,
                          ST77XX_MAGENTA, ST77XX_RED, ST77XX_BLUE, ST77XX_BLACK};
      tft.fillScreen(ST77XX_BLACK);
      int bw = w / 8;
      for (int i = 0; i < 8; i++) tft.fillRect(i * bw, 0, bw, h, cols[i]);
      tft.drawRect(7 * bw, 0, bw, h, ST77XX_WHITE);
      break;
    }

    case 5: {
      tft.fillScreen(ST77XX_BLACK);
      tft.drawRect(0, 0, w, h, ST77XX_WHITE);
      tft.drawRect(2, 2, w - 4, h - 4, ST77XX_YELLOW);
      tft.fillRect(4, 4, 10, 10, ST77XX_RED);
      tft.fillRect(w - 14, 4, 10, 10, ST77XX_GREEN);
      tft.fillRect(4, h - 14, 10, 10, ST77XX_BLUE);
      tft.fillRect(w - 14, h - 14, 10, 10, ST77XX_MAGENTA);
      tft.drawLine(0, 0, w - 1, h - 1, ST77XX_CYAN);
      tft.drawLine(w - 1, 0, 0, h - 1, ST77XX_CYAN);
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(20, h / 2 - 12); tft.print("Sol ust: KIRMIZI");
      tft.setCursor(20, h / 2);      tft.print("Sag ust: YESIL");
      tft.setCursor(20, h / 2 + 12); tft.print("Sol alt: MAVI");
      break;
    }

    case 6: {
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      tft.setCursor(2, 2);  tft.print("UST (boyut 1)");
      tft.setTextSize(2);
      tft.setCursor(2, 16); tft.print("Boyut 2");
      tft.setTextSize(3);
      tft.setTextColor(ST77XX_GREEN);
      tft.setCursor(2, 40); tft.print("ABC");
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_YELLOW);
      tft.setCursor(2, 72); tft.print("0123456789");
      tft.setCursor(2, 84); tft.print("abcdefghijklmnop");
      tft.setTextColor(ST77XX_CYAN);
      tft.setCursor(2, 100); tft.printf("%dx%d", w, h);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(2, h - 10); tft.print("ALT");
      tft.fillTriangle(w - 20, 30, w - 6, 30, w - 13, 10, ST77XX_RED);
      break;
    }

    case 7: {
      int bandH = h / 4;
      for (int x = 0; x < w; x++) {
        int v = map(x, 0, w - 1, 0, 255);
        tft.drawFastVLine(x, 0,          bandH, tft.color565(v, v, v));
        tft.drawFastVLine(x, bandH,      bandH, tft.color565(v, 0, 0));
        tft.drawFastVLine(x, bandH * 2,  bandH, tft.color565(0, v, 0));
        tft.drawFastVLine(x, bandH * 3,  h - bandH * 3, tft.color565(0, 0, v));
      }
      break;
    }

    case 8: {   // CETVEL: hangi piksel nerede görünüyor?
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextSize(1);
      for (int x = 0; x < w; x += 16) {
        tft.drawFastVLine(x, 0, h, (x % 32 == 0) ? 0x7BEF : 0x39E7);
        tft.setTextColor(ST77XX_YELLOW);
        tft.setCursor(x + 2, 2 + ((x / 16) % 2) * 10);
        tft.print(x);
      }
      for (int y = 16; y < h; y += 16) {
        tft.drawFastHLine(0, y, w, (y % 32 == 0) ? 0x7BEF : 0x39E7);
        tft.setTextColor(ST77XX_CYAN);
        tft.setCursor(2, y + 2);
        tft.print(y);
      }
      tft.fillRect(0, 0, 6, 6, ST77XX_RED);               // (0,0) noktası
      tft.fillRect(w - 6, h - 6, 6, 6, ST77XX_GREEN);     // son nokta
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(150);
  delay(300);
  Serial.printf("Pinler: CS=%d DC=%d RST=%d SCK=%d MOSI=%d\n",
                TFT_CS, TFT_DC, TFT_RST, TFT_SCK, TFT_MOSI);
#if SOFT_SPI
  Serial.println("SPI modu: YAZILIM (yavas)");
#else
  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
  Serial.println("SPI modu: DONANIM");
#endif
  initDisplay();
  printHelp();
  drawPattern(pattern);
  printInfo();
  lastSwitch = millis();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    bool redraw = true;
    bool known = true;
    switch (c) {
      case 'n': pattern = (pattern + 1) % PATTERN_COUNT; break;
      case 'p': autoCycle = !autoCycle; redraw = false; break;
      case '1': tabType = 1; useCustomOffset = false; colOff = rowOff = 0; rawMode = false; mx = my = true; bgr = false; initDisplay(); break;
      case '2': tabType = 2; useCustomOffset = false; colOff = rowOff = 0; rawMode = false; mx = my = true; bgr = false; initDisplay(); break;
      case '3': tabType = 3; useCustomOffset = false; colOff = rowOff = 0; rawMode = false; mx = my = true; bgr = false; initDisplay(); break;
      case '4': tabType = 1; useCustomOffset = true; colOff = 2; rowOff = 1; rawMode = false; mx = my = true; bgr = false; initDisplay(); break;
      case 'x': useCustomOffset = true; colOff = (colOff + 1) % 64; applyGeometry(); break;
      case 'y': useCustomOffset = true; rowOff = (rowOff + 1) % 64; applyGeometry(); break;
      case 'X': useCustomOffset = true; colOff = (colOff + 8) % 64; applyGeometry(); break;
      case 'Y': useCustomOffset = true; rowOff = (rowOff + 8) % 64; applyGeometry(); break;
      case 'k': useCustomOffset = true; colOff = constrain(Serial.parseInt(), 0, 63); applyGeometry(); break;
      case 's': useCustomOffset = true; rowOff = constrain(Serial.parseInt(), 0, 63); applyGeometry(); break;
      case 'z': useCustomOffset = false; colOff = rowOff = 0; initDisplay(); break;
      case 'a': rawMode = true; mx = !mx; applyGeometry(); break;
      case 'b': rawMode = true; my = !my; applyGeometry(); break;
      case 'c': rawMode = true; bgr = !bgr; applyGeometry(); break;
      case 'i': inverted = !inverted; tft.invertDisplay(inverted); break;
      case 'r': rotation = (rotation + 1) % 4; rawMode = false; mx = my = true; bgr = false; applyGeometry(); break;
      case 'h': printHelp(); redraw = false; break;
      default:  redraw = false; known = false; break;
    }
    if (redraw) { tft.fillScreen(ST77XX_BLACK); drawPattern(pattern); lastSwitch = millis(); }
    if (known && c != 'h') printInfo();
  }

  if (autoCycle && millis() - lastSwitch > 3000) {
    pattern = (pattern + 1) % PATTERN_COUNT;
    drawPattern(pattern);
    printInfo();
    lastSwitch = millis();
  }
}
