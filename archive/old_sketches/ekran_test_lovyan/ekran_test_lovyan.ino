/*
  ST7735S 1.8" (128x160) EKRAN TEST - LovyanGFX KÜTÜPHANESİ
  ----------------------------------------------------------
  Kütüphane: Arduino Library Manager'dan "LovyanGFX" (lovyan03) kur.

  Pinler: CS=15  RESET=4  A0(DC)=2  SCK=18  SDA(MOSI)=23

  Serial Monitor: 115200 baud, alt satır ayarı: "Newline" (Yeni satır)

  Cetvel deseniyle açılır: sol üst KIRMIZI, sağ alt YESIL kare görünmeli,
  çizgiler ve piksel numaraları tüm ekranı kaplamalı.

  KOMUTLAR (harf, sonra Enter):
    X = kolon ofseti +8     Y = satır ofseti +8
    x = kolon ofseti +1     y = satır ofseti +1
    k32 = kolon ofseti 32   s0 = satır ofseti 0   (sayıyı sen yaz)
    z = ofsetleri sıfırla
    o = ayna/yön ayarı (offset_rotation 0..7, 4-7 aynalı)
    g = RGB/BGR sırası      i = renkleri ters çevir
    f = bellek boyutu: 132x162 <-> 128x160
    r = döndür              n = sonraki desen
    h = yardım
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
 public:
  lgfx::Panel_ST7735S _panel;
  lgfx::Bus_SPI       _bus;

  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host   = VSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 8000000;     // 8 MHz, güvenli
      cfg.freq_read  = 0;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 18;
      cfg.pin_mosi = 23;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = 4;
      cfg.pin_busy = -1;
      cfg.panel_width   = 128;
      cfg.panel_height  = 160;
      cfg.memory_width  = 132;
      cfg.memory_height = 162;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable   = false;
      cfg.invert     = false;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

LGFX lcd;

int  offX = 0, offY = 0, offRot = 0, rot = 0;
bool inv = false, rgbOrder = false, bigMem = true;
int  pattern = 6;                      // cetvelle başla
const int PATTERN_COUNT = 7;
const char* patternNames[PATTERN_COUNT] = {
  "1) Kirmizi", "2) Yesil", "3) Mavi", "4) Beyaz",
  "5) Renk cubuklari", "6) Cerceve", "7) CETVEL"
};

void printHelp() {
  Serial.println();
  Serial.println("=== EKRAN TEST (LovyanGFX) ===");
  Serial.println("X/Y=ofset+8  x/y=+1  k32/s0=tam deger  z=sifirla");
  Serial.println("o=ayna/yon  g=RGB/BGR  i=invert  f=bellek 132x162/128x160");
  Serial.println("r=dondur  n=desen  h=yardim");
}

void printInfo() {
  Serial.printf("[OfsetX=%d OfsetY=%d | offset_rotation=%d | rot=%d | BGR=%s | invert=%s | bellek=%s | Desen: %s]\n",
                offX, offY, offRot, rot, rgbOrder ? "EVET" : "hayir", inv ? "EVET" : "hayir",
                bigMem ? "132x162" : "128x160", patternNames[pattern]);
}

void applyCfg() {
  auto cfg = lcd._panel.config();
  cfg.offset_x = offX;
  cfg.offset_y = offY;
  cfg.offset_rotation = offRot;
  cfg.invert = inv;
  cfg.rgb_order = rgbOrder;
  cfg.memory_width  = bigMem ? 132 : 128;
  cfg.memory_height = bigMem ? 162 : 160;
  lcd._panel.config(cfg);
  lcd.init();
  lcd.setRotation(rot);
  lcd.fillScreen(0x0000);
}

void fillLabel(uint32_t bg, const char* text, uint32_t fg) {
  lcd.fillScreen(bg);
  lcd.setTextSize(2);
  lcd.setTextColor(fg);
  int tw = strlen(text) * 12;
  lcd.setCursor((lcd.width() - tw) / 2, lcd.height() / 2 - 8);
  lcd.print(text);
}

void drawPattern(int p) {
  int w = lcd.width(), h = lcd.height();
  switch (p) {
    case 0: fillLabel(0xF800, "KIRMIZI", 0xFFFF); break;
    case 1: fillLabel(0x07E0, "YESIL",   0x0000); break;
    case 2: fillLabel(0x001F, "MAVI",    0xFFFF); break;
    case 3: fillLabel(0xFFFF, "BEYAZ",   0x0000); break;

    case 4: {
      uint16_t cols[8] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000};
      lcd.fillScreen(0x0000);
      int bw = w / 8;
      for (int i = 0; i < 8; i++) lcd.fillRect(i * bw, 0, bw, h, cols[i]);
      lcd.drawRect(7 * bw, 0, bw, h, 0xFFFF);
      break;
    }

    case 5: {
      lcd.fillScreen(0x0000);
      lcd.drawRect(0, 0, w, h, 0xFFFF);
      lcd.drawRect(2, 2, w - 4, h - 4, 0xFFE0);
      lcd.fillRect(4, 4, 10, 10, 0xF800);
      lcd.fillRect(w - 14, 4, 10, 10, 0x07E0);
      lcd.fillRect(4, h - 14, 10, 10, 0x001F);
      lcd.fillRect(w - 14, h - 14, 10, 10, 0xF81F);
      lcd.drawLine(0, 0, w - 1, h - 1, 0x07FF);
      lcd.drawLine(w - 1, 0, 0, h - 1, 0x07FF);
      lcd.setTextSize(1);
      lcd.setTextColor(0xFFFF);
      lcd.setCursor(20, h / 2 - 12); lcd.print("Sol ust: KIRMIZI");
      lcd.setCursor(20, h / 2);      lcd.print("Sag ust: YESIL");
      lcd.setCursor(20, h / 2 + 12); lcd.print("Sol alt: MAVI");
      break;
    }

    case 6: {   // CETVEL
      lcd.fillScreen(0x0000);
      lcd.setTextSize(1);
      for (int x = 0; x < w; x += 16) {
        lcd.drawFastVLine(x, 0, h, (x % 32 == 0) ? 0x7BEF : 0x39E7);
        lcd.setTextColor(0xFFE0);
        lcd.setCursor(x + 2, 2 + ((x / 16) % 2) * 10);
        lcd.print(x);
      }
      for (int y = 16; y < h; y += 16) {
        lcd.drawFastHLine(0, y, w, (y % 32 == 0) ? 0x7BEF : 0x39E7);
        lcd.setTextColor(0x07FF);
        lcd.setCursor(2, y + 2);
        lcd.print(y);
      }
      lcd.fillRect(0, 0, 6, 6, 0xF800);
      lcd.fillRect(w - 6, h - 6, 6, 6, 0x07E0);
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(150);
  delay(300);
  lcd.init();
  lcd.setRotation(rot);
  lcd.fillScreen(0x0000);
  printHelp();
  drawPattern(pattern);
  printInfo();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    bool redraw = true, known = true, reinit = false;
    switch (c) {
      case 'n': pattern = (pattern + 1) % PATTERN_COUNT; break;
      case 'x': offX = (offX + 1) % 64; reinit = true; break;
      case 'y': offY = (offY + 1) % 64; reinit = true; break;
      case 'X': offX = (offX + 8) % 64; reinit = true; break;
      case 'Y': offY = (offY + 8) % 64; reinit = true; break;
      case 'k': offX = constrain(Serial.parseInt(), 0, 63); reinit = true; break;
      case 's': offY = constrain(Serial.parseInt(), 0, 63); reinit = true; break;
      case 'z': offX = offY = 0; reinit = true; break;
      case 'o': offRot = (offRot + 1) % 8; reinit = true; break;
      case 'g': rgbOrder = !rgbOrder; reinit = true; break;
      case 'i': inv = !inv; reinit = true; break;
      case 'f': bigMem = !bigMem; reinit = true; break;
      case 'r': rot = (rot + 1) % 4; lcd.setRotation(rot); lcd.fillScreen(0x0000); break;
      case 'h': printHelp(); redraw = false; break;
      default:  redraw = false; known = false; break;
    }
    if (reinit) applyCfg();
    if (redraw) drawPattern(pattern);
    if (known && c != 'h') printInfo();
  }
}
