#include "VoiceChat.hpp"
#include "Audio.hpp"
#include "Network.hpp"
#include <miniaudio.h>
#include <iostream>
#include <cmath>
#include <random>
#include <cstring>

namespace Mercenary {

VoiceChatManager& VoiceChatManager::get() {
    static VoiceChatManager instance;
    return instance;
}

VoiceChatManager::VoiceChatManager() {
}

VoiceChatManager::~VoiceChatManager() {
    shutdown();
}

void VoiceChatManager::captureCallback(void* pDevice, void* pOutput, const void* pInput, unsigned int frameCount) {
    (void)pOutput;
    if (!pInput || frameCount == 0) return;
    ::ma_device* device = static_cast<::ma_device*>(pDevice);
    VoiceChatManager* mgr = static_cast<VoiceChatManager*>(device->pUserData);
    if (!mgr || !mgr->m_isTransmitting.load()) return;

    const float* inSamples = static_cast<const float*>(pInput);
    std::lock_guard<std::mutex> lock(mgr->m_txMutex);
    mgr->m_capturedInputSamples.insert(mgr->m_capturedInputSamples.end(), inSamples, inSamples + frameCount);
}

bool VoiceChatManager::init() {
    if (m_initialized) return true;

    ::ma_context* ctx = new ::ma_context();
    if (ma_context_init(NULL, 0, NULL, ctx) != MA_SUCCESS) {
        std::cerr << "[Voice Chat Error] Failed to initialize miniaudio context\n";
        delete ctx;
        ctx = nullptr;
    }
    m_context = ctx;

    ::ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate        = 16000;
    config.dataCallback      = (ma_device_data_proc)captureCallback;
    config.pUserData         = this;
    config.capture.pDeviceID = nullptr; // Default system capture device

    ::ma_device* dev = new ::ma_device();
    if (ma_device_init(ctx, &config, dev) == MA_SUCCESS) {
        if (ma_device_start(dev) == MA_SUCCESS) {
            m_captureDevice = dev;
            m_hardwareMicAvailable = true;
            m_activeCaptureDeviceName = "Default System Microphone";
            std::cout << "[Voice Chat] Hardware microphone initialized @ 16kHz mono capture.\n";
        } else {
            ma_device_uninit(dev);
            delete dev;
            m_hardwareMicAvailable = false;
        }
    } else {
        delete dev;
        m_hardwareMicAvailable = false;
        std::cout << "[Voice Chat] No hardware microphone detected. Using Procedural Vocal Carrier Fallback.\n";
    }

    m_initialized = true;
    return true;
}

void VoiceChatManager::shutdown() {
    if (!m_initialized) return;

    if (m_captureDevice) {
        ::ma_device* dev = static_cast<::ma_device*>(m_captureDevice);
        ma_device_stop(dev);
        ma_device_uninit(dev);
        delete dev;
        m_captureDevice = nullptr;
    }

    if (m_context) {
        ::ma_context* ctx = static_cast<::ma_context*>(m_context);
        ma_context_uninit(ctx);
        delete ctx;
        m_context = nullptr;
    }

    m_speakers.clear();
    m_initialized = false;
}

std::vector<std::string> VoiceChatManager::getCaptureDeviceNames() {
    std::vector<std::string> names;
    names.push_back("Default System Microphone");
    if (!m_context) return names;

    ::ma_context* ctx = static_cast<::ma_context*>(m_context);
    ::ma_device_info* pCaptureInfos = nullptr;
    ::ma_uint32 captureCount = 0;
    if (ma_context_get_devices(ctx, nullptr, nullptr, &pCaptureInfos, &captureCount) == MA_SUCCESS) {
        for (::ma_uint32 i = 0; i < captureCount; ++i) {
            names.push_back(pCaptureInfos[i].name);
        }
    }
    return names;
}

bool VoiceChatManager::setCaptureDevice(int deviceIndex) {
    if (!m_initialized || !m_context) return false;
    if (m_currentCaptureDeviceIndex == deviceIndex && m_captureDevice != nullptr) return true;

    ::ma_context* ctx = static_cast<::ma_context*>(m_context);
    ::ma_device_info* pCaptureInfos = nullptr;
    ::ma_uint32 captureCount = 0;
    if (ma_context_get_devices(ctx, nullptr, nullptr, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        return false;
    }

    if (deviceIndex < 0 || deviceIndex > static_cast<int>(captureCount)) {
        return false;
    }

    if (m_captureDevice) {
        ::ma_device* oldDev = static_cast<::ma_device*>(m_captureDevice);
        ma_device_stop(oldDev);
        ma_device_uninit(oldDev);
        delete oldDev;
        m_captureDevice = nullptr;
    }

    m_currentCaptureDeviceIndex = deviceIndex;
    m_activeCaptureDeviceName = (deviceIndex == 0) ? "Default System Microphone" : pCaptureInfos[deviceIndex - 1].name;

    ::ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate        = 16000;
    config.dataCallback      = (ma_device_data_proc)captureCallback;
    config.pUserData         = this;
    if (deviceIndex > 0) {
        config.capture.pDeviceID = &pCaptureInfos[deviceIndex - 1].id;
    } else {
        config.capture.pDeviceID = nullptr;
    }

    ::ma_device* dev = new ::ma_device();
    if (ma_device_init(ctx, &config, dev) == MA_SUCCESS && ma_device_start(dev) == MA_SUCCESS) {
        m_captureDevice = dev;
        m_hardwareMicAvailable = true;
        std::cout << "[Voice Chat] Switched capture recording device to: " << m_activeCaptureDeviceName << "\n";
        return true;
    } else {
        if (dev) {
            ma_device_uninit(dev);
            delete dev;
        }
        m_hardwareMicAvailable = false;
        std::cerr << "[Voice Chat Error] Failed to start chosen capture recording device\n";
        return false;
    }
}

void VoiceChatManager::playSquelchChirp(bool start) {
    if (start) {
        AudioEngine::get().playSound(SoundType::RadioBeepDatalink, 0.45f, 1.35f);
    } else {
        AudioEngine::get().playSound(SoundType::TacticalClick, 0.40f, 0.85f);
    }
}

void VoiceChatManager::setProximityPtt(bool active) {
    if (m_proximityPttActive == active) return;

    bool wasTransmitting = m_proximityPttActive || m_squadPttActive;
    m_proximityPttActive = active;
    bool nowTransmitting = m_proximityPttActive || m_squadPttActive;

    if (!wasTransmitting && nowTransmitting) {
        m_isTransmitting.store(true);
        playSquelchChirp(true);
    } else if (wasTransmitting && !nowTransmitting) {
        m_isTransmitting.store(false);
        playSquelchChirp(false);

        // Send squelch end packet
        VoiceDataPacket endPkt{};
        endPkt.speakerId = m_localPlayerId;
        endPkt.channel = 0;
        endPkt.flags = 4; // squelch end
        endPkt.speakerPos = m_localPlayerPos;
        std::strncpy(endPkt.callsign, m_localCallsign.c_str(), sizeof(endPkt.callsign) - 1);
        NetworkEngine::get().broadcastVoiceData(endPkt);
    }
}

void VoiceChatManager::setSquadPtt(bool active) {
    if (m_squadPttActive == active) return;

    bool wasTransmitting = m_proximityPttActive || m_squadPttActive;
    m_squadPttActive = active;
    bool nowTransmitting = m_proximityPttActive || m_squadPttActive;

    if (!wasTransmitting && nowTransmitting) {
        m_isTransmitting.store(true);
        playSquelchChirp(true);
    } else if (wasTransmitting && !nowTransmitting) {
        m_isTransmitting.store(false);
        playSquelchChirp(false);

        // Send squelch end packet
        VoiceDataPacket endPkt{};
        endPkt.speakerId = m_localPlayerId;
        endPkt.channel = 1; // squad
        endPkt.flags = 4; // squelch end
        endPkt.speakerPos = m_localPlayerPos;
        std::strncpy(endPkt.callsign, m_localCallsign.c_str(), sizeof(endPkt.callsign) - 1);
        NetworkEngine::get().broadcastVoiceData(endPkt);
    }
}

void VoiceChatManager::processTransmission(float dt) {
    if (!m_isTransmitting.load()) {
        float cur = m_localMicAmplitude.load();
        m_localMicAmplitude.store(std::max(0.0f, cur - dt * 4.0f));
        return;
    }

    std::vector<float> availableSamples;
    {
        std::lock_guard<std::mutex> lock(m_txMutex);
        if (!m_capturedInputSamples.empty()) {
            availableSamples = std::move(m_capturedInputSamples);
            m_capturedInputSamples.clear();
        }
    }

    // If hardware mic didn't supply frames (e.g. no hardware mic or buffer starving), synthesize procedural voice
    if (availableSamples.empty()) {
        size_t samplesNeeded = static_cast<size_t>(16000.0f * dt);
        if (samplesNeeded == 0) samplesNeeded = 256;
        availableSamples.resize(samplesNeeded);

        for (size_t i = 0; i < samplesNeeded; ++i) {
            m_simVocalPhase += 2.0f * 3.14159f * 135.0f / 16000.0f;
            if (m_simVocalPhase > 2.0f * 3.14159f) m_simVocalPhase -= 2.0f * 3.14159f;

            m_simFormantPhase1 += 2.0f * 3.14159f * 720.0f / 16000.0f;
            if (m_simFormantPhase1 > 2.0f * 3.14159f) m_simFormantPhase1 -= 2.0f * 3.14159f;

            m_simFormantPhase2 += 2.0f * 3.14159f * 1220.0f / 16000.0f;
            if (m_simFormantPhase2 > 2.0f * 3.14159f) m_simFormantPhase2 -= 2.0f * 3.14159f;

            float vocalFold = std::sin(m_simVocalPhase) * 0.5f;
            float formant1  = std::sin(m_simFormantPhase1) * 0.35f;
            float formant2  = std::sin(m_simFormantPhase2) * 0.20f;
            float voice = vocalFold + formant1 + formant2;

            // Subtle speech modulation
            float speechEnv = 0.70f + 0.30f * std::sin(m_txPacketTimer * 9.5f);
            availableSamples[i] = voice * speechEnv * 0.65f;
        }
    }

    // Compute RMS Energy for VU Meters
    float sumSq = 0.0f;
    for (float s : availableSamples) {
        sumSq += s * s;
    }
    float rms = std::sqrt(sumSq / std::max(1.0f, (float)availableSamples.size())) * m_micSensitivity;
    m_localMicAmplitude.store(std::clamp(rms, 0.0f, 1.0f));

    // Append to accumulator and emit 256-sample packets (~16ms)
    m_txAccumulator.insert(m_txAccumulator.end(), availableSamples.begin(), availableSamples.end());

    while (m_txAccumulator.size() >= 256) {
        VoiceDataPacket pkt{};
        pkt.speakerId = m_localPlayerId;
        pkt.channel = m_proximityPttActive ? 0 : 1;
        pkt.flags = 1; // active speech
        pkt.amplitude = m_localMicAmplitude.load();
        pkt.speakerPos = m_localPlayerPos;
        std::strncpy(pkt.callsign, m_localCallsign.c_str(), sizeof(pkt.callsign) - 1);
        pkt.sampleCount = 256;

        for (int i = 0; i < 256; ++i) {
            float val = std::clamp(m_txAccumulator[i] * m_micSensitivity, -1.0f, 1.0f);
            pkt.samples[i] = static_cast<int16_t>(val * 32767.0f);
        }

        // Broadcast to dedicated server
        NetworkEngine::get().broadcastVoiceData(pkt);

        // If loopback test is active, also feed local loopback buffer
        if (m_micTestLoopback) {
            std::lock_guard<std::mutex> loopLock(m_loopbackMutex);
            // Resample 256 samples from 16kHz to 48kHz (3x)
            for (int i = 0; i < 256; ++i) {
                float s0 = m_txAccumulator[i];
                float s1 = (i + 1 < 256) ? m_txAccumulator[i + 1] : s0;
                m_loopbackBuffer.push_back(s0);
                m_loopbackBuffer.push_back(s0 + (s1 - s0) * 0.333f);
                m_loopbackBuffer.push_back(s0 + (s1 - s0) * 0.666f);
            }
            if (m_loopbackBuffer.size() > 48000) {
                m_loopbackBuffer.erase(m_loopbackBuffer.begin(), m_loopbackBuffer.begin() + 24000);
            }
        }

        m_txAccumulator.erase(m_txAccumulator.begin(), m_txAccumulator.begin() + 256);
    }
}

void VoiceChatManager::onReceiveVoicePacket(const VoiceDataPacket& pkt, const glm::vec3& listenerPos, float cameraYaw) {
    (void)listenerPos;
    (void)cameraYaw;
    if (!m_voiceEnabled) return;
    if (pkt.speakerId == m_localPlayerId && m_localPlayerId > 0) return; // Don't loop back self from network

    std::lock_guard<std::mutex> lock(m_speakersMutex);
    RemoteVoiceSpeaker& spk = m_speakers[pkt.speakerId];
    spk.speakerId = pkt.speakerId;
    spk.callsign = pkt.callsign[0] ? pkt.callsign : ("OPERATIVE_" + std::to_string(pkt.speakerId));
    spk.position = pkt.speakerPos;
    spk.isSquadRadio = (pkt.channel == 1);
    spk.currentAmplitude = pkt.amplitude;
    spk.activeTimer = 0.0f;
    spk.isSpeaking = (pkt.flags & 1) != 0;

    if (pkt.flags & 2) {
        AudioEngine::get().playSound(SoundType::RadioBeepDatalink, 0.30f, 1.40f);
    }
    if (pkt.flags & 4) {
        spk.isSpeaking = false;
        spk.currentAmplitude = 0.0f;
        AudioEngine::get().playSound(SoundType::TacticalClick, 0.25f, 0.90f);
    }

    if (pkt.sampleCount > 0) {
        // Resample from 16 kHz to 48 kHz (3x linear interpolation)
        spk.sampleBuffer.reserve(spk.sampleBuffer.size() + pkt.sampleCount * 3);
        for (uint16_t i = 0; i < pkt.sampleCount; ++i) {
            float s0 = static_cast<float>(pkt.samples[i]) / 32768.0f;
            float s1 = (i + 1 < pkt.sampleCount) ? (static_cast<float>(pkt.samples[i + 1]) / 32768.0f) : s0;
            spk.sampleBuffer.push_back(s0);
            spk.sampleBuffer.push_back(s0 + (s1 - s0) * 0.333f);
            spk.sampleBuffer.push_back(s0 + (s1 - s0) * 0.666f);
        }

        // Cap buffer to max 0.5s of audio to prevent delay buildup
        if (spk.sampleBuffer.size() > 24000) {
            spk.sampleBuffer.erase(spk.sampleBuffer.begin(), spk.sampleBuffer.end() - 12000);
            spk.readIndex = 0;
        }
    }
}

void VoiceChatManager::update(float dt, const glm::vec3& listenerPos, float cameraYaw) {
    m_txPacketTimer += dt;
    processTransmission(dt);

    std::lock_guard<std::mutex> lock(m_speakersMutex);
    std::vector<uint32_t> toRemove;

    for (auto& [id, spk] : m_speakers) {
        spk.activeTimer += dt;
        if (spk.activeTimer > 0.40f) {
            spk.isSpeaking = false;
            spk.currentAmplitude = 0.0f;
        }
        if (spk.activeTimer > 4.0f && spk.sampleBuffer.empty()) {
            toRemove.push_back(id);
            continue;
        }

        // 3D Spatial Audio Calibration
        if (spk.isSquadRadio) {
            spk.distanceGain = 1.0f;
            spk.panL = 0.88f;
            spk.panR = 0.88f;
            spk.filterAlpha = 0.58f; // Radio bandpass
        } else {
            float dist = glm::distance(listenerPos, spk.position);
            if (dist > 40.0f) {
                spk.distanceGain = 0.0f;
            } else if (dist <= 4.0f) {
                spk.distanceGain = 1.0f;
            } else {
                spk.distanceGain = std::clamp(1.0f - (dist - 4.0f) / 36.0f, 0.0f, 1.0f);
            }

            // Stereo Panning relative to camera orientation
            float angle = std::atan2(spk.position.x - listenerPos.x, spk.position.z - listenerPos.z) - cameraYaw;
            float pan = std::sin(angle);
            spk.panL = std::sqrt(std::clamp((1.0f - pan) * 0.5f, 0.06f, 0.94f));
            spk.panR = std::sqrt(std::clamp((1.0f + pan) * 0.5f, 0.06f, 0.94f));

            // High frequency distance attenuation (air damping)
            spk.filterAlpha = std::clamp(1.0f - (dist / 45.0f) * 0.65f, 0.18f, 0.92f);
        }
    }

    for (uint32_t id : toRemove) {
        m_speakers.erase(id);
    }
}

void VoiceChatManager::mixBlock(float* outL, float* outR, unsigned int frameCount, float sampleRate) {
    (void)sampleRate;
    if (!m_voiceEnabled || frameCount == 0 || !outL || !outR) return;

    {
        std::unique_lock<std::mutex> lock(m_speakersMutex, std::try_to_lock);
        if (lock.owns_lock()) {
            for (auto& [id, spk] : m_speakers) {
                if (spk.distanceGain <= 0.001f) continue;
                float volScale = spk.distanceGain * m_voiceVolume;
                float panLVol = spk.panL * volScale;
                float panRVol = spk.panR * volScale;
                float alpha = spk.filterAlpha;

                for (unsigned int f = 0; f < frameCount; ++f) {
                    size_t remaining = (spk.sampleBuffer.size() > spk.readIndex) ? (spk.sampleBuffer.size() - spk.readIndex) : 0;
                    if (remaining > 0) {
                        float sample = spk.sampleBuffer[spk.readIndex++];
                        if (remaining < 64) {
                            float fade = static_cast<float>(remaining) / 64.0f;
                            sample *= fade;
                        }
                        spk.filterStateL += alpha * (sample - spk.filterStateL);
                        spk.filterStateR += alpha * (sample - spk.filterStateR);

                        outL[f] += spk.filterStateL * panLVol;
                        outR[f] += spk.filterStateR * panRVol;
                    } else {
                        spk.filterStateL *= 0.85f;
                        spk.filterStateR *= 0.85f;
                        if (std::abs(spk.filterStateL) < 1e-4f) spk.filterStateL = 0.0f;
                        if (std::abs(spk.filterStateR) < 1e-4f) spk.filterStateR = 0.0f;
                        if (!spk.sampleBuffer.empty()) {
                            spk.sampleBuffer.clear();
                            spk.readIndex = 0;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (m_micTestLoopback) {
        std::unique_lock<std::mutex> loopLock(m_loopbackMutex, std::try_to_lock);
        if (loopLock.owns_lock()) {
            for (unsigned int f = 0; f < frameCount; ++f) {
                if (m_loopbackReadIndex < m_loopbackBuffer.size()) {
                    float s = m_loopbackBuffer[m_loopbackReadIndex++] * 0.75f * m_voiceVolume;
                    outL[f] += s;
                    outR[f] += s;
                } else {
                    if (m_loopbackBuffer.size() > 0) {
                        m_loopbackBuffer.clear();
                        m_loopbackReadIndex = 0;
                    }
                    break;
                }
            }
        }
    }
}

void VoiceChatManager::mixOutput(float& outL, float& outR, float sampleRate) {
    (void)sampleRate;
    if (!m_voiceEnabled) return;

    {
        std::lock_guard<std::mutex> lock(m_speakersMutex);
        for (auto& [id, spk] : m_speakers) {
            size_t remaining = (spk.sampleBuffer.size() > spk.readIndex) ? (spk.sampleBuffer.size() - spk.readIndex) : 0;
            if (remaining > 0 && spk.distanceGain > 0.001f) {
                float sample = spk.sampleBuffer[spk.readIndex++];

                // Smooth micro-envelope de-click if approaching buffer exhaustion
                if (remaining < 64) {
                    float fade = static_cast<float>(remaining) / 64.0f;
                    sample *= fade;
                }

                // IIR Low-pass / Radio filter
                spk.filterStateL += spk.filterAlpha * (sample - spk.filterStateL);
                spk.filterStateR += spk.filterAlpha * (sample - spk.filterStateR);

                outL += spk.filterStateL * spk.panL * spk.distanceGain * m_voiceVolume;
                outR += spk.filterStateR * spk.panR * spk.distanceGain * m_voiceVolume;
            } else if (remaining == 0) {
                // Soft exponential decay for filter tail prevents audible pops / clicks
                spk.filterStateL *= 0.85f;
                spk.filterStateR *= 0.85f;
                if (std::abs(spk.filterStateL) < 1e-4f) spk.filterStateL = 0.0f;
                if (std::abs(spk.filterStateR) < 1e-4f) spk.filterStateR = 0.0f;
                if (!spk.sampleBuffer.empty()) {
                    spk.sampleBuffer.clear();
                    spk.readIndex = 0;
                }
            }
        }
    }

    // Loopback test mixing
    if (m_micTestLoopback) {
        std::lock_guard<std::mutex> loopLock(m_loopbackMutex);
        if (m_loopbackReadIndex < m_loopbackBuffer.size()) {
            float s = m_loopbackBuffer[m_loopbackReadIndex++];
            outL += s * 0.75f * m_voiceVolume;
            outR += s * 0.75f * m_voiceVolume;
        } else if (m_loopbackBuffer.size() > 0) {
            m_loopbackBuffer.clear();
            m_loopbackReadIndex = 0;
        }
    }
}

void VoiceChatManager::removeRemoteSpeaker(uint32_t speakerId) {
    std::lock_guard<std::mutex> lock(m_speakersMutex);
    m_speakers.erase(speakerId);
}

void VoiceChatManager::triggerSimulatedNpcVoice(uint32_t npcId, const std::string& callsign, const glm::vec3& pos, float duration) {
    std::lock_guard<std::mutex> lock(m_speakersMutex);
    RemoteVoiceSpeaker& spk = m_speakers[npcId];
    spk.speakerId = npcId;
    spk.callsign = callsign;
    spk.position = pos;
    spk.isSquadRadio = false;
    spk.currentAmplitude = 0.75f;
    spk.activeTimer = 0.0f;
    spk.isSpeaking = true;

    // Generate synthesized speech frames for duration
    size_t sampleCount = static_cast<size_t>(48000.0f * duration);
    spk.sampleBuffer.resize(sampleCount);

    float p0 = 0.0f, p1 = 0.0f;
    for (size_t i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / 48000.0f;
        p0 += 2.0f * 3.14159f * 125.0f / 48000.0f;
        p1 += 2.0f * 3.14159f * 850.0f / 48000.0f;

        float vocal = std::sin(p0) * 0.4f + std::sin(p1) * 0.25f;
        float cad = 0.75f + 0.25f * std::sin(t * 12.0f);
        spk.sampleBuffer[i] = vocal * cad * 0.45f;
    }
    spk.readIndex = 0;
    playSquelchChirp(true);
}

} // namespace Mercenary
