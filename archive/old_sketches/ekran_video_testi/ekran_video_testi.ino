/*
  EKRAN VIDEO TESTİ  (iade/şikayet videosu için)
  -----------------------------------------------
  Kendi kendine döner, Serial Monitor gerekmez.
  Her ekran 3 sn kalır, toplam 8 adım (~24 sn), sonra başa döner.
  Alt çizgide çalışma süresi sayacı görünür (video canlı çekildiğini gösterir).

  Pinler: CS=15  RESET=4  A0(DC)=2  SCK=18  SDA(MOSI)=23
  Kütüphaneler: Adafruit GFX, Adafruit ST7735 and ST7789
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

const int STEP_COUNT = 8;
const unsigned long STEP_MS = 3000;

const uint16_t FILL_BG[5] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_WHITE, ST77XX_BLACK};
const uint16_t FILL_FG[5] = {ST77XX_WHITE, ST77XX_BLACK, ST77XX_WHITE, ST77XX_BLACK, ST77XX_WHITE};
const char*    FILL_TX[5] = {"KIRMIZI", "YESIL", "MAVI", "BEYAZ", "SIYAH"};

int lastStep = -1;
unsigned long lastSec = 9999999;

void drawCounter(unsigned long sec) {
  int w = tft.width(), h = tft.height();
  tft.fillRect(0, h - 12, w, 12, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, h - 10);
  tft.print("Sure: ");
  tft.print(sec);
  tft.print(" sn");
}

void drawStep(int s) {
  int w = tft.width(), h = tft.height();
  tft.setTextWrap(false);

  if (s < 5) {                               // düz renk dolgular
    tft.fillScreen(FILL_BG[s]);
    tft.setTextSize(2);
    tft.setTextColor(FILL_FG[s]);
    int tw = strlen(FILL_TX[s]) * 12;
    tft.setCursor((w - tw) / 2, h / 2 - 16);
    tft.print(FILL_TX[s]);
    tft.setTextSize(1);
    tft.setCursor(10, h / 2 + 4);
    tft.print("1.8 TFT ST7735S 128x160");
  }
  else if (s == 5) {                         // renk çubukları
    uint16_t cols[8] = {ST77XX_WHITE, ST77XX_YELLOW, ST77XX_CYAN, ST77XX_GREEN,
                        ST77XX_MAGENTA, ST77XX_RED, ST77XX_BLUE, ST77XX_BLACK};
    tft.fillScreen(ST77XX_BLACK);
    int bw = w / 8;
    for (int i = 0; i < 8; i++) tft.fillRect(i * bw, 0, bw, h, cols[i]);
    tft.drawRect(7 * bw, 0, bw, h, ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.setCursor(8, h / 2 - 4);
    tft.print(" RENK CUBUKLARI ");
  }
  else if (s == 6) {                         // çerçeve ve köşeler
    tft.fillScreen(ST77XX_BLACK);
    tft.drawRect(0, 0, w, h, ST77XX_WHITE);
    tft.drawRect(2, 2, w - 4, h - 4, ST77XX_YELLOW);
    tft.fillRect(4, 4, 10, 10, ST77XX_RED);
    tft.fillRect(w - 14, 4, 10, 10, ST77XX_GREEN);
    tft.fillRect(4, h - 26, 10, 10, ST77XX_BLUE);
    tft.fillRect(w - 14, h - 26, 10, 10, ST77XX_MAGENTA);
    tft.drawLine(0, 0, w - 1, h - 1, ST77XX_CYAN);
    tft.drawLine(w - 1, 0, 0, h - 1, ST77XX_CYAN);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(20, h / 2 - 12); tft.print("Sol ust: KIRMIZI");
    tft.setCursor(20, h / 2);      tft.print("Sag ust: YESIL");
    tft.setCursor(20, h / 2 + 12); tft.print("Sol alt: MAVI");
  }
  else {                                     // yazı testi
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(2, 4);   tft.print("YAZI TESTI (boyut 1)");
    tft.setTextSize(2);
    tft.setCursor(2, 20);  tft.print("Boyut 2");
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(2, 44);  tft.print("ABC");
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(2, 80);  tft.print("0123456789");
    tft.setCursor(2, 92);  tft.print("abcdefghijklmnop");
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(2, 108); tft.print("Ekran sol tarafi: ");
    tft.setCursor(2, 120); tft.print("beyaz seritte yazilmiyor");
  }
  lastSec = 9999999;     // sayaç yeniden çizilsin
}

void setup() {
  Serial.begin(115200);
  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  unsigned long now = millis();
  int step = (now / STEP_MS) % STEP_COUNT;
  if (step != lastStep) {
    lastStep = step;
    drawStep(step);
  }
  unsigned long sec = now / 1000;
  if (sec != lastSec) {
    lastSec = sec;
    drawCounter(sec);
  }
}
