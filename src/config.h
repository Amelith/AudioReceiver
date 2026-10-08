#define BT_NAME "Am"
#define CONFIG_FILE "/config.ini"
#define DEFAULT_VOLUME_PERCENT 85

#define DEFAULT_BUTTON_ENABLE true
// other button values are ignored when enable is set to false
#define BUTTON 32
#define BUTTON_DEBOUNCE_MS 100

#define I2S_WS 25
#define I2S_DATA_OUT 27
#define I2S_BCLK 26

// gets set to HIGH when the device is unlikely to be playing audio to avoid noise.
// Volume is set to 0 when muted, but this pin can be used to mute an external amplifier/DAC or similar.
// If you don't have a mute pin, set it to -1
#define MUTE 16