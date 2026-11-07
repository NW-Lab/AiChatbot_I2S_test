#include <M5Unified.h>
#include <M5EchoBase.h>
#include <math.h>

namespace {

constexpr uint32_t SAMPLE_RATE = 44100;
constexpr uint32_t NOTE_DURATION_MS = 300;
constexpr uint32_t NOTE_FRAMES = SAMPLE_RATE * NOTE_DURATION_MS / 1000;
constexpr uint32_t FADE_FRAMES = SAMPLE_RATE / 100;
constexpr int16_t AMPLITUDE = 8000;
constexpr size_t CHANNEL_COUNT = 2;

const float NOTE_FREQUENCIES[] = {
    261.63f, 293.66f, 329.63f, 349.23f,
    392.00f, 440.00f, 493.88f, 523.25f,
};
const char *const NOTE_NAMES[] = {"Do", "Re", "Mi", "Fa", "So", "La", "Ti", "Do"};

M5EchoBase echoBase;
int16_t toneBuffer[NOTE_FRAMES * CHANNEL_COUNT];
size_t noteIndex = 0;

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

    return echoBase.play(
        reinterpret_cast<const uint8_t *>(toneBuffer),
        sizeof(toneBuffer));
}

}  // namespace

void setup()
{
    auto config = M5.config();
    config.serial_baudrate = 115200;
    M5.begin(config);

    if (!echoBase.init(SAMPLE_RATE, 38, 39, 7, 6, 5, 8, Wire)) {
        Serial.println("Failed to initialize the Atomic Voice Base.");
        while (true) {
            delay(1000);
        }
    }

    echoBase.setSpeakerVolume(30);
    echoBase.setMute(false);
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
