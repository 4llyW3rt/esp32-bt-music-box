/*
  ST7735S 1.8" (128x160) EKRAN TEST KODU  -  v3 (ofset ayarlı)
  ------------------------------------------------------------
  Sadece ekranı dener. BT201, buton ve pot yok.

  Pinler (şemaya göre):  CS=15  RESET=4  A0(DC)=2  SCK=18  SDA(MOSI)=23

  Serial Monitor: 115200 baud. Tek harf gönder:
    n = sonraki test deseni        p = otomatik desen değişimini aç/kapat
    1 = BLACKTAB   2 = GREENTAB   3 = REDTAB
    4 = BLACKTAB + ofset (2,1)   <-- bu panel tipinde en sık işe yarayan ayar
    x = kolon (sütun) ofsetini artır (0..4)
    y = satır ofsetini artır (0..4)
    i = renkleri ters çevir        r = ekranı döndür
    h = bu yardımı tekrar yaz

  DOĞRU AYAR: "6) Cerceve" desenindeki ince BEYAZ çerçeve 4 kenarda da
  eksiksiz görünmeli, kenarlarda çizgi/benek olmamalı, ve renkler doğru olmalı.
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

// 0 = donanım SPI (hızlı, normal)
// 1 = yazılım SPI (yavaş ama kablo/hız kaynaklı sorunları eler)
#define SOFT_SPI 1

// Ofset ayarı için küçük bir yardımcı sınıf
class MyTFT : public Adafruit_ST7735 {
 public:
#if SOFT_SPI
  MyTFT(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst)
      : Adafruit_ST7735(cs, dc, mosi, sclk, rst) {}
#else
  MyTFT(SPIClass* s, int8_t cs, int8_t dc, int8_t rst)
      : Adafruit_ST7735(s, cs, dc, rst) {}
#endif
  void setOffsets(uint8_t col, uint8_t row) {
    _colstart = col;
    _rowstart = row;
  }
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
bool autoCycle = true;
int  pattern   = 0;
unsigned long lastSwitch = 0;

const int PATTERN_COUNT = 8;
const char* patternNames[PATTERN_COUNT] = {
  "1) Kirmizi dolgu", "2) Yesil dolgu", "3) Mavi dolgu", "4) Beyaz dolgu",
  "5) Renk cubuklari", "6) Cerceve + kose renkleri", "7) Yazi / yon testi", "8) Gradyan"
};

void printHelp() {
  Serial.println();
  Serial.println("=== EKRAN TEST v3 ===");
  Serial.println("n=sonraki desen  p=otomatik ac/kapa");
  Serial.println("1/2/3=tab tipi (BLACK/GREEN/RED)  4=BLACKTAB + ofset(2,1)");
  Serial.println("x=kolon ofset+1  y=satir ofset+1  i=invert  r=dondur  h=yardim");
}

void printInfo() {
  const char* tabNames[] = {"", "BLACKTAB", "GREENTAB", "REDTAB"};
  Serial.printf("[Tab: %s | Ofset: %s kolon=%d satir=%d | Rotasyon: %d | Invert: %s | Desen: %s | Otomatik: %s]\n",
                tabNames[tabType], useCustomOffset ? "OZEL" : "varsayilan", colOff, rowOff,
                rotation, inverted ? "ACIK" : "kapali",
                patternNames[pattern], autoCycle ? "acik" : "kapali");
}

void applyGeometry() {
  if (useCustomOffset) tft.setOffsets(colOff, rowOff);
  tft.setRotation(rotation);      // ofseti ekran yönüne göre yeniden hesaplatır
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

    case 4: {   // beyaz sarı camgöbeği yeşil mor kırmızı mavi siyah
      uint16_t cols[8] = {ST77XX_WHITE, ST77XX_YELLOW, ST77XX_CYAN, ST77XX_GREEN,
                          ST77XX_MAGENTA, ST77XX_RED, ST77XX_BLUE, ST77XX_BLACK};
      tft.fillScreen(ST77XX_BLACK);
      int bw = w / 8;
      for (int i = 0; i < 8; i++) tft.fillRect(i * bw, 0, bw, h, cols[i]);
      tft.drawRect(7 * bw, 0, bw, h, ST77XX_WHITE);
      break;
    }

    case 5: {   // Çerçeve: en önemli desen, ofseti buna göre ayarla
      tft.fillScreen(ST77XX_BLACK);
      tft.drawRect(0, 0, w, h, ST77XX_WHITE);            // en dış çerçeve
      tft.drawRect(2, 2, w - 4, h - 4, ST77XX_YELLOW);   // iç çerçeve
      tft.fillRect(4, 4, 10, 10, ST77XX_RED);            // sol üst
      tft.fillRect(w - 14, 4, 10, 10, ST77XX_GREEN);     // sağ üst
      tft.fillRect(4, h - 14, 10, 10, ST77XX_BLUE);      // sol alt
      tft.fillRect(w - 14, h - 14, 10, 10, ST77XX_MAGENTA); // sağ alt
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
  }
}

void setup() {
  Serial.begin(115200);
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
  pattern = 5;                       // çerçeve deseniyle başla
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
      case '1': tabType = 1; useCustomOffset = false; colOff = rowOff = 0; initDisplay(); break;
      case '2': tabType = 2; useCustomOffset = false; colOff = rowOff = 0; initDisplay(); break;
      case '3': tabType = 3; useCustomOffset = false; colOff = rowOff = 0; initDisplay(); break;
      case '4': tabType = 1; useCustomOffset = true; colOff = 2; rowOff = 1; initDisplay(); break;
      case 'x': useCustomOffset = true; colOff = (colOff + 1) % 5; applyGeometry(); tft.fillScreen(ST77XX_BLACK); break;
      case 'y': useCustomOffset = true; rowOff = (rowOff + 1) % 5; applyGeometry(); tft.fillScreen(ST77XX_BLACK); break;
      case 'i': inverted = !inverted; tft.invertDisplay(inverted); break;
      case 'r': rotation = (rotation + 1) % 4; applyGeometry(); tft.fillScreen(ST77XX_BLACK); break;
      case 'h': printHelp(); redraw = false; break;
      default:  redraw = false; known = false; break;
    }
    if (redraw) { drawPattern(pattern); lastSwitch = millis(); }
    if (known && c != 'h') printInfo();
  }

  if (autoCycle && millis() - lastSwitch > 3000) {
    pattern = (pattern + 1) % PATTERN_COUNT;
    drawPattern(pattern);
    printInfo();
    lastSwitch = millis();
  }
}
