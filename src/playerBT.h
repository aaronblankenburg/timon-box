#ifndef PLAYERBT_CLASS
#define PLAYERBT_CLASS

#include "Arduino.h"
#include <player.cpp>
#include "SD.h"
#include "BluetoothA2DPSource.h"      // From https://github.com/pschatzmann/ESP32-A2DP.git
#include "ImprovedAudio.h"            // Our subclass of Audio with a PCM callback
#include "SoundData.h"                // Defines the Frame struct

// Define the size (in samples) for our PCM ring buffer.
#define PCM_BUFFER_SIZE 8192

class PlayerBT : public Player {
private:
    BluetoothA2DPSource a2dp_source;

    // --- Ring Buffer for PCM samples (16-bit mono) ---
    int16_t pcmBuffer[PCM_BUFFER_SIZE];
    volatile size_t pcmBufferHead = 0;
    volatile size_t pcmBufferTail = 0;

    // Push one sample into the ring buffer.
    void pushPCM(int16_t sample) {
        size_t nextHead = (pcmBufferHead + 1) % PCM_BUFFER_SIZE;
        if (nextHead != pcmBufferTail) {  // Only push if the buffer is not full.
            pcmBuffer[pcmBufferHead] = sample;
            pcmBufferHead = nextHead;
        }
        // Else: Buffer full – sample is dropped.
    }

    // Returns the number of available samples in the buffer.
    size_t availablePCM() {
        if (pcmBufferHead >= pcmBufferTail)
            return pcmBufferHead - pcmBufferTail;
        else
            return PCM_BUFFER_SIZE - pcmBufferTail + pcmBufferHead;
    }

    // Pop one sample from the ring buffer; returns false if the buffer is empty.
    bool popPCM(int16_t* sample) {
        if (pcmBufferHead == pcmBufferTail) {
            return false; // Buffer empty.
        }
        *sample = pcmBuffer[pcmBufferTail];
        pcmBufferTail = (pcmBufferTail + 1) % PCM_BUFFER_SIZE;
        return true;
    }

    // --- Static instance pointer for callback access ---
    static PlayerBT* instance;

    // This static callback is passed to the A2DP source.
    // It must match the signature: int32_t (*)(Frame *frames, int32_t len)
    static int32_t btStreamReader(Frame *frames, int32_t len) {
        if (instance) {
            return instance->readAudioData(frames, len);
        }
        return 0;
    }

    // This function is called by the Bluetooth stack to request PCM data.
    // It reads samples from the ring buffer and fills the array of Frames.
    // (It assumes each Frame is a structure with members "channel1" and "channel2".)
    int32_t readAudioData(Frame *frames, int32_t len) {
        int32_t framesNeeded = len; // number of frames requested
        int32_t framesProvided = 0;
        int16_t sample;
        while (framesNeeded > 0 && availablePCM() > 0) {
            if (popPCM(&sample)) {
                // For a mono source, duplicate the sample into both channels.
                frames[framesProvided].channel1 = sample;
                frames[framesProvided].channel2 = sample;
                framesProvided++;
                framesNeeded--;
            } else {
                break;
            }
        }
        // Return the number of frames filled.
        return framesProvided;
    }

    // This method will be called by the ImprovedAudio callback.
    // It receives decoded PCM samples and pushes them into the ring buffer.
    void onPCMData(const int16_t* samples, uint16_t sampleCount) {
        for (uint16_t i = 0; i < sampleCount; i++) {
            pushPCM(samples[i]);
        }
    }

    // Override the pure virtual playFile() method from Player.
    void playFile(String path) override {
        if (SD.exists(path)) {
            handleParams(path);
            currentlyPlayingFile = path;
            // Clear the ring buffer for the new file.
            pcmBufferHead = 0;
            pcmBufferTail = 0;
            bool connected = audio.connecttoSD(path.c_str());
            if (!connected) {
                Serial.println("Failed to open file: " + path);
                flushPlayerCache();
                loadAlbumDirIntoPlaylist(albumDir);
                startOrContinueAlbum();
            } else {
                Serial.println("Playing file via Bluetooth: " + path);
            }
        } else {
            Serial.println("File does not exist, flushing player cache: " + path);
            flushPlayerCache();
            loadAlbumDirIntoPlaylist(albumDir);
            startOrContinueAlbum();
        }
    }

public:
    // Use our ImprovedAudio class instead of the plain Audio class.
    ImprovedAudio audio;
    // Constructor – note that we initialize our ImprovedAudio instance here.
    PlayerBT() : audio(false, 3, I2S_NUM_0) {
        instance = this;
        // Set the PCM-data callback for our audio instance.
        audio.setPCMDataCallback([](const int16_t* data, uint16_t len) {
            if (PlayerBT::instance) {
                PlayerBT::instance->onPCMData(data, len);
            }
        });
    }

    // In startAudio(), we initialize the Bluetooth A2DP source.
    void startAudio() override {
        // Set the local Bluetooth name.
        a2dp_source.set_local_name("ESP32_BT_Source");
        // Start the A2DP source and provide our stream reader callback.
        a2dp_source.start("ESP32_BT_Source", btStreamReader);
        Serial.println("Bluetooth A2DP Source started");
    }

    // The main loop simply calls the audio decoder's loop method.
    void loop() override {
        audio.loop();
    }

    // Resume/pause functionality.
    void resume() override {
        if (!audio.isRunning()) audio.pauseResume();
    }

    void pause() override {
        if (audio.isRunning()) audio.pauseResume();
    }

    // Return the current playback time.
    int getAudioCurrentTime() override {
        return audio.getAudioCurrentTime();
    }

    // Play a click sound via Bluetooth.
    void playClick() override {
        this->songMaxVolume = 0;
        Serial.println("Playing click sound via Bluetooth");
        audio.stopSong();
        bool connected = audio.connecttoSD("/tap.mp3");
        if (connected) {
            unsigned long startTime = millis();
            while (millis() < startTime + 300) {
                loop();
            }
        } else {
            Serial.println("Failed to play tap sound");
        }
    }

    // Play an alert sound via Bluetooth.
    void playAlert() override {
        this->songMaxVolume = 0;
        Serial.println("Playing alert sound via Bluetooth");
        audio.stopSong();
        bool connected = audio.connecttoSD("/alert.mp3");
        if (connected) {
            unsigned long startTime = millis();
            while (millis() < startTime + 4500) {
                loop();
            }
        } else {
            Serial.println("Failed to play alert sound");
        }
    }
};

// Initialize the static instance pointer.
PlayerBT* PlayerBT::instance = nullptr;

#endif // PLAYERBT_CLASS
