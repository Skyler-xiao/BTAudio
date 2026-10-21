# BTAudio

Bluetooth audio (A2DP) for the Wii U. BTAudio streams the console's sound to Bluetooth headphones and speakers.

BTAudio is based on [Bloopair](https://github.com/GaryOderNichts/Bloopair) by GaryOderNichts and includes all of
its controller support. It adds audio streaming to the Bluetooth stack and an Aroma plugin to control it.

> **Status:** Work in progress. BTAudio has been tested on a single console with a small number of devices. Use at your own Risk.

## Features
- System-wide audio: Wii U Menu, HOME Menu, games and homebrew
- Pairing, connecting and disconnecting from the Aroma plugin menu; paired devices are saved
- Button combo to connect or disconnect without opening the menu (default: hold L + R + Minus for one second)
- Volume control and choice of sound source (GamePad, TV or automatic)
- Three audio quality levels; lower quality reduces dropouts at longer range
- Optional low latency mode
- On-screen notifications for connection changes and low headphone battery
- Headphone battery level in the menu (experimental)
- Detection and restore of the BTAudio module after a Bloopair update
- Menu available in all 12 system languages
- Full Bloopair controller support, also while audio is playing

Tested devices: CMF Buds 2, JBL Charge 5.

## Requirements
- Wii U on firmware 5.5.5
- [Aroma](https://wiiu.hacks.guide/)

## Installation
1. Download the latest `BTAudio_vX.Y.Z.zip` from the releases page.
2. Extract the `wiiu` folder to the root of the SD card and overwrite existing files. The archive contains:

   | File | Purpose |
   |---|---|
   | `wiiu/environments/aroma/modules/setup/30_bloopair.rpx` | BTAudio module; replaces an installed Bloopair |
   | `wiiu/environments/aroma/plugins/BTAudio.wps` | Plugin |
   | `wiiu/btaudio/module/30_bloopair.rpx` | Backup of the module, used for restoring it |
   | `wiiu/apps/Koopair/Koopair.wuhb` | Bloopair's companion app |

3. Turn the console off completely and on again.

## Usage
Open the Aroma plugin menu (L + D-Pad Down + Minus on the GamePad) and select **Bluetooth audio**.
Actions such as connecting, searching or removing a device are confirmed with **A**.

### Pairing a device
1. Select **Pair new device** and put the headphones into pairing mode.
2. Select **Start search** and press A.
3. Close the menu and wait about 20 seconds.
4. Reopen the menu, select **Pair** and choose the device.

Saved devices are listed in the main menu and can be connected with **Connect to: \<device\>** or the button combo.

### Settings
| Setting | Description |
|---|---|
| Sound source | GamePad, TV or automatic |
| Audio quality | High, Medium (default) or Stable |
| Low latency | Smaller audio buffer; may cause dropouts on weak connections |
| Show headphone battery (test) | Experimental, see below |
| Button combo | Combo for connecting and disconnecting |
| Language | Menu language; follows the system language by default |
| Remove | Removes a single saved device |
| Reset plugin | Removes all devices and restores the default settings |
| Log | Writes diagnostic output to `wiiu/btaudio/plugin.txt`; disabled by default |

## Battery level (experimental)
Headphones report their battery level through the Hands-Free Profile (HFP). The Wii U's Bluetooth stack does not
expose a usable RFCOMM interface, so BTAudio implements a minimal RFCOMM client and responds to the headset's HFP
commands as an audio gateway, without call support. When enabled, the reported level is shown next to the device
name, for example `Connected: CMF Buds 2 (80 %)`. Devices that do not report a level are not otherwise affected.

## Known limitations
- New controllers cannot be paired while audio is playing, because the device search interrupts the stream.
  Pair the controller first and connect the headphones afterwards. Previously paired controllers are not affected.
- Most of the latency comes from the headphones' SBC buffer. Low latency mode reduces it slightly.
- SBC is the only supported codec.
- Only firmware 5.5.5 is supported, as the IOS-PAD addresses are fixed.
- Updating Bloopair (Homebrew App Store, Aroma Updater) replaces the BTAudio module. The plugin detects this and
  offers **Restore BTAudio module** in its menu. Restart the console afterwards.
- If the console crashes while connecting to a saved device, remove the device and pair it again.

## Building
Build with the container in `tools/build`:
```
docker build -t btaudio-build tools/build
docker run --rm -u $(id -u):$(id -g) -v "$PWD":/work -w /work btaudio-build make dist
```
Podman works the same way; on SELinux systems, add `:Z` to the volume option.
The output is written to `dist/`: `dist/sd` contains the SD card layout, alongside a release archive.

## Credits
- [Bloopair](https://github.com/GaryOderNichts/Bloopair) by GaryOderNichts: IOS-PAD patching, loader and Koopair
- [libsbc](https://git.kernel.org/pub/scm/bluetooth/sbc.git) 2.0 from BlueZ (LGPL-2.1, see `third_party/libsbc/COPYING.LIB`): SBC encoder
- [wiiu-env](https://github.com/wiiu-env): Aroma, WUPS, notifications and button combo modules

## License
GNU General Public License v2 or later. See `LICENSE`.
