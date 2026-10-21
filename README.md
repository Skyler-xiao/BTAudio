# BTAudio

Bluetooth audio for the Wii U: plays the console's sound on Bluetooth headphones and speakers (A2DP).

BTAudio is built on top of [Bloopair](https://github.com/GaryOderNichts/Bloopair) by GaryOderNichts. It contains
all of Bloopair's controller support, plus audio streaming and an Aroma plugin to control it.

> **Status:** work in progress. It runs well on the developer's console, but it has only been tested with a few
> devices so far. Use at your own risk.

*Deutsche Anleitung weiter unten.*

## Features
- System-wide sound on Bluetooth headphones and speakers: Wii U Menu, HOME Menu, games, homebrew
- Pair, connect and disconnect in the Aroma plugin menu, devices are remembered
- Button combo to connect / disconnect without opening the menu (default: hold **L + R + Minus** for a second)
- Volume, sound source (GamePad / TV / automatic)
- Audio quality (High / Medium / Stable): less data means fewer dropouts at a distance
- Optional low latency mode
- On-screen notifications ("Connected to ...", "Disconnected")
- Headphone battery level in the menu (experimental, see below)
- Detects when a Bloopair update replaced the BTAudio module and restores it on request
- Menu in all 12 languages of the Wii U, following the console's language
- Bloopair's controller support keeps working, also while audio is playing

Tested with CMF Buds 2 and JBL Charge 5 (SBC codec).

## Installation
1. Download the latest `BTAudio_vX.Y.Z.zip` from the releases page.
2. Copy the `wiiu` folder from the zip to the root of your SD card and allow it to overwrite files.
   This places:
   - `wiiu/environments/aroma/modules/setup/30_bloopair.rpx` (**replaces an installed Bloopair**, controllers keep working)
   - `wiiu/environments/aroma/plugins/BTAudio.wps`
   - `wiiu/btaudio/module/30_bloopair.rpx` (backup of the module, used by "Restore BTAudio module")
   - `wiiu/apps/Koopair/Koopair.wuhb` (Bloopair's companion app)
3. Turn the console fully off and on again.

Requires [Aroma](https://wiiu.hacks.guide/) and Wii U firmware 5.5.5.

## Usage
Open the Aroma plugin menu (default: **L + D-Pad Down + Minus** on the GamePad) and select **Bluetooth audio**.
Entries that do something at once (connect, disconnect, search, remove, reset) are triggered with **A**.

**Pairing a new device**
1. "Pair new device": put the headphones in pairing mode.
2. "2. Search": press A.
3. Close the menu, wait about 20 seconds, open it again.
4. "4. Pair": pick your device.

**Next time:** "Connect to: <device>" with A, or hold **L + R + Minus**.

**Settings**
- Sound source, audio quality (default Medium), low latency, button combo, language
- "Show headphone battery (test)": see below
- "Remove: <device>" forgets one device, "Reset plugin" deletes all devices and settings
- "Log": off by default. For finding problems the plugin can write `wiiu/btaudio/plugin.txt` on the SD card,
  with a choice of topics.

## Battery level (experimental)
Headphones report their battery to a phone inside the hands-free profile (HFP). The console's Bluetooth stack has
no usable RFCOMM interface for applications, so BTAudio brings a small RFCOMM client and answers the hands-free
commands like a phone (without calls). When the switch is on and the headphones report a level, the status shows it,
for example "Connected: CMF Buds 2 (80 %)". If it does not work with a device, nothing else is affected.

## Known limitations
- New controllers cannot be paired (SYNC) while audio is playing: the search would interrupt the sound. Pair the
  controller first, then connect the headphones. Already paired controllers connect normally.
- Latency: mostly caused by the headphones' own buffer (SBC), noticeable in games. Low latency mode helps a bit.
- Only the SBC codec.
- Only Wii U firmware 5.5.5 (the IOS-PAD addresses are fixed).
- An update of Bloopair (Homebrew App Store, Aroma Updater) replaces the BTAudio module. The plugin notices it and
  offers "Restore BTAudio module" in its menu, then restart the console.
- If the console ever crashes while connecting to saved headphones, remove the device and pair it again.

## Building
The easiest way is the container in `tools/build`:
```
docker build -t btaudio-build tools/build
docker run --rm -u $(id -u):$(id -g) -v "$PWD":/work -w /work btaudio-build make dist
```
(Podman works the same, add `:Z` to the volume on SELinux systems.)
The result is in `dist/` (`dist/sd` has the SD card layout, plus a zip).

## Credits and license
- [Bloopair](https://github.com/GaryOderNichts/Bloopair) by GaryOderNichts: the base of the IOS-PAD patches, the
  loader and Koopair.
- libsbc 2.0 from BlueZ (LGPL 2.1, see `third_party/libsbc/COPYING.LIB`): SBC encoder.
- [Aroma / WUPS](https://github.com/wiiu-env): plugin system, notifications, button combos.

Licensed under the GNU General Public License v2 or later, see `LICENSE`.

---

## Deutsch

BTAudio schickt den Ton der Wii U an Bluetooth-Kopfhörer und -Lautsprecher. Es basiert auf Bloopair und enthält
dessen Controller-Unterstützung.

> **Stand:** in Arbeit. Läuft auf der Konsole des Entwicklers gut, ist aber erst mit wenigen Geräten getestet.

**Installation:** Den Ordner `wiiu` aus dem Zip ins Hauptverzeichnis der SD-Karte kopieren und überschreiben.
Ein vorhandenes Bloopair wird dabei ersetzt, Controller funktionieren weiter. Konsole ganz aus- und wieder einschalten.
Voraussetzung: Aroma und Firmware 5.5.5.

**Benutzung:** Aroma-Plugin-Menü öffnen (**L + Steuerkreuz unten + Minus**) → **Bluetooth-Audio**.
Einträge, die sofort etwas tun, löst du mit **A** aus.
- Koppeln: "Neues Gerät koppeln" → Kopfhörer in den Kopplungsmodus → "2. Suchen" mit A → Menü schließen, etwa
  20 Sekunden warten, wieder öffnen → "4. Koppeln" → Gerät wählen.
- Danach: "Verbinden mit: <Gerät>" mit A, oder **L + R + Minus** eine Sekunde halten.
- Einstellungen: Tonquelle, Audioqualität, geringe Verzögerung, Tastenkürzel, Sprache, Akkustand (Test),
  einzelne Geräte entfernen, Plugin zurücksetzen, Log (standardmäßig aus).

**Einschränkungen:**
- Neue Controller nicht koppeln, während Ton läuft: erst den Controller koppeln, dann die Kopfhörer verbinden.
- Nur der Codec SBC und nur Firmware 5.5.5.
- Ein Bloopair-Update (App Store, Aroma Updater) ersetzt das BTAudio-Modul. Das Plugin meldet das und bietet im Menü
  "BTAudio-Modul wiederherstellen" an, danach die Konsole neu starten.
- Stürzt die Konsole beim Verbinden mit gespeicherten Kopfhörern ab: das Gerät entfernen und neu koppeln.
