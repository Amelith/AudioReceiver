# AudioReceiver

An ESP32 A2DP Bluetooth Audio receiver, outputting to I2S.  
Mostly intended for personal usage but yea  
Works for the original esp32, idk about other versions.  
I use it with an Adafruit UDA1334.

Features, in addition to bluetooth audio:
- Controlling volume via Bluetooth Serial
- Button to play/pause from the ESP device

Depends on [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) and [AudioTools](https://github.com/pschatzmann/arduino-audio-tools)

## Installation & Usage

```bash
git clone https://github.com/Amelith/AudioReceiver.git
cd AudioReceiver
```

Get (PlatformIO)[https://platformio.org/].  

Configure Bluetooth name, default volume and I2S/Button pins in src/config.h. Defaults are listed there.  
You can also change the volume without flashing using BluetoothSerial, either until device restart or permanently.  
To permanently configure settings, you need to flash a config file:
- Create data/config.ini (or whatever name you specified in src/config.h; needs to be within the data/ directory)
- Write default configuration. Example:
```ini
# volume
v=85
# button enable
b=1
# mode (either bluetooth or wifi) (currently unused)
m=bluetooth
# ssid (currently unused)
s=MyWifiSSID
# password (currently unused)
p=MyWiFiPassword
```
- After creating the config file, run "Build Filesystem Image" in PlatformIO or `platformio run --target buildfs --environment esp32dev`
- Flash the image to ESP using "Upload Filesystem Image" or `platformio run --target uploadfs --environment esp32dev`

**Flashing the program using PlatformIO**
Plug the ESP32 into the system.  
Flash the program with one of those two methods:
- Open the PlatformIO menu (vscode: left sidebar) and click "Upload" or "Upload and monitor"
- Run `platformio run --target upload` or `platformio run --target upload --target monitor` (Make sure the platformio binary is in PATH)

WiFi mode (using WiFi for communication instead of BTSerial) may be supported at one point in the future, although currently the config options are useless.  

### BluetoothSerial Commands

There's BTSerial Terminals available on the Play Store if you don't want to build your own app for it.  
The same commands are also accepted via UART Serial.  
Responses are sent to both UART and BTSerial regardless of where the command was sent.  
Commands are terminated with a newline (`\n`).

- `v` - Print current volume
- `v.<value>` - Set volume to specified value (Integer from 0-100)
- `v+<value>` - Increase volume by value
- `v-<value>` - Decrease volume by value

- `c` - Print current config (config.ini)
- `c.` - Reload config from file without restart
- `c.<option>` - Print specific config option
- `c.<option>.<value>` - Change specific config option value
Possible config options are listed above in the example config.ini.

Runtime configuration values are only updated after a device restart, unless `c.` is run.  
Writing an unknown configuration value adds it to the file (but the file needs to exist before writing).

Examples:
```
v
v.60
v+10

c
c.v
c.v.80
```