/*
  2.8" IPS TFT 240x320  ILI9341  -  EKRAN TESTİ
  ----------------------------------------------
  Kütüphane: "Adafruit ILI9341" (Library Manager)  [+ Adafruit GFX, BusIO]

  BAĞLANTI (ekran -> ESP32):
    VCC  -> 3V3          GND  -> GND
    CLK  -> D18          MOSI -> D23
    DC   -> D2           RES  -> D4
    BLK  -> 3V3 (220 ohm direnç üzerinden)   [yanmazsa GND'ye dene]
    MISO -> boş bırak    CS pini modülde yok (kartta GND'ye bağlı)

  Kendi kendine döner (her ekran 3 sn). İstersen Serial Monitor (115200,
  "Newline") ile şu harfleri gönder:
    n = sonraki desen     p = otomatik değişimi aç/kapa
    i = renkleri ters çevir (IPS ekranlarda gerekebilir)
    r = ekranı döndür     c = RGB/BGR sırasını değiştir (kırmızı-mavi ters ise)
    h = yardım
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_DC    2
#define TFT_RST   4
#define TFT_SCK   18
#define TFT_MOSI  23
#define TFT_CS    -1          // modülde CS pini yok

#define BL_PIN    -1          // -1: BLK doğrudan 3V3'e bağlı. Bir GPIO yazarsan kod yönetir
#define BL_ON     HIGH        // arka ışık yanmazsa LOW yap (sadece BL_PIN kullanırken)
#define SPI_HZ    16000000    // 16 MHz: jumper kablolar için güvenli

class MyTFT : public Adafruit_ILI9341 {
 public:
  MyTFT(SPIClass* s, int8_t dc, int8_t cs, int8_t rst) : Adafruit_ILI9341(s, dc, cs, rst) {}
  void setMadctl(uint8_t v) { sendCommand(ILI9341_MADCTL, &v, 1); }
};

SPIClass tftSPI(VSPI);
MyTFT tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

int  rotation = 0;
bool inverted = false;
bool bgr = true;
bool autoCycle = true;
int  pattern = 0;
unsigned long lastSwitch = 0, lastAnim = 0;

const int PATTERN_COUNT = 11;
const char* patternNames[PATTERN_COUNT] = {
  "Kirmizi", "Yesil", "Mavi", "Beyaz", "Siyah", "Renk cubuklari",
  "Cerceve + koseler", "Yazi testi", "Cetvel", "Gradyan", "Animasyon"
};

// animasyon durumu
int bx = 20, by = 40, vx = 3, vy = 2;
uint16_t bcol = ILI9341_YELLOW;

uint8_t madctlValue() {
  static const uint8_t base[4] = {0x40, 0x20, 0x80, 0xE0};   // MX | MV | MY | (MX+MY+MV)
  return base[rotation] | (bgr ? 0x08 : 0x00);
}

void applyGeometry() {
  tft.setRotation(rotation);
  tft.setMadctl(madctlValue());
}

void printHelp() {
  Serial.println();
  Serial.println("=== ILI9341 EKRAN TESTI ===");
  Serial.println("n=sonraki desen  p=otomatik ac/kapa  i=invert  r=dondur  c=RGB/BGR  h=yardim");
}

void printInfo() {
  Serial.printf("[Desen: %s | Rotasyon: %d | Invert: %s | Renk sirasi: %s | Otomatik: %s | %dx%d]\n",
                patternNames[pattern], rotation, inverted ? "ACIK" : "kapali",
                bgr ? "BGR" : "RGB", autoCycle ? "acik" : "kapali", tft.width(), tft.height());
}

void fillLabel(uint16_t bg, const char* text, uint16_t fg) {
  int w = tft.width(), h = tft.height();
  tft.fillScreen(bg);
  tft.setTextSize(3);
  tft.setTextColor(fg);
  int tw = strlen(text) * 18;
  tft.setCursor((w - tw) / 2, h / 2 - 20);
  tft.print(text);
  tft.setTextSize(1);
  tft.setCursor((w - 21 * 6) / 2, h / 2 + 14);
  tft.print("ILI9341 240x320 IPS");
}

void drawPattern(int p) {
  int w = tft.width(), h = tft.height();
  tft.setTextWrap(false);

  switch (p) {
    case 0: fillLabel(ILI9341_RED,   "KIRMIZI", ILI9341_WHITE); break;
    case 1: fillLabel(ILI9341_GREEN, "YESIL",   ILI9341_BLACK); break;
    case 2: fillLabel(ILI9341_BLUE,  "MAVI",    ILI9341_WHITE); break;
    case 3: fillLabel(ILI9341_WHITE, "BEYAZ",   ILI9341_BLACK); break;
    case 4: fillLabel(ILI9341_BLACK, "SIYAH",   ILI9341_WHITE); break;

    case 5: {   // beyaz sarı camgöbeği yeşil mor kırmızı mavi siyah
      uint16_t cols[8] = {ILI9341_WHITE, ILI9341_YELLOW, ILI9341_CYAN, ILI9341_GREEN,
                          ILI9341_MAGENTA, ILI9341_RED, ILI9341_BLUE, ILI9341_BLACK};
      tft.fillScreen(ILI9341_BLACK);
      int bw = w / 8;
      for (int i = 0; i < 8; i++) tft.fillRect(i * bw, 0, bw, h, cols[i]);
      tft.drawRect(7 * bw, 0, bw, h, ILI9341_WHITE);
      break;
    }

    case 6: {   // çerçeve ve köşe renkleri
      tft.fillScreen(ILI9341_BLACK);
      tft.drawRect(0, 0, w, h, ILI9341_WHITE);
      tft.drawRect(3, 3, w - 6, h - 6, ILI9341_YELLOW);
      tft.fillRect(6, 6, 16, 16, ILI9341_RED);                 // sol üst
      tft.fillRect(w - 22, 6, 16, 16, ILI9341_GREEN);          // sağ üst
      tft.fillRect(6, h - 22, 16, 16, ILI9341_BLUE);           // sol alt
      tft.fillRect(w - 22, h - 22, 16, 16, ILI9341_MAGENTA);   // sağ alt
      tft.drawLine(0, 0, w - 1, h - 1, ILI9341_CYAN);
      tft.drawLine(w - 1, 0, 0, h - 1, ILI9341_CYAN);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(30, h / 2 - 24); tft.print("Sol ust: KIRMIZI");
      tft.setCursor(30, h / 2);      tft.print("Sag ust: YESIL");
      tft.setCursor(30, h / 2 + 24); tft.print("Sol alt: MAVI");
      break;
    }

    case 7: {   // yazı testi
      tft.fillScreen(ILI9341_BLACK);
      tft.setTextColor(ILI9341_WHITE);
      tft.setTextSize(1);
      tft.setCursor(4, 4);   tft.print("YAZI TESTI (boyut 1)");
      tft.setTextSize(2);
      tft.setCursor(4, 24);  tft.print("Boyut 2");
      tft.setTextSize(3);
      tft.setTextColor(ILI9341_GREEN);
      tft.setCursor(4, 50);  tft.print("Boyut 3");
      tft.setTextSize(4);
      tft.setTextColor(ILI9341_YELLOW);
      tft.setCursor(4, 84);  tft.print("ABC123");
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_CYAN);
      tft.setCursor(4, 130); tft.print("0123456789");
      tft.setCursor(4, 150); tft.print("abcdefghijklmnop");
      tft.setTextSize(1);
      tft.setTextColor(ILI9341_ORANGE);
      tft.setCursor(4, 180); tft.printf("Cozunurluk: %dx%d", w, h);
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(4, h - 12); tft.print("ALT");
      tft.fillTriangle(w - 30, 60, w - 10, 60, w - 20, 30, ILI9341_RED);  // yukarı ok
      break;
    }

    case 8: {   // cetvel
      tft.fillScreen(ILI9341_BLACK);
      tft.setTextSize(1);
      for (int x = 0; x < w; x += 20) {
        tft.drawFastVLine(x, 0, h, (x % 40 == 0) ? 0x7BEF : 0x39E7);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setCursor(x + 2, 2 + ((x / 20) % 2) * 10);
        tft.print(x);
      }
      for (int y = 20; y < h; y += 20) {
        tft.drawFastHLine(0, y, w, (y % 40 == 0) ? 0x7BEF : 0x39E7);
        tft.setTextColor(ILI9341_CYAN);
        tft.setCursor(2, y + 2);
        tft.print(y);
      }
      tft.fillRect(0, 0, 8, 8, ILI9341_RED);
      tft.fillRect(w - 8, h - 8, 8, 8, ILI9341_GREEN);
      break;
    }

    case 9: {   // gradyan: gri, kırmızı, yeşil, mavi bantlar
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

    case 10: {  // animasyon (loop içinde ilerler)
      tft.fillScreen(ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(40, 4);
      tft.print("ANIMASYON");
      bx = 20; by = 40; vx = 3; vy = 2;
      break;
    }
  }
}

void stepAnimation() {
  int w = tft.width(), h = tft.height();
  const int S = 28;
  tft.fillRect(bx, by, S, S, ILI9341_BLACK);      // eski yeri sil
  bx += vx; by += vy;
  if (bx < 0)        { bx = 0;        vx = -vx; bcol = ILI9341_YELLOW; }
  if (bx > w - S)    { bx = w - S;    vx = -vx; bcol = ILI9341_CYAN; }
  if (by < 28)       { by = 28;       vy = -vy; bcol = ILI9341_MAGENTA; }
  if (by > h - S)    { by = h - S;    vy = -vy; bcol = ILI9341_GREEN; }
  tft.fillRect(bx, by, S, S, bcol);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  if (BL_PIN >= 0) { pinMode(BL_PIN, OUTPUT); digitalWrite(BL_PIN, BL_ON); }

  Serial.printf("Pinler: DC=%d RST=%d SCK=%d MOSI=%d CS=yok\n", TFT_DC, TFT_RST, TFT_SCK, TFT_MOSI);
  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
  tft.begin(SPI_HZ);
  applyGeometry();
  tft.invertDisplay(inverted);
  tft.fillScreen(ILI9341_BLACK);

  printHelp();
  drawPattern(pattern);
  printInfo();
  lastSwitch = millis();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    bool redraw = true, known = true;
    switch (c) {
      case 'n': pattern = (pattern + 1) % PATTERN_COUNT; break;
      case 'p': autoCycle = !autoCycle; redraw = false; break;
      case 'i': inverted = !inverted; tft.invertDisplay(inverted); redraw = false; break;
      case 'r': rotation = (rotation + 1) % 4; applyGeometry(); break;
      case 'c': bgr = !bgr; applyGeometry(); break;
      case 'h': printHelp(); redraw = false; break;
      default:  redraw = false; known = false; break;
    }
    if (redraw) { drawPattern(pattern); lastSwitch = millis(); }
    if (known && c != 'h') printInfo();
  }

  unsigned long now = millis();
  if (autoCycle && now - lastSwitch > 3000) {
    pattern = (pattern + 1) % PATTERN_COUNT;
    drawPattern(pattern);
    printInfo();
    lastSwitch = now;
  }
  if (pattern == 10 && now - lastAnim > 20) {
    lastAnim = now;
    stepAnimation();
  }
}
