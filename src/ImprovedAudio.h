#ifndef IMPROVED_AUDIO_H
#define IMPROVED_AUDIO_H

#include "Audio.h"

// Declare a global pointer that our weak function will use
extern class ImprovedAudio* globalAudioInstance;

class ImprovedAudio : public Audio {
public:
    // The constructor passes parameters to the base Audio class.
    // (Adjust the parameters as needed for your setup.)
    ImprovedAudio(bool internalDAC = false, uint8_t channelEnabled = 3, uint8_t i2sPort = I2S_NUM_0)
      : Audio(internalDAC, channelEnabled, i2sPort)
    {
        globalAudioInstance = this;
    }
    
    ~ImprovedAudio() {
        if (globalAudioInstance == this)
            globalAudioInstance = nullptr;
    }

    // Allow registration of a PCM-data callback.
    // The callback should accept a pointer to an array of int16_t samples and the number of samples.
    void setPCMDataCallback(void (*callback)(const int16_t* data, uint16_t len)) {
        _pcmCallback = callback;
    }

    // This method is called by our overridden audio_process_extern() function.
    void processPCMData(const int16_t* buff, uint16_t len) {
        if (_pcmCallback) {
            _pcmCallback(buff, len);
        }
    }
    
private:
    void (*_pcmCallback)(const int16_t* data, uint16_t len) = nullptr;
};

#endif // IMPROVED_AUDIO_H
