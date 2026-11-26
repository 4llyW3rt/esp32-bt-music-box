# ESP32 Bluetooth / SD Music Box

*Türkçe: [README.tr.md](README.tr.md)*

A small music box built around an **ESP32**, a **BT201 Bluetooth/SD audio module** and a **2.8" ILI9341 TFT** (320×240, landscape). It plays music from a phone (Bluetooth) or an SD card, shows animations on the screen, and has a built-in Wi-Fi web page for changing the animation and controlling playback. The example case is a cat head: the screen is the mouth, and the mouth "sings" with the music.

> **Status:** works on the author's build. The wiring drawings are drafts, see [Schematics](#schematics).

## Features

- Bluetooth and SD-card playback through a BT201 module (UART AT commands)
- 320×240 screen with four pages: singing cat mouth, full-screen GIF, spectrum + record, Wi-Fi/QR page
- Intro and outro animations (GIF), outro frame stays on screen when the box is switched off
- Rotary encoder: volume (capped at 20 for amplifier/power safety), press = play/pause, long press = off
- Three buttons: previous, next, mode (short = change screen, long = Bluetooth/SD)
- Web page over the box's own Wi-Fi access point: transport controls, volume, screen change, upload/select GIFs
- Optional per-song GIF in SD mode (matches words in the file name, see [examples](examples/song_gifs))
- Small black-box log (`/log.txt`) for diagnosing resets and brown-outs

## Hardware

| Part | Notes |
|---|---|
| ESP32 DevKit V1 | 4 MB flash |
| BT201 Bluetooth/SD audio module | UART 115200 baud |
| PAM8403 amplifier module | 2 × speaker output |
| 2 × speaker | impedance/power not recorded; the PAM8403 suits 4–8 Ω, up to 3 W per channel |
| 2.8" ILI9341 SPI TFT (no CS pin on this module) | IPS panel, colours inverted in software |
| HW-040 rotary encoder with push button | |
| 3 × push button | previous, next, mode |
| 2 × 1000 µF 25 V electrolytic capacitor | one across VIN–GND, one at the PAM8403 (220 µF were tried too); see [troubleshooting](docs/TROUBLESHOOTING.md) |
| 5 V power supply, ideally ≥ 2 A | |
| microSD card | FAT32, mp3 files |

### Pin map (from the firmware)

| Function | ESP32 pin |
|---|---|
| TFT DC | GPIO 2 |
| TFT RST | GPIO 4 |
| TFT SCK | GPIO 18 |
| TFT MOSI | GPIO 23 |
| TFT CS | not used (`-1`) |
| BT201 RX ← ESP32 TX | GPIO 17 |
| BT201 TX → ESP32 RX | GPIO 16 |
| Encoder CLK | GPIO 32 |
| Encoder DT | GPIO 33 |
| Encoder SW (also wake-up pin) | GPIO 25 |
| Button Previous | GPIO 26 |
| Button Next | GPIO 27 |
| Button Mode | GPIO 14 |

Buttons and the encoder switch are active low (internal pull-ups). Power, audio and SD wiring of the BT201 and the PAM8403 depend on your modules, check their data sheets.

## Schematics

`schematics/` contains the author's Fritzing files (`.fzz`) and a pin diagram, [`schematics/wiring.svg`](schematics/wiring.svg). **These are drafts and are not a complete view of the build** (power, audio and ground connections are simplified). Check every connection yourself before powering up.

## Build and flash

Requirements: Arduino IDE or `arduino-cli`, ESP32 core **2.0.17**, libraries: Adafruit GFX, Adafruit ILI9341, AnimatedGIF (bitbank2), ArduinoJson **v7**. The QR code library is bundled as `qrcode_moore.*`.

Board settings: `esp32:esp32:esp32` with partition scheme **No OTA (2 MB APP / 2 MB SPIFFS)**, which gives a ~1.9 MB LittleFS area.

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota firmware/music_box
```

Then upload the sketch, and build and upload the animation partition (`data/` folder):

```bash
mklittlefs -c firmware/music_box/data -p 256 -b 4096 -s 1966080 fs.bin
esptool.py --chip esp32 --port COM15 --baud 115200 write_flash 0x210000 fs.bin
```

Writing `fs.bin` **erases GIFs uploaded over Wi-Fi**; flashing only the sketch (offset `0x10000`) keeps them.
If upload fails with *Wrong boot mode*, hold **BOOT** while plugging in USB.

## Using it

**Buttons / encoder**

| Control | Action |
|---|---|
| Encoder turn | volume (max 20) |
| Encoder press | play / pause |
| Encoder long press | power off (outro animation, then sleep; press again to wake) |
| Previous / Next | previous / next track |
| Mode, short | next screen: cat mouth → GIF → spectrum → Wi-Fi |
| Mode, long | switch Bluetooth ⇄ SD |

**Wi-Fi page.** Open the Wi-Fi screen, scan the QR code or connect to the access point shown (default password `12345678`, **change it in `data/config.json` before sharing your build**) and open `http://192.168.4.1`. You can control playback, change the screen, upload GIFs (max 320×240, 900 KB) and pick the active one.

**Per-song GIFs (SD mode).** If the playing file name contains `kuzey` or `kanunsuz`, `/default/kuzey.gif` or `/default/kanunsuz.gif` is shown full screen; changing track returns to the cat. Copy the sample GIFs from `examples/song_gifs/` into `data/default/` to try it, and edit `songGifCheck()` in `music_box.ino` for your own words.

## Tools

- [`tools/gif_converter`](tools/gif_converter): drag a GIF/PNG/JPG onto `gif_donustur.bat` (or run the window) to get a 320×240, size-reduced GIF. Crops to fill the screen, can trim a time range. Needs Python and Pillow.
- [`tools/gif_generators`](tools/gif_generators): the Python scripts that drew the intro, outro and sample GIFs (`OUT=folder python make_intro.py "A|B" "text"`). Needs Pillow.

## Case

`case/` holds the 3D files of the cat-head case (SolidWorks parts, STL, OpenSCAD, 3MF). The screen is the mouth, two oval holes are the eyes and a small hole is the nose.

## Repository layout

```
firmware/music_box/   Arduino sketch, web page, data/ (LittleFS content)
tools/                GIF converter and GIF generators
examples/song_gifs/   sample per-song GIFs
schematics/           Fritzing drafts, pin diagram
case/                 3D models
docs/                 troubleshooting
archive/              older sketches and earlier firmware versions
```

## Known problems and fixes

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md): brown-outs when the volume goes up, USB dropping while flashing, the BT201 jumping from SD to Bluetooth, screen going black in deep sleep.

## Music

No music is included. Use files you have the right to use.

## License

MIT, see [LICENSE](LICENSE).
