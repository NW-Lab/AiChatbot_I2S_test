#include <M5Unified.h>
#include <M5EchoBase.h>
#include <driver/gpio.h>
#include <math.h>

namespace {

constexpr uint32_t SAMPLE_RATE = 44100;
constexpr uint32_t NOTE_DURATION_MS = 300;
constexpr uint32_t NOTE_FRAMES = SAMPLE_RATE * NOTE_DURATION_MS / 1000;
constexpr uint32_t FADE_FRAMES = SAMPLE_RATE / 100;
constexpr int16_t AMPLITUDE = 8000;
constexpr size_t CHANNEL_COUNT = 2;
constexpr int SPEAKER_VOLUME = 75;
constexpr int I2C_SDA_PIN = 38;
constexpr int I2C_SCL_PIN = 39;
constexpr gpio_num_t I2S_BCK_PIN = GPIO_NUM_8;
constexpr gpio_num_t I2S_WS_PIN = GPIO_NUM_6;
constexpr gpio_num_t I2S_DOUT_PIN = GPIO_NUM_5;
constexpr uint8_t CODEC_ADDRESS = 0x18;
constexpr uint8_t AMPLIFIER_IO_ADDRESS = 0x43;
constexpr uint8_t CODEC_INPUT_FORMAT_REGISTER = 0x09;
constexpr uint8_t CODEC_OUTPUT_FORMAT_REGISTER = 0x0A;
constexpr uint8_t CODEC_MUTE_REGISTER = 0x31;
constexpr uint8_t CODEC_MUTE_BITS = 0x60;
constexpr uint8_t CODEC_VOLUME_REGISTER = 0x32;
constexpr uint8_t CODEC_CHIP_ID_HIGH_REGISTER = 0xFD;
constexpr uint8_t CODEC_CHIP_ID_LOW_REGISTER = 0xFE;
constexpr uint8_t CODEC_16BIT_I2S_FORMAT = 0x0C;
constexpr uint8_t AMPLIFIER_DIRECTION_REGISTER = 0x03;
constexpr uint8_t AMPLIFIER_OUTPUT_REGISTER = 0x05;
constexpr uint8_t AMPLIFIER_DRIVE_REGISTER = 0x07;
constexpr uint8_t AMPLIFIER_ENABLE_BIT = 0x01;

static_assert(SPEAKER_VOLUME >= 0 && SPEAKER_VOLUME <= 100, "Volume must be between 0 and 100");

const float NOTE_FREQUENCIES[] = {
    261.63f, 293.66f, 329.63f, 349.23f,
    392.00f, 440.00f, 493.88f, 523.25f,
};
const char *const NOTE_NAMES[] = {"Do", "Re", "Mi", "Fa", "So", "La", "Ti", "Do"};

#if USE_NEW_I2S_API
I2SClass audioI2s;
#else
constexpr i2s_port_t AUDIO_I2S_PORT = I2S_NUM_0;
#endif
int16_t toneBuffer[NOTE_FRAMES * CHANNEL_COUNT];
size_t noteIndex = 0;

void halt(const char *message)
{
    Serial.println(message);
    while (true) {
        delay(1000);
    }
}

bool readRegister(uint8_t address, uint8_t reg, uint8_t &value)
{
    Wire.beginTransmission(address);
    const bool registerWritten = Wire.write(reg) == 1;
    const uint8_t status = Wire.endTransmission(false);
    if (!registerWritten || status != 0 || Wire.requestFrom(address, uint8_t(1)) != 1) {
        return false;
    }

    const int result = Wire.read();
    if (result < 0) {
        return false;
    }
    value = static_cast<uint8_t>(result);
    return true;
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(address);
    const bool registerWritten = Wire.write(reg) == 1;
    const bool valueWritten = Wire.write(value) == 1;
    const uint8_t status = Wire.endTransmission();
    return registerWritten && valueWritten && status == 0;
}

bool codecDetected()
{
    uint8_t idHigh = 0;
    uint8_t idLow = 0;
    return readRegister(CODEC_ADDRESS, CODEC_CHIP_ID_HIGH_REGISTER, idHigh) && idHigh == 0x83 &&
           readRegister(CODEC_ADDRESS, CODEC_CHIP_ID_LOW_REGISTER, idLow) && idLow == 0x11;
}

bool configureCodec()
{
    es8311_set_twowire(&Wire);
    es8311_handle_t codec = es8311_create(I2C_NUM_0, CODEC_ADDRESS);
    if (codec == nullptr) {
        Serial.println("Failed to allocate an ES8311 codec handle.");
        return false;
    }

    es8311_clock_config_t clockConfig{};
    clockConfig.sample_frequency = SAMPLE_RATE;
    esp_err_t result = es8311_init(codec, &clockConfig, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (result == ESP_OK) {
        result = es8311_voice_mute(codec, false);
    }
    if (result == ESP_OK) {
        result = es8311_voice_volume_set(codec, SPEAKER_VOLUME, nullptr);
    }
    es8311_delete(codec);
    if (result != ESP_OK) {
        Serial.printf("Failed to configure ES8311 (error %d).\n", static_cast<int>(result));
        return false;
    }

    const uint8_t expectedVolume =
        SPEAKER_VOLUME == 0 ? 0 : (SPEAKER_VOLUME * 256 / 100) - 1;
    uint8_t inputFormat = 0;
    uint8_t outputFormat = 0;
    uint8_t mute = 0;
    uint8_t volume = 0;
    if (!readRegister(CODEC_ADDRESS, CODEC_INPUT_FORMAT_REGISTER, inputFormat) ||
        !readRegister(CODEC_ADDRESS, CODEC_OUTPUT_FORMAT_REGISTER, outputFormat) ||
        !readRegister(CODEC_ADDRESS, CODEC_MUTE_REGISTER, mute) ||
        !readRegister(CODEC_ADDRESS, CODEC_VOLUME_REGISTER, volume) ||
        inputFormat != CODEC_16BIT_I2S_FORMAT || outputFormat != CODEC_16BIT_I2S_FORMAT ||
        (mute & CODEC_MUTE_BITS) != 0 || volume != expectedVolume) {
        Serial.println("ES8311 did not accept the audio configuration.");
        return false;
    }
    return true;
}

bool enableAmplifier()
{
    uint8_t drive = 0;
    uint8_t direction = 0;
    uint8_t output = 0;
    if (!readRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_DRIVE_REGISTER, drive) ||
        !readRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_DIRECTION_REGISTER, direction) ||
        !readRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_OUTPUT_REGISTER, output) ||
        !writeRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_DRIVE_REGISTER, drive & ~AMPLIFIER_ENABLE_BIT) ||
        !writeRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_OUTPUT_REGISTER, output | AMPLIFIER_ENABLE_BIT) ||
        !writeRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_DIRECTION_REGISTER, direction | AMPLIFIER_ENABLE_BIT)) {
        return false;
    }

    return readRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_DIRECTION_REGISTER, direction) &&
           (direction & AMPLIFIER_ENABLE_BIT) != 0 &&
           readRegister(AMPLIFIER_IO_ADDRESS, AMPLIFIER_OUTPUT_REGISTER, output) &&
           (output & AMPLIFIER_ENABLE_BIT) != 0;
}

