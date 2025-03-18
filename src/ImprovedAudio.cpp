#include "ImprovedAudio.h"
#include "Audio.h"

// Define the global pointer.
ImprovedAudio* globalAudioInstance = nullptr;

void audio_process_extern(int16_t* buff, uint16_t len, bool *continueI2S) {
    if (globalAudioInstance != nullptr) {
        globalAudioInstance->processPCMData(buff, len);
    }
    // Optionally, handle I2S output here.
}
