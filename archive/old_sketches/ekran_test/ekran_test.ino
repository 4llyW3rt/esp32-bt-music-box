/*
  ST7735S 1.8" (128x160) EKRAN TEST KODU
  ---------------------------------------
  Sadece ekranı dener. BT201, buton ve pot yok.

  Pinler (şemaya göre):  CS=15  RESET=4  A0(DC)=2  SCK=18  SDA(MOSI)=23

  Serial Monitor: 115200 baud. Alttaki "Newline" ayarı fark etmez.
  Tek harf gönder:
    n = sonraki test deseni
    p = otomatik desen değişimini aç/kapat
    1 = INITR_BLACKTAB   2 = INITR_GREENTAB   3 = INITR_REDTAB
    i = renkleri ters çevir (invert aç/kapa)
    r = ekranı döndür (0-3)
    h = bu yardımı tekrar yaz
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

SPIClass tftSPI(VSPI);
Adafruit_ST7735 tft(&tftSPI, TFT_CS, TFT_DC, TFT_RST);

int  tabType  = 1;       // 1=BLACK, 2=GREEN, 3=RED
int  rotation = 0;
bool inverted = false;
bool autoCycle = true;
int  pattern  = 0;
unsigned long lastSwitch = 0;

const int PATTERN_COUNT = 8;
const char* patternNames[PATTERN_COUNT] = {
  "1) Kirmizi dolgu", "2) Yesil dolgu", "3) Mavi dolgu", "4) Beyaz dolgu",
  "5) Renk cubuklari", "6) Cerceve + kose renkleri", "7) Yazi / yon testi", "8) Gradyan"
};

void printHelp() {
  Serial.println();
  Serial.println("=== EKRAN TEST ===");
  Serial.println("n=sonraki desen  p=otomatik ac/kapa  1/2/3=tab tipi (BLACK/GREEN/RED)");
  Serial.println("i=invert  r=dondur  h=yardim");
}

void printInfo() {
  const char* tabNames[] = {"", "BLACKTAB", "GREENTAB", "REDTAB"};
  Serial.printf("[Tab: %s | Rotasyon: %d | Invert: %s | Desen: %s | Otomatik: %s]\n",
                tabNames[tabType], rotation, inverted ? "ACIK" : "kapali",
                patternNames[pattern], autoCycle ? "acik" : "kapali");
}

void initDisplay() {
  if (tabType == 1)      tft.initR(INITR_BLACKTAB);
  else if (tabType == 2) tft.initR(INITR_GREENTAB);
  else                   tft.initR(INITR_REDTAB);
  tft.setRotation(rotation);
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

    case 4: {   // Renk çubukları: beyaz sarı camgöbeği yeşil mor kırmızı mavi siyah
      uint16_t cols[8] = {ST77XX_WHITE, ST77XX_YELLOW, ST77XX_CYAN, ST77XX_GREEN,
                          ST77XX_MAGENTA, ST77XX_RED, ST77XX_BLUE, ST77XX_BLACK};
      tft.fillScreen(ST77XX_BLACK);
      int bw = w / 8;
      for (int i = 0; i < 8; i++) tft.fillRect(i * bw, 0, bw, h, cols[i]);
      tft.drawRect(7 * bw, 0, bw, h, ST77XX_WHITE);   // siyah çubuğun yerini göster
      break;
    }

    case 5: {   // Çerçeve ve köşe işaretleri
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

    case 6: {   // Yazı ve yön testi
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
      tft.fillTriangle(w - 20, 30, w - 6, 30, w - 13, 10, ST77XX_RED);  // yukarı ok
      break;
    }

    case 7: {   // Gradyan: gri, kırmızı, yeşil, mavi bantlar
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
  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
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
    switch (c) {
      case 'n': pattern = (pattern + 1) % PATTERN_COUNT; break;
      case 'p': autoCycle = !autoCycle; redraw = false; break;
      case '1': tabType = 1; initDisplay(); break;
      case '2': tabType = 2; initDisplay(); break;
      case '3': tabType = 3; initDisplay(); break;
      case 'i': inverted = !inverted; tft.invertDisplay(inverted); break;
      case 'r': rotation = (rotation + 1) % 4; tft.setRotation(rotation); break;
      case 'h': printHelp(); redraw = false; break;
      default:  redraw = false; break;
    }
    if (redraw) { drawPattern(pattern); lastSwitch = millis(); }
    if (c == 'n' || c == 'p' || c == '1' || c == '2' || c == '3' || c == 'i' || c == 'r') printInfo();
  }

  if (autoCycle && millis() - lastSwitch > 3000) {
    pattern = (pattern + 1) % PATTERN_COUNT;
    drawPattern(pattern);
    printInfo();
    lastSwitch = millis();
  }
}
