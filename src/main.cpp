// put millis() into IRAM (used in ISR)
#define CONFIG_ARDUINO_ISR_IRAM 1

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#include <BluetoothSerial.h>
#include <Stream.h>
#include <SPIFFS.h>

#include "config.h"

uint8_t volumePercent = DEFAULT_VOLUME_PERCENT;

bool buttonPressed = false;
bool enableButton = true;
bool isPlaying = false;

I2SStream out;
BluetoothA2DPSink a2dpSink(out);
BluetoothSerial btSerial;

void lt(void *pvParameters);

void IRAM_ATTR handleButtonPress();

void setupA2DP();
void onAudioStateChanged(esp_a2d_audio_state_t state, void*);
void onConnectionChanged(esp_a2d_connection_state_t state, void*);
void onBluetoothSerialConnection(esp_spp_cb_event_t event, esp_spp_cb_param_t* param);

void loadConfig();
void setConfig(char key, String value);
void mute(bool setVol = true);
void unmute(bool setVol = true);
void setVolume(uint8_t percent);
uint8_t volumePercentToValue(uint8_t percent);
uint8_t volumeValueToPercent(uint8_t value);

String readSerialInput(Stream& serial);
void handleCommand(String cmd);
template <typename... Args>
void serialPrintf(const char* s, Args... args);

void setup() {
    Serial.begin(115200);
    if (MUTE >= 0) pinMode(MUTE, OUTPUT);
    mute(); // mute until connected

    setupA2DP();
    a2dpSink.set_volume(0);

    if (!btSerial.begin(BT_NAME)) {
        Serial.println("Bluetooth Serial failed to start");
        while(true);
    }
    btSerial.register_callback(onBluetoothSerialConnection);

    if (!SPIFFS.begin(true)) {
        Serial.printf("An error occurred while mounting SPIFFS\n");
        return;
    }
    Serial.printf("SPIFFS mounted successfully\n");
    loadConfig();

    if (enableButton) {
        pinMode(BUTTON, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(BUTTON), handleButtonPress, FALLING);
    }

    xTaskCreatePinnedToCore(lt, "OtherTasks", 8192, NULL, 1, NULL, 0);
}

void loop() {}