bool startI2s()
{
#if USE_NEW_I2S_API
    audioI2s.setPins(I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    if (!audioI2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.printf("I2S initialization failed (error %d).\n", audioI2s.lastError());
        return false;
    }
#else
    i2s_config_t config{};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.tx_desc_auto_clear = true;

    i2s_pin_config_t pins{};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = I2S_BCK_PIN;
    pins.ws_io_num = I2S_WS_PIN;
    pins.data_out_num = I2S_DOUT_PIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t result = i2s_driver_install(AUDIO_I2S_PORT, &config, 0, nullptr);
    if (result == ESP_OK) {
        result = i2s_set_pin(AUDIO_I2S_PORT, &pins);
    }
    if (result == ESP_OK) {
        result = i2s_zero_dma_buffer(AUDIO_I2S_PORT);
    }
    if (result == ESP_OK) {
        result = i2s_start(AUDIO_I2S_PORT);
    }
    if (result != ESP_OK) {
        Serial.printf("I2S initialization failed (error %d).\n", static_cast<int>(result));
        return false;
    }
#endif
    return true;
}

bool signalToggles(gpio_num_t pin)
{
    if (gpio_input_enable(pin) != ESP_OK) {
        return false;
    }

    uint8_t levels = 0;
    const uint32_t start = micros();
    while (micros() - start < 1000) {
        levels |= 1U << gpio_get_level(pin);
        if (levels == 0x03) {
            return true;
        }
    }
    return false;
}

bool playNote(float frequency)
{
    const float phaseStep = 2.0f * PI * frequency / SAMPLE_RATE;

    for (uint32_t frame = 0; frame < NOTE_FRAMES; ++frame) {
        float envelope = 1.0f;
        if (frame < FADE_FRAMES) {
            envelope = static_cast<float>(frame) / FADE_FRAMES;
        } else if (frame >= NOTE_FRAMES - FADE_FRAMES) {
            envelope = static_cast<float>(NOTE_FRAMES - 1 - frame) / FADE_FRAMES;
        }

        const int16_t sample = static_cast<int16_t>(
            AMPLITUDE * envelope * sinf(phaseStep * frame));
        toneBuffer[frame * CHANNEL_COUNT] = sample;
        toneBuffer[frame * CHANNEL_COUNT + 1] = sample;
    }

    size_t bytesWritten = 0;
#if USE_NEW_I2S_API
    bytesWritten = audioI2s.write(reinterpret_cast<const uint8_t *>(toneBuffer), sizeof(toneBuffer));
    const int result = audioI2s.lastError();
#else
    const esp_err_t result = i2s_write(AUDIO_I2S_PORT, toneBuffer, sizeof(toneBuffer), &bytesWritten, portMAX_DELAY);
#endif
    if (result != ESP_OK || bytesWritten != sizeof(toneBuffer)) {
        Serial.printf("I2S write failed: %u/%u bytes (error %d).\n",
                      static_cast<unsigned>(bytesWritten), static_cast<unsigned>(sizeof(toneBuffer)),
                      static_cast<int>(result));
        return false;
    }
    Serial.printf("I2S wrote %u bytes.\n", static_cast<unsigned>(bytesWritten));
    return true;
}

}  // namespace

