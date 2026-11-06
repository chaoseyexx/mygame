#pragma once
#include "Types.hpp"
#include "NetworkTypes.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <algorithm>

namespace Mercenary {

struct RemoteVoiceSpeaker {
    uint32_t speakerId = 0;
    std::string callsign;
    glm::vec3 position{0.0f};
    bool isSquadRadio = false;
    float currentAmplitude = 0.0f;
    float activeTimer = 0.0f; // Seconds since last received audio packet
    std::vector<float> sampleBuffer;
    size_t readIndex = 0;
    float panL = 1.0f;
    float panR = 1.0f;
    float distanceGain = 1.0f;
    float filterAlpha = 0.85f;
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;
    bool isSpeaking = false;
};

class VoiceChatManager {
public:
    static VoiceChatManager& get();

    bool init();
    void shutdown();
    void update(float dt, const glm::vec3& listenerPos, float cameraYaw);

    // Push-to-Talk Controls
    void setProximityPtt(bool active);
    void setSquadPtt(bool active);
    bool isProximityPttActive() const { return m_proximityPttActive; }
    bool isSquadPttActive() const { return m_squadPttActive; }
    bool isTransmitting() const { return m_isTransmitting.load(); }

    // Telemetry & UI
    float getLocalMicAmplitude() const { return m_localMicAmplitude.load(); }
    bool isHardwareMicAvailable() const { return m_hardwareMicAvailable; }

    // Settings
    bool isVoiceChatEnabled() const { return m_voiceEnabled; }
    void setVoiceChatEnabled(bool enabled) { m_voiceEnabled = enabled; }
    float getVoiceVolume() const { return m_voiceVolume; }
    void setVoiceVolume(float vol) { m_voiceVolume = std::clamp(vol, 0.0f, 2.0f); }
    float getMicSensitivity() const { return m_micSensitivity; }
    void setMicSensitivity(float sens) { m_micSensitivity = std::clamp(sens, 0.1f, 3.0f); }
    bool isMicTestLoopback() const { return m_micTestLoopback; }
    void setMicTestLoopback(bool test) { m_micTestLoopback = test; }

    // Hardware Microphone Input Device Enumeration & Dynamic Switching
    std::vector<std::string> getCaptureDeviceNames();
    int getCurrentCaptureDeviceIndex() const { return m_currentCaptureDeviceIndex; }
    bool setCaptureDevice(int deviceIndex);
    const std::string& getActiveCaptureDeviceName() const { return m_activeCaptureDeviceName; }

    // Network Packet I/O
    void onReceiveVoicePacket(const VoiceDataPacket& pkt, const glm::vec3& listenerPos, float cameraYaw);
    const std::unordered_map<uint32_t, RemoteVoiceSpeaker>& getActiveSpeakers() const { return m_speakers; }
    void removeRemoteSpeaker(uint32_t speakerId);

    // Audio mixing callback called by AudioEngine::audioCallback
    void mixBlock(float* outL, float* outR, unsigned int frameCount, float sampleRate);
    void mixOutput(float& outL, float& outR, float sampleRate);

    // Simulated Voice Trigger for Bots / Garrison Comms
    void triggerSimulatedNpcVoice(uint32_t npcId, const std::string& callsign, const glm::vec3& pos, float duration = 2.0f);

    // Context getters/setters for local player identity
    void setLocalPlayerContext(uint32_t playerId, const std::string& callsign, const glm::vec3& pos, uint8_t faction) {
        m_localPlayerId = playerId;
        m_localCallsign = callsign;
        m_localPlayerPos = pos;
        m_localFaction = faction;
    }

private:
    VoiceChatManager();
    ~VoiceChatManager();

    bool m_initialized = false;
    bool m_voiceEnabled = true;
    float m_voiceVolume = 0.95f;
    float m_micSensitivity = 1.25f;
    bool m_micTestLoopback = false;
    bool m_hardwareMicAvailable = false;

    // Local identity
    uint32_t m_localPlayerId = 0;
    std::string m_localCallsign = "OPERATIVE";
    glm::vec3 m_localPlayerPos{0.0f};
    uint8_t m_localFaction = 0;

    // PTT State
    bool m_proximityPttActive = false;
    bool m_squadPttActive = false;
    std::atomic<bool> m_isTransmitting{false};
    std::atomic<float> m_localMicAmplitude{0.0f};

    // Miniaudio capture context & hardware device
    void* m_context = nullptr; // ma_context*
    void* m_captureDevice = nullptr; // ma_device*
    int m_currentCaptureDeviceIndex = 0;
    std::string m_activeCaptureDeviceName = "Default System Microphone";
    static void captureCallback(void* pDevice, void* pOutput, const void* pInput, unsigned int frameCount);

    // Transmission accumulator
    std::mutex m_txMutex;
    std::vector<float> m_capturedInputSamples;
    std::vector<float> m_txAccumulator;
    float m_txPacketTimer = 0.0f;
    float m_simVocalPhase = 0.0f;
    float m_simFormantPhase1 = 0.0f;
    float m_simFormantPhase2 = 0.0f;

    // Active remote speakers
    std::mutex m_speakersMutex;
    std::unordered_map<uint32_t, RemoteVoiceSpeaker> m_speakers;

    // Loopback test buffer
    std::mutex m_loopbackMutex;
    std::vector<float> m_loopbackBuffer;
    size_t m_loopbackReadIndex = 0;

    void processTransmission(float dt);
    void playSquelchChirp(bool start);
};

} // namespace Mercenary
