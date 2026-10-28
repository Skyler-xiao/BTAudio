# Changelog

## 1.1.0

### Bloopair
- Updated to Bloopair 1.1.0: support for Bluetooth Low Energy controllers (Switch 2 Joy-Con, Pro Controller and
  GameCube Controller, Xbox Series S/X, Xbox One S/X with the latest firmware, Stadia Controller). Controller
  pairings are now stored on the SD card under `wiiu/bloopair/devices`.
- BLE controllers have not been tested with BTAudio yet, reports are welcome.

### Devices
- Fixed: removing a device and pairing it again without restarting the console reused the old key, so the device
  was not saved and disappeared after disconnecting.

### Plugin
- The plugin menu names Xiao as the author.

### Known issue
- Opening or closing the HOME Menu can interrupt the audio for a few seconds (also in 1.0.0).

## 1.0.0

First release.

### Audio
- System-wide Bluetooth audio (A2DP, SBC) from the Wii U Menu, HOME Menu, games and homebrew
- Sound source: GamePad, TV or automatic
- Volume control
- Three audio quality levels (High, Medium, Stable); a change takes effect by reconnecting when the menu is closed
- Low latency mode
- Stable delay on weak connections: audio that queued up during radio congestion is skipped instead of delayed

### Devices
- Pairing, connecting and disconnecting from the Aroma plugin menu
- Saved devices, removable one by one
- Button combo to connect or disconnect (default: hold L + R + Minus)
- Battery level of the connected device over the Hands-Free Profile (experimental), with a low battery notification

### Plugin
- Notifications for connection changes
- Menu in all 12 system languages
- Optional diagnostic log on the SD card (off by default)
- Reset of all plugin settings
- Detection of a Bloopair update that replaced the BTAudio module, with a restore option in the menu

### Bloopair
- Based on Bloopair 1.0.4; controller support is unchanged and keeps working while audio plays
- Fixed: the controller requests of the IPC interface could read one entry past the controller list