void setup()
{
    auto config = M5.config();
    config.serial_baudrate = 115200;
    M5.begin(config);

    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000U)) {
        halt("Failed to initialize I2C.");
    }
    if (!codecDetected()) {
        halt("ES8311 not detected. Check the Atomic Voice Base connection.");
    }
    if (!startI2s()) {
        halt("Audio I2S initialization failed.");
    }
    if (!configureCodec()) {
        halt("Audio codec initialization failed.");
    }
    if (!enableAmplifier()) {
        halt("Speaker amplifier setup failed. Check the Atomic Voice Base connection.");
    }

    Serial.printf("I2S clocks: BCK %s, WS %s.\n",
                  signalToggles(I2S_BCK_PIN) ? "active" : "not detected",
                  signalToggles(I2S_WS_PIN) ? "active" : "not detected");
    Serial.printf("Audio ready: 16-bit I2S, speaker volume %d/100.\n", SPEAKER_VOLUME);
    Serial.println("Press the AtomS3R button to play Do-Re-Mi-Fa-So-La-Ti-Do.");
}

void loop()
{
    M5.update();
    if (M5.BtnA.wasPressed()) {
        Serial.printf("Playing %s\n", NOTE_NAMES[noteIndex]);
        if (!playNote(NOTE_FREQUENCIES[noteIndex])) {
            Serial.println("I2S playback failed.");
        }
        noteIndex = (noteIndex + 1) % (sizeof(NOTE_FREQUENCIES) / sizeof(NOTE_FREQUENCIES[0]));
    }
    delay(5);
}
