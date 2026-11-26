# ESP32 Bluetooth / SD Müzik Kutusu

*English: [README.md](README.md)*

**ESP32**, **BT201 Bluetooth/SD ses modülü** ve **2,8" ILI9341 TFT** (320×240, yatay) ile yapılmış küçük bir müzik kutusu. Telefondan (Bluetooth) ya da SD karttan müzik çalar, ekranda animasyon gösterir, kendi Wi-Fi sayfasından animasyon seçmeni ve çalmayı kontrol etmeni sağlar. Örnek kasa bir kedi kafası: ekran kedinin ağzı, ağız müzikle birlikte "şarkı söyler".

> **Durum:** yapanın kendi cihazında çalışıyor. Bağlantı çizimleri taslaktır, bkz. [Şemalar](#şemalar).

## Özellikler

- BT201 ile Bluetooth ve SD kart çalma (UART AT komutları)
- Dört sayfalı 320×240 ekran: şarkı söyleyen kedi ağzı, tam ekran GIF, spektrum + plak, Wi-Fi/QR sayfası
- Açılış ve kapanış animasyonu (GIF). Kapatınca kapanışın son karesi ekranda kalır
- Döner enkoder: ses (güç ve amfi güvenliği için en çok 20), basma = oynat/duraklat, uzun basma = kapat
- Üç buton: önceki, sonraki, mod (kısa = ekran değiştir, uzun = Bluetooth/SD)
- Kutunun kendi Wi-Fi ağından web sayfası: çalma kontrolü, ses, ekran değiştirme, GIF yükleme/seçme
- SD modunda isteğe bağlı şarkıya özel GIF (dosya adındaki kelimeye göre, bkz. [örnekler](examples/song_gifs))
- Reset ve voltaj düşmesi (brownout) teşhisi için küçük kara kutu kaydı (`/log.txt`)

## Donanım

| Parça | Not |
|---|---|
| ESP32 DevKit V1 | 4 MB flash |
| BT201 Bluetooth/SD ses modülü | UART 115200 baud |
| PAM8403 amfi modülü | 2 hoparlör çıkışı |
| 2 × hoparlör | empedans/güç kaydedilmedi; PAM8403 kanal başına 3 W'a kadar 4–8 Ω hoparlöre uygundur |
| 2,8" ILI9341 SPI TFT (bu modülde CS pini yok) | IPS panel, renkler yazılımla ters çevrilir |
| HW-040 döner enkoder (butonlu) | |
| 3 × buton | önceki, sonraki, mod |
| 2 × 1000 µF 25 V elektrolitik kondansatör | biri VIN–GND arasına, biri PAM8403'e (220 µF da denendi); bkz. [sorunlar](docs/TROUBLESHOOTING.md) |
| 5 V güç kaynağı, tercihen ≥ 2 A | |
| microSD kart | FAT32, mp3 dosyaları |

### Pin tablosu (yazılımdan)

| İşlev | ESP32 pini |
|---|---|
| TFT DC | GPIO 2 |
| TFT RST | GPIO 4 |
| TFT SCK | GPIO 18 |
| TFT MOSI | GPIO 23 |
| TFT CS | kullanılmıyor (`-1`) |
| BT201 RX ← ESP32 TX | GPIO 17 |
| BT201 TX → ESP32 RX | GPIO 16 |
| Enkoder CLK | GPIO 32 |
| Enkoder DT | GPIO 33 |
| Enkoder SW (uyandırma pini) | GPIO 25 |
| Buton Önceki | GPIO 26 |
| Buton Sonraki | GPIO 27 |
| Buton Mod | GPIO 14 |

Butonlar ve enkoder tuşu aktif-düşük (dahili pull-up). BT201 ve PAM8403'ün güç, ses ve SD bağlantıları modüllerine göre değişir, veri sayfalarına bak.

## Şemalar

`schematics/` klasöründe yapanın Fritzing dosyaları (`.fzz`) ve bir pin çizimi, [`schematics/wiring.svg`](schematics/wiring.svg), var. **Bunlar taslaktır, yapının tam görünümü değildir** (güç, ses ve toprak bağlantıları sadeleştirilmiştir). Güç vermeden önce her bağlantıyı kendin kontrol et.

## Derleme ve yükleme

Gerekenler: Arduino IDE ya da `arduino-cli`, ESP32 çekirdeği **2.0.17**, kütüphaneler: Adafruit GFX, Adafruit ILI9341, AnimatedGIF (bitbank2), ArduinoJson **v7**. QR kütüphanesi `qrcode_moore.*` olarak projeye dahil.

Kart ayarı: `esp32:esp32:esp32`, bölüm şeması **No OTA (2 MB APP / 2 MB SPIFFS)**. Bu, yaklaşık 1,9 MB'lık LittleFS alanı verir.

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota firmware/music_box
```

Sonra sketch'i yükle; animasyon bölümünü (`data/` klasörü) da derleyip yükle:

```bash
mklittlefs -c firmware/music_box/data -p 256 -b 4096 -s 1966080 fs.bin
esptool.py --chip esp32 --port COM15 --baud 115200 write_flash 0x210000 fs.bin
```

`fs.bin` yazmak Wi-Fi ile yüklediğin **GIF'leri siler**; yalnızca sketch'i yazarsan (`0x10000`) GIF'ler kalır.
Yükleme *Wrong boot mode* hatası verirse USB'yi takarken **BOOT** tuşunu basılı tut.

## Kullanım

**Butonlar / enkoder**

| Kontrol | İşlev |
|---|---|
| Enkoder çevir | ses (en çok 20) |
| Enkoder bas | oynat / duraklat |
| Enkoder uzun bas | kapat (kapanış animasyonu, sonra uyku; tekrar basınca uyanır) |
| Önceki / Sonraki | önceki / sonraki parça |
| Mod, kısa | sonraki ekran: kedi ağzı → GIF → spektrum → Wi-Fi |
| Mod, uzun | Bluetooth ⇄ SD |

**Wi-Fi sayfası.** Wi-Fi ekranını aç, QR kodu okut ya da gösterilen ağa bağlan (varsayılan şifre `12345678`, **paylaşmadan önce `data/config.json` içinde değiştir**) ve `http://192.168.4.1` adresini aç. Çalmayı kontrol edebilir, ekranı değiştirebilir, GIF yükleyebilir (en çok 320×240, 900 KB) ve etkin GIF'i seçebilirsin.

**Şarkıya özel GIF (SD modu).** Çalan dosyanın adında `kuzey` ya da `kanunsuz` geçerse `/default/kuzey.gif` ya da `/default/kanunsuz.gif` tam ekran gösterilir; parça değişince kediye dönülür. Denemek için `examples/song_gifs/` içindeki örnek GIF'leri `data/default/` klasörüne kopyala. Kendi kelimelerin için `music_box.ino` içindeki `songGifCheck()` fonksiyonunu düzenle.

## Araçlar

- [`tools/gif_converter`](tools/gif_converter): bir GIF/PNG/JPG dosyasını `gif_donustur.bat` üstüne sürükle (ya da pencereyi aç), 320×240 ve küçültülmüş GIF alırsın. Ekranı dolduracak şekilde kırpar, zaman aralığı kesebilir. Python ve Pillow gerekir.
- [`tools/gif_generators`](tools/gif_generators): açılış, kapanış ve örnek GIF'leri çizen Python betikleri (`OUT=klasor python make_intro.py "A|B" "yazı"`). Pillow gerekir.

## Kasa

`case/` klasöründe kedi kafası kasanın 3B dosyaları var (SolidWorks parçaları, STL, OpenSCAD, 3MF). Ekran ağız, iki oval delik göz, küçük delik burun.

## Depo yapısı

```
firmware/music_box/   Arduino sketch, web sayfası, data/ (LittleFS içeriği)
tools/                GIF dönüştürücü ve GIF üreticiler
examples/song_gifs/   şarkıya özel örnek GIF'ler
schematics/           Fritzing taslakları, pin çizimi
case/                 3B modeller
docs/                 sorun giderme
archive/              eski denemeler ve önceki yazılım sürümleri
```

## Bilinen sorunlar ve çözümler

Bkz. [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md): ses yükselince voltaj düşmesi (brownout), yüklemede USB kopması, BT201'in SD'den Bluetooth'a atlaması, derin uykuda ekranın kararması.

## Müzik

Depoda müzik yok. Kullanma hakkın olan dosyaları kullan.

## Lisans

MIT, bkz. [LICENSE](LICENSE).