void lt(void *pvParameters) {
    while (1) {
        if (Serial.available()) {
            String cmd = readSerialInput(Serial);
            handleCommand(cmd);
        }
        if (btSerial.available()) {
            String cmd = readSerialInput(btSerial);
            handleCommand(cmd);
        }
        if (buttonPressed && enableButton) {
            buttonPressed = false;
            if (isPlaying) {
                a2dpSink.pause();
                isPlaying = false;
            } else {
                a2dpSink.play();
                isPlaying = true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void IRAM_ATTR handleButtonPress() {
    static unsigned long lastPressTime = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastPressTime > BUTTON_DEBOUNCE_MS) {
        // set flag
        buttonPressed = true;
        lastPressTime = currentTime;
    }
}

void setupA2DP() {
    auto cfg = out.defaultConfig(TX_MODE);
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    cfg.channels = 2;
    cfg.pin_bck = I2S_BCLK;
    cfg.pin_ws = I2S_WS;
    cfg.pin_data = I2S_DATA_OUT;
    cfg.use_apll = true;
    cfg.buffer_count = 8;
    cfg.buffer_size = 512;
    out.begin(cfg);

    a2dpSink.set_auto_reconnect(true, 300);
    a2dpSink.set_on_connection_state_changed(onConnectionChanged);
    a2dpSink.set_on_audio_state_changed(onAudioStateChanged);

    a2dpSink.set_task_core(1);
    a2dpSink.start(BT_NAME);
    Serial.println("A2DP started");
}

void onAudioStateChanged(esp_a2d_audio_state_t state, void*) {
    switch (state) {
        case ESP_A2D_AUDIO_STATE_STARTED:
            isPlaying = true;
            unmute();
            break;

        case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
        case ESP_A2D_AUDIO_STATE_STOPPED:
            isPlaying = false;
            mute();
            break;
    }
}

void onConnectionChanged(esp_a2d_connection_state_t state, void*) {
    if (state != ESP_A2D_CONNECTION_STATE_CONNECTED) {
        isPlaying = false;
        mute();
    }
}

void onBluetoothSerialConnection(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
    if (event == ESP_SPP_SRV_OPEN_EVT) { // Triggered when a device connects
        Serial.println("BluetoothSerial: Device connected.");
        btSerial.println("Connected!");
    } else if (event == ESP_SPP_CLOSE_EVT) { // Triggered when a device disconnects
        Serial.println("BluetoothSerial: Device disconnected.");
    }
}

String readSerialInput(Stream& serial) {
    static String buffer = "";
    while (serial.available()) {
        char c = serial.read();

        if (c == '\n') {  // Enter key
            String input = buffer;
            buffer = "";
            input.trim();
            return input;
        } else if (c == '\b' || c == 127) {  // Handle backspace (127 for some terminals)
            if (buffer.length() > 0) {
                buffer.remove(buffer.length() - 1);
                serial.print("\b \b");  // Erase last character from Serial Monitor
            }
        } else {
            buffer += c;
            serial.print(c);  // Echo character
        }
    }
    return "";
}

void handleCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.startsWith("v")) {
        if (cmd.length() == 1) {
            serialPrintf("Current volume: %d%%\n", volumePercent);
            return;
        }
        char action = cmd.charAt(1);

        String volumeStr = cmd.substring(2);
        uint8_t vol = volumeStr.toInt();

        if (vol == 0 && volumeStr != "0") {
            serialPrintf("Invalid input! Please enter a valid integer.\n");
        } else {
            switch (action) {
                case '.':
                    volumePercent = vol;
                    break;
                case '+':
                    volumePercent = std::min(volumePercent + vol, 100);
                    break;
                case '-':
                    volumePercent = std::max(volumePercent - vol, 0);
                    break;
                default:
                    serialPrintf("Invalid action! Use '.', '+', or '-'.\n");
                    return;
            }
            setVolume(volumePercent);
            serialPrintf("Set volume: %d%%\n", volumePercent);
        }
    } else if (cmd.startsWith("c")) {
        if (cmd.length() == 1) {
            // Print entire config file
            File configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
            if (!configFile) {
                serialPrintf("Failed to open config file for reading\n");
                return;
            }
            serialPrintf("Current config:\n");
            while (configFile.available()) {
                String line = configFile.readStringUntil('\n');
                serialPrintf("%s\n", line.c_str());
            }
            configFile.close();
            return;
        } else if (cmd.length() == 2) {
            loadConfig();
            serialPrintf("Config reloaded from file.\n");
        }

        String configLine = cmd.substring(2);
        char key = configLine.charAt(0);
        
        if (configLine.length() == 1) {
            // read existing value
            File configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
            if (!configFile) {
                serialPrintf("Failed to open config file for reading\n");
                return;
            }
            String line;
            while (configFile.available()) {
                line = configFile.readStringUntil('\n');
                line.trim();
                if (line.length() == 0 || line.startsWith("#")) continue; // Skip empty lines and comments
                if (line.charAt(0) == key) {
                    String value = line.substring(2);
                    serialPrintf("Config %c: %s\n", key, value.c_str());
                    configFile.close();
                    return;
                }
            }

            configFile.close();
            return serialPrintf("Config %c not found\n", key);
        }

        // set new value
        String value = configLine.substring(2);
        setConfig(key, value);
        serialPrintf("Set config %c to %s. Some values may require a restart to take effect.\n", key, value.c_str());
    } else {
        serialPrintf("Unknown command: %s\n", cmd);
    }
}

template <typename... Args>
void serialPrintf(const char* s, Args... args) {
    Serial.printf(s, args...);
    btSerial.printf(s, args...);
}

void loadConfig() {
    File configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
    if (!configFile) {
        serialPrintf("Failed to open config file for reading, using default settings.\n");
        // Set default values if the config file doesn't exist
        setVolume(DEFAULT_VOLUME_PERCENT);
        enableButton = DEFAULT_BUTTON_ENABLE;
        return;
    }

    configFile.seek(0); // Ensure we're at the beginning of the file

    String line;
    while (configFile.available()) {
        line = configFile.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue; // Skip empty lines and comments
        
        char key = line.charAt(0);
        String value = line.substring(2);
        value.trim();
        
        switch (key) {
            case 'v':
                volumePercent = value.toInt();
                setVolume(volumePercent);
                serialPrintf("Loaded volume: %d%%\n", volumePercent);
                break;
            case 'm':
                // Handle mode if needed
                break;
            case 's':
                // Handle SSID if needed
                break;
            case 'p':
                // Handle password if needed
                break;

            case 'b':
                enableButton = (value == "1");
                serialPrintf("Loaded button enable: %s\n", enableButton ? "true" : "false");
                break;
            
            default:
                serialPrintf("Unknown config key: %c\n", key);
                break;
        }
    }

    configFile.close();
}

void setConfig(char key, String value) {
    File configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
    if (!configFile) {
        serialPrintf("Failed to open config file for reading\n");
        return;
    }

    String newConfig = "";
    bool keyFound = false;

    while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) {
            newConfig += line + "\n";
            continue; // Skip empty lines and comments
        }

        char currentKey = line.charAt(0);
        if (currentKey == key) {
            newConfig += String(key) + "=" + value + "\n";
            keyFound = true;
        } else {
            newConfig += line + "\n";
        }
    }

    if (!keyFound) {
        serialPrintf("Key %c not found in config, adding it.\n", key);
        newConfig += String(key) + "=" + value + "\n";
    }

    // Reopen the file in write mode to overwrite with new config
    configFile.close();
    configFile = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
    if (configFile) {
        configFile.print(newConfig);
        configFile.close();
    } else {
        serialPrintf("Failed to open config file for writing\n");
    }
}

void mute(bool setVol) {
    if (MUTE >= 0) digitalWrite(MUTE, HIGH);
    if (setVol) a2dpSink.set_volume(0);
}

void unmute(bool setVol) {
    if (MUTE >= 0) digitalWrite(MUTE, LOW);
    if (setVol) setVolume(volumePercent);
}

void setVolume(uint8_t percent) {
    volumePercent = percent;
    uint8_t value = volumePercentToValue(volumePercent);
    a2dpSink.set_volume(value);
}

uint8_t volumePercentToValue(uint8_t percent) {
    return percent * 127 / 100;
}

uint8_t volumeValueToPercent(uint8_t value) {
    return value * 100 / 127;
}