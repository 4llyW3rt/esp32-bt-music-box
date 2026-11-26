# Troubleshooting / Sorun giderme

Problems met while building this box, and what fixed them.
Bu kutuyu yaparken karşılaşılan sorunlar ve çözümleri.

## 1. Resets or crackling when the volume goes up (brown-out)
## Ses yükselince reset ya da çıtırtı (brownout)

- **Symptom / Belirti:** the box restarts or sounds bad at higher volume; `/log.txt` shows repeated brown-out resets, even with Wi-Fi off. / Yüksek sesde cihaz resetleniyor ya da ses bozuluyor; `/log.txt` içinde Wi-Fi kapalıyken bile tekrarlayan brownout kaydı var.
- **Cause / Sebep:** the 5 V rail sags when the amplifier draws current. / Amfi akım çekince 5 V hattı düşüyor.
- **Fix / Çözüm:**
  - two 1000 µF 25 V capacitors: one across VIN–GND, one at the PAM8403 supply / iki adet 1000 µF 25 V kondansatör: biri VIN–GND, biri PAM8403 beslemesi
  - the volume is capped at 20 in firmware (`max_volume` in `data/config.json`) / ses yazılımda 20 ile sınırlı (`data/config.json` içinde `max_volume`)
  - ideas if it still happens / hâlâ olursa: feed the PAM8403 straight from the 5 V supply (common ground), add a 100 nF ceramic capacitor, use a ≥ 2 A supply, keep Wi-Fi off when not needed (`wifi_always_on`)

## 2. USB drops while flashing / Yüklerken USB kopması

- Replug the cable, use a short data cable and a rear PC port. / Kabloyu yeniden tak, kısa ve veri destekli kablo ve PC'nin arka portunu kullan.
- Disconnect the PAM8403 5 V wire during flashing. / Yüklerken PAM8403'ün 5 V kablosunu çıkar.
- Use 115200 baud. A full flash (app + animations) takes ~3 minutes. / 115200 baud kullan. Tam yükleme (uygulama + animasyonlar) yaklaşık 3 dakika sürer.
- *Wrong boot mode detected*: hold **BOOT** while plugging in USB. / *Wrong boot mode detected*: USB'yi takarken **BOOT** tuşunu basılı tut.

## 3. BT201 jumps from SD to Bluetooth / BT201 SD'den Bluetooth'a atlıyor

- **Cause / Sebep:** a phone notification or a connection event switches the module to Bluetooth mode. / Telefon bildirimi ya da bağlantı olayı modülü Bluetooth moduna alıyor.
- **Fix / Çözüm:** the firmware remembers that SD was wanted and sends `AT+CM03` again, at most 3 times, 5 s apart. / Yazılım SD'nin istendiğini hatırlıyor ve `AT+CM03` komutunu en çok 3 kez, 5 sn arayla tekrar gönderiyor.
- Note: in Bluetooth mode the module reports no track title; in SD mode it reports the file name. / Not: Bluetooth modunda modül parça adı bildirmez, SD modunda dosya adını bildirir.

## 4. Screen goes black after the outro / Kapanıştan sonra ekran kararıyor

- **Cause / Sebep:** in deep sleep the ESP32 pins float, which blanks the TFT. Holding the reset pin did not help. / Derin uykuda ESP32 pinleri boşta kalıyor ve TFT kararıyor. Reset pinini kilitlemek işe yaramadı.
- **Fix / Çözüm:** use light sleep (pins keep their state) and restart the ESP32 when the encoder button wakes it. Wi-Fi is switched off first. / Hafif uyku kullanılıyor (pinler korunur), enkoder tuşu uyandırınca ESP32 yeniden başlatılıyor. Önce Wi-Fi kapatılıyor.
- **Limit / Sınır:** without a power switch (e.g. a MOSFET) the board stays powered, only the screen content stays. / Güç anahtarı (ör. MOSFET) yoksa kart beslenmeye devam eder; yalnızca ekran görüntüsü kalır.

## 5. Display colours or orientation wrong / Ekran renk ya da yön hatası

- Colours inverted: `TFT_INVERT`. Red/blue swapped: `TFT_BGR`. Upside down: `TFT_ROTATION` 1 or 3. Artefacts: lower `SPI_HZ`. / Renkler ters: `TFT_INVERT`. Kırmızı-mavi ters: `TFT_BGR`. Baş aşağı: `TFT_ROTATION` 1 ya da 3. Artefakt: `SPI_HZ` değerini düşür.

## 6. Uploaded GIF refused / Yüklenen GIF reddediliyor

- Maximum 320×240 and 900 KB. Use `tools/gif_converter`. / En çok 320×240 ve 900 KB. `tools/gif_converter` aracını kullan.
- Flashing the animation partition (`fs.bin`) erases uploaded GIFs. / Animasyon bölümünü (`fs.bin`) yazmak yüklenen GIF'leri siler.
