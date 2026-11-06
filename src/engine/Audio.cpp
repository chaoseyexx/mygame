#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "Audio.hpp"
#include "VoiceChat.hpp"
#include "SteamAudioBridge.hpp"
#include <cmath>
#include <random>
#include <iostream>
#include <algorithm>

namespace Mercenary {

AudioEngine& AudioEngine::get() {
    static AudioEngine instance;
    return instance;
}

AudioEngine::AudioEngine() {
    m_voices.resize(64); // 64 concurrent channels to eliminate voice clipping
}

AudioEngine::~AudioEngine() {
    shutdown();
}

static float whiteNoise() {
    static thread_local std::mt19937 gen(1337);
    static thread_local std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    return dis(gen);
}

void AudioEngine::audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, unsigned int frameCount) {
    (void)pInput;
    ma_device* device = pDevice;
    AudioEngine* engine = static_cast<AudioEngine*>(device->pUserData);
    float* out = static_cast<float*>(pOutput);

    const float sampleRate = (float)device->sampleRate;
    const float dt = 1.0f / sampleRate;

    // Lock voice mutex once per audio buffer callback rather than 48,000 times/sec
    std::unique_lock<std::mutex> lock(engine->m_voiceMutex);

    const float sfxVol = engine->m_sfxVolume.load();
    const float ambVol = engine->m_ambientVolume.load();
    const float masterVol = engine->m_masterVolume.load();
    const float rainIntensity = engine->m_rainIntensity.load();

    for (unsigned int i = 0; i < frameCount; ++i) {
        float sampleL = 0.0f;
        float sampleR = 0.0f;

        for (auto& voice : engine->m_voices) {
            if (!voice.active) continue;

            float soundSample = 0.0f;
            float soundSampleL = 0.0f;
            float soundSampleR = 0.0f;
            bool isBinaural = false;

            // Priority 1: AAA Valve Steam Audio Binaural HRTF Buffers
                if (voice.hasBinaural && !voice.binauralLeft.empty()) {
                    isBinaural = true;
                    const auto& bL = voice.binauralLeft;
                    const auto& bR = voice.binauralRight;
                    size_t sz = bL.size();
                    float pos = voice.samplePos;
                    size_t i0 = static_cast<size_t>(pos);
                    if (i0 < sz) {
                        size_t i1 = (i0 + 1 < sz) ? i0 + 1 : i0;
                        float frac = pos - static_cast<float>(i0);
                        soundSampleL = bL[i0] + frac * (bL[i1] - bL[i0]);
                        soundSampleR = bR[i0] + frac * (bR[i1] - bR[i0]);
                    }
                    voice.samplePos += voice.pitch;
                    voice.progress = voice.samplePos / sampleRate;
                    if (voice.samplePos >= static_cast<float>(sz)) {
                        voice.active = false;
                    }
                } else if (voice.sampleBuffer && !voice.sampleBuffer->empty()) {
                    const auto& buf = *voice.sampleBuffer;
                    size_t sz = buf.size();
                    float pos = voice.samplePos;
                    size_t i0 = static_cast<size_t>(pos);
                    if (i0 < sz) {
                        size_t i1 = (i0 + 1 < sz) ? i0 + 1 : i0;
                        float frac = pos - static_cast<float>(i0);
                        soundSample = buf[i0] + frac * (buf[i1] - buf[i0]);
                    }
                    voice.samplePos += voice.pitch;
                    voice.progress = voice.samplePos / sampleRate;
                    if (voice.samplePos >= static_cast<float>(sz)) {
                        voice.active = false;
                    }
                } else {
                    // Fallback procedural generation for remaining ambient & UI sounds
                    float t = voice.progress;

                    switch (voice.type) {
                    case SoundType::Footstep:
                    case SoundType::FootstepDirt: {
                        float env = std::exp(-t * 40.0f);
                        float thud = std::sin(2.0f * 3.14159f * 85.0f * t);
                        float grass = whiteNoise() * 0.08f * std::exp(-t * 55.0f);
                        soundSample = (thud * 0.90f + grass) * env * 0.30f;
                        break;
                    }
                    case SoundType::FootstepMud: {
                        float fSweep = 62.0f + 118.0f * std::exp(-t * 60.0f);
                        float pop = std::sin(2.0f * 3.14159f * fSweep * t) * std::exp(-t * 32.0f);
                        float suction = whiteNoise() * std::exp(-t * 28.0f) * 0.16f;
                        float slosh = (t > 0.04f) ? std::sin(2.0f * 3.14159f * 130.0f * (t - 0.04f)) * std::exp(-(t - 0.04f) * 45.0f) * 0.4f : 0.0f;
                        soundSample = (pop * 0.70f + suction + slosh) * 0.35f;
                        break;
                    }
                    case SoundType::FootstepWater: {
                        float splash1 = std::sin(2.0f * 3.14159f * 310.0f * t) * std::exp(-t * 45.0f);
                        float splash2 = std::sin(2.0f * 3.14159f * 460.0f * (t - 0.02f)) * std::exp(-t * 50.0f);
                        float spray = whiteNoise() * std::exp(-t * 38.0f) * 0.20f;
                        soundSample = (splash1 * 0.45f + splash2 * 0.35f + spray) * 0.36f;
                        break;
                    }
                    case SoundType::FootstepWood: {
                        float env = std::exp(-t * 48.0f);
                        float wood1 = std::sin(2.0f * 3.14159f * 215.0f * t);
                        float wood2 = std::sin(2.0f * 3.14159f * 430.0f * t) * 0.35f;
                        soundSample = (wood1 + wood2 + whiteNoise() * 0.04f) * env * 0.32f;
                        break;
                    }
                    case SoundType::FootstepConcrete: {
                        float env = std::exp(-t * 42.0f);
                        float heel = std::sin(2.0f * 3.14159f * 145.0f * t) * 0.75f;
                        float grit = whiteNoise() * 0.15f * std::exp(-t * 35.0f);
                        soundSample = (heel + grit) * env * 0.32f;
                        break;
                    }
                    case SoundType::GearRustle: {
                        float env = std::sin(3.14159f * std::clamp(t / 0.16f, 0.0f, 1.0f));
                        soundSample = whiteNoise() * env * 0.09f;
                        break;
                    }
                    case SoundType::ArtilleryWhistle: {
                        float fDesc = 280.0f + 1170.0f * std::exp(-t * 1.8f);
                        float whistle = std::sin(2.0f * 3.14159f * fDesc * t);
                        float volRamp = std::clamp(t / 1.1f, 0.05f, 1.0f);
                        soundSample = whistle * volRamp * 0.45f;
                        break;
                    }
                    case SoundType::ArtilleryExplosion: {
                        float shockwave = std::tanh(std::sin(2.0f * 3.14159f * 38.0f * t) * 4.2f) * std::exp(-t * 1.8f);
                        float blastPop = std::sin(2.0f * 3.14159f * 75.0f * t) * std::exp(-t * 4.5f) * 0.9f;
                        float rumbleTail = (std::sin(2.0f * 3.14159f * 48.0f * t) * 0.6f + whiteNoise() * 0.18f) * std::exp(-t * 0.85f);
                        soundSample = (shockwave * 0.65f + blastPop * 0.40f + rumbleTail * 0.45f) * 0.95f;
                        break;
                    }
                    case SoundType::FortifyBuild: {
                        float env = std::exp(-t * 35.0f);
                        float hammer = std::sin(2.0f * 3.14159f * 115.0f * t) + std::sin(2.0f * 3.14159f * 340.0f * t) * 0.4f;
                        float burlap = whiteNoise() * 0.12f * std::exp(-t * 22.0f);
                        soundSample = (hammer * 0.75f + burlap) * env * 0.50f;
                        break;
                    }
                    case SoundType::RadioChatter: {
                        float env = (t < 0.08f) ? (t / 0.08f) : std::exp(-(t - 0.08f) * 16.0f);
                        float pip = std::sin(2.0f * 3.14159f * 960.0f * t);
                        float hash = whiteNoise() * 0.08f * std::exp(-t * 12.0f);
                        soundSample = (pip * 0.7f + hash) * env * 0.35f;
                        break;
                    }
                    case SoundType::Bandage: {
                        float env = (t < 0.22f) ? (t / 0.05f) : std::exp(-(t - 0.22f) * 12.0f);
                        soundSample = whiteNoise() * env * 0.12f;
                        break;
                    }
                    case SoundType::TacticalClick: {
                        float env = std::exp(-t * 180.0f);
                        soundSample = std::sin(2.0f * 3.14159f * 1750.0f * t) * env * 0.25f;
                        break;
                    }
                    case SoundType::ItemEquip: {
                        float env1 = std::exp(-t * 70.0f) * std::sin(2.0f * 3.14159f * 620.0f * t);
                        float t2 = t - 0.07f;
                        float env2 = (t2 > 0.0f) ? std::exp(-t2 * 95.0f) * std::sin(2.0f * 3.14159f * 1180.0f * t2) : 0.0f;
                        soundSample = (env1 * 0.45f + env2 * 0.55f) * 0.40f;
                        break;
                    }
                    case SoundType::StashMove: {
                        float env = std::sin(3.14159f * (t / 0.16f)) * std::exp(-t * 14.0f);
                        soundSample = (whiteNoise() * 0.12f + std::sin(2.0f * 3.14159f * 260.0f * t) * 0.88f) * env * 0.15f;
                        break;
                    }
                    case SoundType::MoneyRustle: {
                        float env1 = std::exp(-t * 30.0f) * whiteNoise() * 0.10f;
                        float t2 = t - 0.12f;
                        float coin = (t2 > 0.0f) ? std::exp(-t2 * 60.0f) * std::sin(2.0f * 3.14159f * 2400.0f * t2) * 0.35f : 0.0f;
                        soundSample = (env1 + coin) * 0.38f;
                        break;
                    }
                    case SoundType::DeployHorn: {
                        float env = (t < 0.25f) ? (t / 0.25f) : std::exp(-(t - 0.25f) * 1.2f);
                        float sub = std::sin(2.0f * 3.14159f * 46.0f * t) * 0.70f + std::sin(2.0f * 3.14159f * 92.0f * t) * 0.30f;
                        soundSample = sub * env * 0.60f;
                        break;
                    }
                    case SoundType::Thunder: {
                        float env = (t < 0.4f) ? (t / 0.4f) : std::exp(-(t - 0.4f) * 0.75f);
                        float rumble = std::sin(2.0f * 3.14159f * 34.0f * t) * 0.65f + std::sin(2.0f * 3.14159f * 56.0f * t) * 0.35f;
                        soundSample = (whiteNoise() * 0.06f + rumble * 0.94f) * env * 0.55f;
                        break;
                    }
                    case SoundType::RainAmbience: {
                        float rainDrop = (whiteNoise() > 0.92f) ? (whiteNoise() - 0.92f) * 1.8f : 0.0f;
                        float windDrone = std::sin(2.0f * 3.14159f * 55.0f * t) * 0.04f + whiteNoise() * 0.06f;
                        soundSample = (rainDrop * 0.40f + windDrone) * 0.35f;
                        break;
                    }
                    case SoundType::AimDownSight: {
                        float env = std::sin(3.14159f * std::clamp(t / 0.16f, 0.0f, 1.0f));
                        float fabric = whiteNoise() * env * 0.14f;
                        float click = (t > 0.06f) ? std::sin(2.0f * 3.14159f * 920.0f * (t - 0.06f)) * std::exp(-(t - 0.06f) * 80.0f) * 0.18f : 0.0f;
                        soundSample = fabric + click;
                        break;
                    }
                    case SoundType::AimDownSightOut: {
                        float env = std::sin(3.14159f * std::clamp(t / 0.14f, 0.0f, 1.0f));
                        soundSample = whiteNoise() * env * 0.09f;
                        break;
                    }
                    case SoundType::ItemDrop: {
                        float thud = std::sin(2.0f * 3.14159f * 85.0f * t) * std::exp(-t * 28.0f) * 0.65f;
                        float metalPing = std::sin(2.0f * 3.14159f * 1250.0f * t) * std::exp(-t * 22.0f) * 0.35f;
                        float rattle = (t > 0.05f) ? std::sin(2.0f * 3.14159f * 740.0f * (t - 0.05f)) * std::exp(-(t - 0.05f) * 35.0f) * 0.30f : 0.0f;
                        float debris = whiteNoise() * std::exp(-t * 30.0f) * 0.15f;
                        soundSample = (thud + metalPing + rattle + debris) * 0.55f;
                        break;
                    }
                    case SoundType::HeavyBreathing: {
                        float breathCycle = std::sin(2.0f * 3.14159f * 1.35f * t);
                        float airFlow = (breathCycle > 0.0f ? breathCycle : 0.0f) * whiteNoise() * 0.16f;
                        soundSample = airFlow * std::exp(-t * 2.2f);
                        break;
                    }
                    case SoundType::SoldierBarkContact: {
                        float vocalCore = std::sin(2.0f * 3.14159f * 280.0f * t) * 0.6f + std::sin(2.0f * 3.14159f * 640.0f * t) * 0.4f;
                        float squelch = (t < 0.05f || t > 0.45f) ? whiteNoise() * 0.15f : 0.0f;
                        soundSample = (vocalCore * 0.65f + whiteNoise() * 0.20f) * std::exp(-t * 2.1f) + squelch;
                        break;
                    }
                    case SoundType::SoldierBarkSuppressed: {
                        float grunt = std::sin(2.0f * 3.14159f * 360.0f * t) * 0.7f + std::sin(2.0f * 3.14159f * 820.0f * t) * 0.3f;
                        soundSample = (grunt * 0.7f + whiteNoise() * 0.15f) * std::exp(-t * 2.8f);
                        break;
                    }
                    case SoundType::SoldierBarkWounded: {
                        float groan = std::sin(2.0f * 3.14159f * (240.0f - t * 45.0f) * t) * 0.8f;
                        soundSample = (groan * 0.75f + whiteNoise() * 0.12f) * std::exp(-t * 1.8f);
                        break;
                    }
                    case SoundType::RadioBeepDatalink: {
                        float f = (t < 0.045f) ? 1200.0f : 1850.0f;
                        float beep = std::sin(2.0f * 3.14159f * f * t) * 0.6f;
                        soundSample = beep * std::exp(-t * 12.0f);
                        break;
                    }
                    case SoundType::SoldierBarkReloading: {
                        float fVocal = std::sin(2.0f * 3.14159f * (320.0f - t * 40.0f) * t) * 0.6f + std::sin(2.0f * 3.14159f * 720.0f * t) * 0.4f;
                        float burst = (t < 0.22f) ? 1.0f : ((t < 0.26f) ? 0.2f : 0.85f);
                        soundSample = (fVocal * burst + whiteNoise() * 0.18f) * std::exp(-t * 2.3f);
                        break;
                    }
                    case SoundType::SoldierBarkIncoming: {
                        float fVocal = std::sin(2.0f * 3.14159f * (420.0f + std::sin(t * 30.0f) * 60.0f) * t) * 0.75f;
                        soundSample = (fVocal * 0.75f + whiteNoise() * 0.22f) * std::exp(-t * 2.0f);
                        break;
                    }
                    case SoundType::SoldierBarkFlanking: {
                        float fVocal = std::sin(2.0f * 3.14159f * (310.0f + t * 45.0f) * t) * 0.70f + std::sin(2.0f * 3.14159f * 680.0f * t) * 0.35f;
                        float squelch = (t < 0.04f || t > 0.45f) ? whiteNoise() * 0.12f : 0.0f;
                        soundSample = (fVocal * 0.7f + whiteNoise() * 0.15f) * std::exp(-t * 2.2f) + squelch;
                        break;
                    }
                    case SoundType::SoldierBarkManDown: {
                        float groan = std::sin(2.0f * 3.14159f * (380.0f - t * 85.0f) * t) * 0.80f;
                        soundSample = (groan * 0.75f + whiteNoise() * 0.22f) * std::exp(-t * 1.9f);
                        break;
                    }
                    case SoundType::SoldierBarkFragOut: {
                        float bark = std::sin(2.0f * 3.14159f * (460.0f - t * 60.0f) * t) * 0.85f;
                        soundSample = (bark * 0.80f + whiteNoise() * 0.18f) * std::exp(-t * 2.6f);
                        break;
                    }
                    case SoundType::HeavyMGShot: {
                        float boom = std::sin(2.0f * 3.14159f * 65.0f * t) * std::exp(-t * 8.0f);
                        float crack = (whiteNoise() * 0.85f + std::sin(2.0f * 3.14159f * 180.0f * t) * 0.5f) * std::exp(-t * 22.0f);
                        float mech = (t > 0.04f && t < 0.08f) ? whiteNoise() * 0.35f : 0.0f;
                        soundSample = boom * 0.85f + crack * 0.65f + mech;
                        break;
                    }
                    default:
                        break;
                    }

                    voice.progress += dt * voice.pitch;
                    if (voice.progress >= voice.duration) {
                        voice.active = false;
                    }
                }

                // Real-time single-pole IIR low-pass filter with transient preservation
                float targetSampleL = isBinaural ? soundSampleL : (soundSample * voice.panL);
                float targetSampleR = isBinaural ? soundSampleR : (soundSample * voice.panR);

                if (voice.samplePos <= voice.pitch) {
                    voice.filterStateL = targetSampleL;
                    voice.filterStateR = targetSampleR;
                } else {
                    voice.filterStateL += voice.filterAlpha * (targetSampleL - voice.filterStateL);
                    voice.filterStateR += voice.filterAlpha * (targetSampleR - voice.filterStateR);
                }

                sampleL += voice.filterStateL * (voice.volume * sfxVol);
                sampleR += voice.filterStateR * (voice.volume * sfxVol);
            }

        // Authentic low-passed Brownian/pink rain ambience (calm, immersive, soothing)
        if (rainIntensity > 0.02f) {
            float noiseL = whiteNoise();
            float noiseR = whiteNoise();
            engine->m_rainFilterL += 0.045f * (noiseL - engine->m_rainFilterL);
            engine->m_rainFilterR += 0.045f * (noiseR - engine->m_rainFilterR);

            float rainVolume = 0.0075f * rainIntensity * ambVol;
            sampleL += engine->m_rainFilterL * rainVolume;
            sampleR += engine->m_rainFilterR * rainVolume;
        } else {
            engine->m_windPhase += dt;
            float windSway = 0.7f + 0.3f * std::sin(engine->m_windPhase * 0.28f);
            float wL = whiteNoise();
            float wR = whiteNoise();
            engine->m_windFilterL += 0.012f * (wL - engine->m_windFilterL);
            engine->m_windFilterR += 0.012f * (wR - engine->m_windFilterR);
            sampleL += engine->m_windFilterL * (0.0028f * windSway * ambVol);
            sampleR += engine->m_windFilterR * (0.0028f * windSway * ambVol);
        }

        // Mix 3D proximity voice chat and squad radio streams
        VoiceChatManager::get().mixOutput(sampleL, sampleR, sampleRate);

        // Master Volume Scaling & Soft clipper to prevent harsh digital clipping
        sampleL = std::tanh(sampleL * 0.75f) * masterVol;
        sampleR = std::tanh(sampleR * 0.75f) * masterVol;

        out[i * 2 + 0] = sampleL;
        out[i * 2 + 1] = sampleR;
    }
}

// =========================================================================
// AAA PROCEDURAL PHYSICAL ACOUSTIC WEAPON SOUND GENERATOR
// Eliminates synthetic sine-wave stacking and arbitrary frequency bands.
// Pure physical modeling: Friedlander blast shockwaves, supersonic ballistic
// N-waves, multi-pole turbulent jet pink noise, non-linear air wavefolding,
// steel barrel acoustic waveguides, second-order sub-bass concussive integrators,
// mechanical bolt cycle dynamics, and early environmental terrain slap.
// =========================================================================
void AudioEngine::generateProceduralSoundBank() {
    constexpr float SR = 48000.0f;

    struct PRNG {
        uint32_t state;
        PRNG(uint32_t seed) : state(seed) {}
        float nextFloat() {
            state = state * 1664525u + 1013904223u;
            return (static_cast<float>(state & 0x00FFFFFF) / 8388607.5f) - 1.0f;
        }
    };

    struct PinkFilter {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float process(float w) {
            b0 = 0.99765f * b0 + w * 0.0555179f;
            b1 = 0.96300f * b1 + w * 0.0750759f;
            b2 = 0.57000f * b2 + w * 0.1538520f;
            return b0 + b1 + b2 + w * 0.5362f;
        }
    };

    auto saturateAir = [](float x) -> float {
        return (x + 0.12f * x * x) / (1.0f + std::abs(x) + 0.12f * x * x);
    };

    // Helper: Outdoor Stokes Atmospheric Reverb with dynamic high-frequency air damping
    auto applyOutdoorRollingReverb = [&](const std::vector<float>& src, float tailDuration, float reverbGain, float dampRate, PRNG& rng) -> std::vector<float> {
        size_t outSize = static_cast<size_t>(SR * tailDuration);
        std::vector<float> out(outSize, 0.0f);
        for (size_t i = 0; i < std::min(src.size(), outSize); ++i) {
            out[i] = src[i];
        }

        struct DelayLine {
            size_t delaySamples;
            float gain;
            float lpfCutoff;
        };

        DelayLine lines[] = {
            { static_cast<size_t>(SR * 0.0058f), -0.42f, 7500.0f }, // Ground reflection (inverted phase notch)
            { static_cast<size_t>(SR * 0.0173f),  0.34f, 6200.0f }, // Immediate trench revetment slap
            { static_cast<size_t>(SR * 0.0389f),  0.28f, 4800.0f }, // Nearby obstacle reflection
            { static_cast<size_t>(SR * 0.0731f),  0.22f, 3400.0f }, // Treeline cluster
            { static_cast<size_t>(SR * 0.1271f),  0.18f, 2200.0f }, // Distant building wall
            { static_cast<size_t>(SR * 0.2311f),  0.14f, 1400.0f }, // Valley slope
            { static_cast<size_t>(SR * 0.3989f),  0.10f,  850.0f }, // Far horizon terrain bounce
            { static_cast<size_t>(SR * 0.6517f),  0.07f,  480.0f }  // Rolling battlefield thunder tail
        };

        for (const auto& dl : lines) {
            if (dl.delaySamples >= outSize) continue;
            float lpState = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * dl.lpfCutoff / SR);
            for (size_t i = 0; i < src.size(); ++i) {
                size_t outIdx = i + dl.delaySamples;
                if (outIdx >= outSize) break;
                lpState += alpha * (src[i] - lpState);
                out[outIdx] += lpState * dl.gain * reverbGain;
            }
        }

        PinkFilter diffusePink;
        float diffuseLpf = 0.0f;
        for (size_t i = static_cast<size_t>(SR * 0.04f); i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::pow(t, 0.4f) * std::exp(-t * dampRate);
            float rawNoise = rng.nextFloat();
            float pNoise = diffusePink.process(rawNoise);
            float fc = 180.0f + 2200.0f * std::exp(-t * 2.8f);
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * fc / SR);
            diffuseLpf += alpha * (pNoise - diffuseLpf);
            out[i] += diffuseLpf * env * reverbGain * 0.28f;
        }

        return out;
    };

    // Helper: Concrete Bunker Enclosed Acoustic Simulation (Harsh flutter, intense early slap, ear pressure saturation)
    auto applyIndoorBunkerReverb = [&](const std::vector<float>& src, PRNG& rng) -> std::vector<float> {
        float tailDuration = 0.65f;
        size_t outSize = static_cast<size_t>(SR * tailDuration);
        std::vector<float> out(outSize, 0.0f);
        for (size_t i = 0; i < std::min(src.size(), outSize); ++i) {
            out[i] = src[i];
        }

        struct SlapbackTap {
            size_t delay;
            float gain;
            float lpf;
        };
        SlapbackTap taps[] = {
            { static_cast<size_t>(SR * 0.0034f), 0.72f, 11000.0f }, // Left concrete wall slap
            { static_cast<size_t>(SR * 0.0068f),-0.65f, 10500.0f }, // Right concrete wall slap
            { static_cast<size_t>(SR * 0.0112f), 0.58f,  9800.0f }, // Low reinforced ceiling reflection
            { static_cast<size_t>(SR * 0.0165f),-0.52f,  9200.0f }, // Rear blast door return
            { static_cast<size_t>(SR * 0.0241f), 0.44f,  8500.0f }, // Diagonal corner flutter
            { static_cast<size_t>(SR * 0.0352f), 0.38f,  7600.0f }, // Doorway opening secondary slap
            { static_cast<size_t>(SR * 0.0510f), 0.30f,  6500.0f }  // Extended room cavity bounce
        };

        for (const auto& tap : taps) {
            if (tap.delay >= outSize) continue;
            float lpState = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * tap.lpf / SR);
            for (size_t i = 0; i < src.size(); ++i) {
                size_t outIdx = i + tap.delay;
                if (outIdx >= outSize) break;
                lpState += alpha * (src[i] - lpState);
                out[outIdx] += lpState * tap.gain;
            }
        }

        PinkFilter diffusePink;
        float diffuseLpf = 0.0f;
        for (size_t i = static_cast<size_t>(SR * 0.018f); i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float flutterMod = 1.0f + 0.35f * std::sin(2.0f * 3.14159f * 220.0f * t) + 0.20f * std::sin(2.0f * 3.14159f * 850.0f * t);
            float env = std::pow(t, 0.25f) * std::exp(-t * 9.5f) * flutterMod;
            float rawNoise = rng.nextFloat();
            float pNoise = diffusePink.process(rawNoise);
            float fc = 750.0f + 6500.0f * std::exp(-t * 7.5f);
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * fc / SR);
            diffuseLpf += alpha * (pNoise - diffuseLpf);
            out[i] += diffuseLpf * env * 0.42f;
        }

        for (size_t i = 0; i < outSize; ++i) {
            out[i] = std::tanh(out[i] * 1.25f);
        }
        return out;
    };

    // Helper: Cavernous Industrial Interior Reverb (Warehouse / Ruined Hall, multipath reflections, long diffuse tail)
    auto applyIndoorWarehouseReverb = [&](const std::vector<float>& src, PRNG& rng) -> std::vector<float> {
        float tailDuration = 1.65f;
        size_t outSize = static_cast<size_t>(SR * tailDuration);
        std::vector<float> out(outSize, 0.0f);
        for (size_t i = 0; i < std::min(src.size(), outSize); ++i) {
            out[i] = src[i];
        }

        struct DelayLine {
            size_t delaySamples;
            float gain;
            float lpfCutoff;
        };
        DelayLine lines[] = {
            { static_cast<size_t>(SR * 0.018f),  0.55f, 6800.0f }, // Immediate girder bounce
            { static_cast<size_t>(SR * 0.038f), -0.48f, 5400.0f }, // Nearest brick pillar
            { static_cast<size_t>(SR * 0.075f),  0.42f, 4200.0f }, // High corrugated roof echo
            { static_cast<size_t>(SR * 0.125f), -0.36f, 3200.0f }, // Far loading bay wall
            { static_cast<size_t>(SR * 0.195f),  0.30f, 2400.0f }, // Opposite warehouse wing
            { static_cast<size_t>(SR * 0.280f), -0.22f, 1800.0f }, // Distant mezzanine return
            { static_cast<size_t>(SR * 0.410f),  0.16f, 1300.0f }  // Cavernous multi-bounce tail
        };

        for (const auto& dl : lines) {
            if (dl.delaySamples >= outSize) continue;
            float lpState = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * dl.lpfCutoff / SR);
            for (size_t i = 0; i < src.size(); ++i) {
                size_t outIdx = i + dl.delaySamples;
                if (outIdx >= outSize) break;
                lpState += alpha * (src[i] - lpState);
                out[outIdx] += lpState * dl.gain;
            }
        }

        PinkFilter diffusePink;
        float diffuseLpf = 0.0f;
        for (size_t i = static_cast<size_t>(SR * 0.045f); i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::pow(t, 0.45f) * std::exp(-t * 3.4f);
            float rawNoise = rng.nextFloat();
            float pNoise = diffusePink.process(rawNoise);
            float fc = 320.0f + 4200.0f * std::exp(-t * 2.2f);
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * fc / SR);
            diffuseLpf += alpha * (pNoise - diffuseLpf);
            out[i] += diffuseLpf * env * 0.35f;
        }

        for (size_t i = 0; i < outSize; ++i) {
            out[i] = std::tanh(out[i] * 0.95f);
        }
        return out;
    };

    // Helper: Earthen Trench Acoustic Slapback (Dirt parapet confinement, high-frequency soil damping)
    auto applyTrenchReverb = [&](const std::vector<float>& src, PRNG& rng) -> std::vector<float> {
        float tailDuration = 0.85f;
        size_t outSize = static_cast<size_t>(SR * tailDuration);
        std::vector<float> out(outSize, 0.0f);
        for (size_t i = 0; i < std::min(src.size(), outSize); ++i) {
            out[i] = src[i];
        }

        struct TrenchTap {
            size_t delay;
            float gain;
            float lpf;
        };
        TrenchTap taps[] = {
            { static_cast<size_t>(SR * 0.0048f),  0.68f, 3400.0f }, // Dirt parapet slap
            { static_cast<size_t>(SR * 0.0115f), -0.52f, 2800.0f }, // Opposite revetment slap
            { static_cast<size_t>(SR * 0.0210f),  0.38f, 2200.0f }, // Sandbag traverse corner
            { static_cast<size_t>(SR * 0.0390f),  0.26f, 1600.0f }, // Dugout entrance bounce
            { static_cast<size_t>(SR * 0.0720f), -0.18f, 1100.0f }  // Along trench corridor
        };

        for (const auto& tap : taps) {
            if (tap.delay >= outSize) continue;
            float lpState = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * tap.lpf / SR);
            for (size_t i = 0; i < src.size(); ++i) {
                size_t outIdx = i + tap.delay;
                if (outIdx >= outSize) break;
                lpState += alpha * (src[i] - lpState);
                out[outIdx] += lpState * tap.gain;
            }
        }

        PinkFilter diffusePink;
        float diffuseLpf = 0.0f;
        for (size_t i = static_cast<size_t>(SR * 0.025f); i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::pow(t, 0.35f) * std::exp(-t * 6.2f);
            float rawNoise = rng.nextFloat();
            float pNoise = diffusePink.process(rawNoise);
            float fc = 140.0f + 1600.0f * std::exp(-t * 4.5f);
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * fc / SR);
            diffuseLpf += alpha * (pNoise - diffuseLpf);
            out[i] += diffuseLpf * env * 0.28f;
        }

        for (size_t i = 0; i < outSize; ++i) {
            out[i] = std::tanh(out[i] * 1.05f);
        }
        return out;
    };

    // =========================================================================
    // 1. ASSAULT RIFLE (M4A1 5.56mm & AK-74M 5.45mm) - 4 Organic Variations
    // =========================================================================
    m_soundBank.rifleShotsOutdoor.resize(4);
    m_soundBank.rifleShotsBunker.resize(4);
    m_soundBank.rifleShotsWarehouse.resize(4);
    m_soundBank.rifleShotsTrench.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(1001u + static_cast<uint32_t>(v) * 337u);
        PinkFilter pinkF;
        float coreDur = 0.20f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float t_rise = 0.00004f;
        float t_pos  = 0.00035f + static_cast<float>(v) * 0.00002f;
        float mech_time = 0.014f + static_cast<float>(v) * 0.001f;

        float lpf_combust = 0.0f;
        float hpf_bite = 0.0f;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;

            // A. Supersonic Shock Front (Instantaneous Mach 2.5 N-Wave Pressure Jump)
            float shock = 0.0f;
            if (t < t_rise) {
                shock = t / t_rise;
            } else if (t < t_pos) {
                shock = 1.0f - 2.0f * (t - t_rise) / (t_pos - t_rise);
            } else if (t < 0.0035f) {
                shock = -0.45f * std::exp(-(t - t_pos) / 0.0008f);
            }

            // B. High-Frequency Mach Bite (Sharp metallic supersonic crack, not a sine beep)
            float rawN = rng.nextFloat();
            float biteAlpha = std::min(1.0f, 2.0f * 3.14159f * 4200.0f / SR);
            hpf_bite += biteAlpha * (rawN - hpf_bite);
            float hpfOut = (rawN - hpf_bite) * std::exp(-t / 0.008f) * 0.95f;

            // C. Concussive Gunpowder Detonation Blast (Visceral non-linear Bessel saturation, NO electronic sine kick!)
            float pN = pinkF.process(rawN);
            float blastAlpha = std::min(1.0f, 2.0f * 3.14159f * 125.0f / SR);
            lpf_combust += blastAlpha * (pN - lpf_combust);
            float blastWave = std::sin(2.0f * 3.14159f * 68.0f * t) * 0.65f + std::sin(2.0f * 3.14159f * 115.0f * t) * 0.35f;
            float blastEnv = std::exp(-t / 0.048f);
            float subThump = std::tanh((blastWave * 1.8f + lpf_combust * 2.4f) * blastEnv) * 1.15f;

            // D. Expanding Combustion Plume (Supersonic expanding gas wash, no comb filter ringing)
            float fc = 350.0f + 9500.0f * std::exp(-t / 0.015f);
            float cAlpha = std::min(1.0f, 2.0f * 3.14159f * fc / SR);
            hpfOut += cAlpha * (pN - hpfOut);
            float combustEnv = (1.0f - std::exp(-t / 0.0002f)) * std::exp(-t / 0.042f);
            float combust = std::tanh(pN * combustEnv * 2.2f) * 0.55f;

            // E. Mechanical Steel Bolt Carrier Action (Particulate impact transients, NOT sine tones)
            float mech = 0.0f;
            if (t >= mech_time && t <= mech_time + 0.035f) {
                float dtm = t - mech_time;
                float boltNoise = (rng.nextFloat() * 2.0f - 1.0f);
                float click1 = std::exp(-dtm * 280.0f) * boltNoise * 0.45f;
                float click2 = (dtm > 0.012f) ? std::exp(-(dtm - 0.012f) * 320.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.40f : 0.0f;
                mech = click1 + click2;
            }

            core[i] = shock * 0.75f + hpfOut * 0.65f + subThump * 1.25f + combust * 0.50f + mech * 0.40f;
        }

        // Multi-environment acoustic profile generation
        auto wetOutdoor = applyOutdoorRollingReverb(core, 1.45f, 0.75f, 2.4f, rng);
        m_soundBank.rifleShotsOutdoor[v].resize(wetOutdoor.size());
        for (size_t i = 0; i < wetOutdoor.size(); ++i) {
            m_soundBank.rifleShotsOutdoor[v][i] = std::tanh(wetOutdoor[i] * 0.92f);
        }

        m_soundBank.rifleShotsBunker[v] = applyIndoorBunkerReverb(core, rng);
        m_soundBank.rifleShotsWarehouse[v] = applyIndoorWarehouseReverb(core, rng);
        m_soundBank.rifleShotsTrench[v] = applyTrenchReverb(core, rng);
    }
    // =========================================================================
    // 2. HEAVY SNIPER / DMR (SVD Marksman 7.62x54mmR) - 4 Organic Variations
    // =========================================================================
    m_soundBank.sniperShotsOutdoor.resize(4);
    m_soundBank.sniperShotsBunker.resize(4);
    m_soundBank.sniperShotsWarehouse.resize(4);
    m_soundBank.sniperShotsTrench.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(2741u + static_cast<uint32_t>(v) * 619u);
        PinkFilter pinkF;
        float coreDur = 0.28f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float t_rise = 0.00003f;
        float t_pos  = 0.00045f + static_cast<float>(v) * 0.00003f;
        float mech_time = 0.020f;

        float lpf_combust = 0.0f;
        float hpf_bite = 0.0f;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;

            // 1. Supersonic Mach 2.8 Shock Crack
            float shock = 0.0f;
            if (t < t_rise) {
                shock = (t / t_rise) * 1.55f;
            } else if (t < t_pos) {
                shock = (1.0f - 2.0f * (t - t_rise) / (t_pos - t_rise)) * 1.55f;
            } else if (t < 0.0055f) {
                shock = -0.65f * std::exp(-(t - t_pos) / 0.0012f);
            }

            float rawN = rng.nextFloat();
            float biteAlpha = std::min(1.0f, 2.0f * 3.14159f * 3600.0f / SR);
            hpf_bite += biteAlpha * (rawN - hpf_bite);
            float hpfOut = (rawN - hpf_bite) * std::exp(-t / 0.011f) * 1.15f;

            // 2. Earth-Shaking High-Pressure 7.62mm Detonation Blast (No sine dive)
            float pN = pinkF.process(rawN);
            float blastAlpha = std::min(1.0f, 2.0f * 3.14159f * 95.0f / SR);
            lpf_combust += blastAlpha * (pN - lpf_combust);
            float blastWave = std::sin(2.0f * 3.14159f * 48.0f * t) * 0.70f + std::sin(2.0f * 3.14159f * 84.0f * t) * 0.35f;
            float blastEnv = std::exp(-t / 0.075f);
            float subThump = std::tanh((blastWave * 2.2f + lpf_combust * 3.2f) * blastEnv) * 1.45f;

            // 3. Expanding Combustion Plume
            float fc = 280.0f + 11000.0f * std::exp(-t / 0.018f);
            float cAlpha = std::min(1.0f, 2.0f * 3.14159f * fc / SR);
            hpfOut += cAlpha * (pN - hpfOut);
            float combustEnv = (1.0f - std::exp(-t / 0.0002f)) * std::exp(-t / 0.065f);
            float combust = std::tanh(pN * combustEnv * 2.6f) * 0.70f;

            // 4. Heavy Steel Bolt Recoil Action
            float mech = 0.0f;
            if (t >= mech_time && t <= mech_time + 0.045f) {
                float dtm = t - mech_time;
                float click1 = std::exp(-dtm * 220.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.55f;
                float click2 = (dtm > 0.016f) ? std::exp(-(dtm - 0.016f) * 260.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.45f : 0.0f;
                mech = click1 + click2;
            }

            core[i] = shock * 0.85f + hpfOut * 0.75f + subThump * 1.45f + combust * 0.60f + mech * 0.45f;
        }

        auto wetOutdoor = applyOutdoorRollingReverb(core, 2.80f, 0.95f, 1.8f, rng);
        m_soundBank.sniperShotsOutdoor[v].resize(wetOutdoor.size());
        for (size_t i = 0; i < wetOutdoor.size(); ++i) {
            m_soundBank.sniperShotsOutdoor[v][i] = std::tanh(wetOutdoor[i] * 0.95f);
        }

        m_soundBank.sniperShotsBunker[v] = applyIndoorBunkerReverb(core, rng);
        m_soundBank.sniperShotsWarehouse[v] = applyIndoorWarehouseReverb(core, rng);
        m_soundBank.sniperShotsTrench[v] = applyTrenchReverb(core, rng);
    }
    // =========================================================================
    // 3. TACTICAL SIDEARM (M9A3 9x19mm Parabellum) - 4 Organic Variations
    // =========================================================================
    m_soundBank.pistolShotsOutdoor.resize(4);
    m_soundBank.pistolShotsBunker.resize(4);
    m_soundBank.pistolShotsWarehouse.resize(4);
    m_soundBank.pistolShotsTrench.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(3201u + static_cast<uint32_t>(v) * 443u);
        PinkFilter pinkF;
        float coreDur = 0.14f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float t_rise = 0.00005f;
        float t_pos = 0.00028f;
        float mech_time = 0.010f;

        float lpf_combust = 0.0f;
        float hpf_bite = 0.0f;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;

            // 1. Snappy 9mm Mach Shock Crack
            float shock = 0.0f;
            if (t < t_rise) {
                shock = (t / t_rise) * 1.10f;
            } else if (t < t_pos) {
                shock = (1.0f - 2.0f * (t - t_rise) / (t_pos - t_rise)) * 1.10f;
            } else if (t < 0.0028f) {
                shock = -0.35f * std::exp(-(t - t_pos) / 0.0007f);
            }

            float rawN = rng.nextFloat();
            float biteAlpha = std::min(1.0f, 2.0f * 3.14159f * 4800.0f / SR);
            hpf_bite += biteAlpha * (rawN - hpf_bite);
            float hpfOut = (rawN - hpf_bite) * std::exp(-t / 0.006f) * 0.85f;

            // 2. Compact 9mm Detonation Thump
            float pN = pinkF.process(rawN);
            float blastAlpha = std::min(1.0f, 2.0f * 3.14159f * 160.0f / SR);
            lpf_combust += blastAlpha * (pN - lpf_combust);
            float blastWave = std::sin(2.0f * 3.14159f * 92.0f * t) * 0.75f + std::sin(2.0f * 3.14159f * 155.0f * t) * 0.35f;
            float blastEnv = std::exp(-t / 0.032f);
            float subThump = std::tanh((blastWave * 1.6f + lpf_combust * 2.0f) * blastEnv) * 0.85f;

            // 3. Combustion Plume
            float fc = 450.0f + 12000.0f * std::exp(-t / 0.012f);
            float cAlpha = std::min(1.0f, 2.0f * 3.14159f * fc / SR);
            hpfOut += cAlpha * (pN - hpfOut);
            float combustEnv = (1.0f - std::exp(-t / 0.0002f)) * std::exp(-t / 0.028f);
            float combust = std::tanh(pN * combustEnv * 1.8f) * 0.45f;

            // 4. Slide Recoil & Ejection Click
            float mech = 0.0f;
            if (t >= mech_time && t <= mech_time + 0.025f) {
                float dtm = t - mech_time;
                mech = std::exp(-dtm * 320.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.45f;
            }

            core[i] = shock * 0.70f + hpfOut * 0.60f + subThump * 0.95f + combust * 0.40f + mech * 0.40f;
        }

        auto wetOutdoor = applyOutdoorRollingReverb(core, 0.85f, 0.65f, 3.8f, rng);
        m_soundBank.pistolShotsOutdoor[v].resize(wetOutdoor.size());
        for (size_t i = 0; i < wetOutdoor.size(); ++i) {
            m_soundBank.pistolShotsOutdoor[v][i] = std::tanh(wetOutdoor[i] * 0.90f);
        }

        m_soundBank.pistolShotsBunker[v] = applyIndoorBunkerReverb(core, rng);
        m_soundBank.pistolShotsWarehouse[v] = applyIndoorWarehouseReverb(core, rng);
        m_soundBank.pistolShotsTrench[v] = applyTrenchReverb(core, rng);
    }
    // =========================================================================
    // 4. HEAVY MACHINE GUN (DShK 12.7mm / .50 BMG) - 4 Organic Variations
    // =========================================================================
    m_soundBank.heavyMGShotsOutdoor.resize(4);
    m_soundBank.heavyMGShotsBunker.resize(4);
    m_soundBank.heavyMGShotsWarehouse.resize(4);
    m_soundBank.heavyMGShotsTrench.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(4523u + static_cast<uint32_t>(v) * 521u);
        PinkFilter pinkF;
        float coreDur = 0.32f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float f_start = 155.0f + static_cast<float>(v) * 6.0f;
        float f_end = 26.0f;
        float tau_dive = 0.055f;
        float t_rise = 0.00005f;
        float t_pos = 0.00075f;
        float mech_time = 0.030f;

        size_t wg_len = static_cast<size_t>(SR * (0.0052f + static_cast<float>(v) * 0.0001f));
        std::vector<float> wg_buf(std::max<size_t>(1, wg_len), 0.0f);
        size_t wg_idx = 0;

        float lpf_combust = 0.0f;
        float hpf_bite = 0.0f;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;

            float shock = 0.0f;
            if (t < t_rise) {
                shock = (t / t_rise) * 1.65f;
            } else if (t < t_pos) {
                shock = (1.0f - (t - t_rise) / (t_pos - t_rise)) * 1.65f;
            } else if (t < 0.008f) {
                shock = -0.75f * std::exp(-(t - t_pos) / 0.0022f);
            }

            float rawN = rng.nextFloat();
            float biteAlpha = std::min(1.0f, 2.0f * 3.14159f * 2200.0f / SR);
            hpf_bite += biteAlpha * (rawN - hpf_bite);
            float hpfOut = (rawN - hpf_bite) * std::exp(-t / 0.014f) * 0.95f;

            // Massive Concussive 12.7mm Low-End (155Hz -> 26Hz)
            float phase = 2.0f * 3.14159265f * (f_end * t + (f_start - f_end) * tau_dive * (1.0f - std::exp(-t / tau_dive)));
            float subThump = std::tanh(3.2f * std::sin(phase)) * std::exp(-t / 0.20f) * 1.35f;

            float pN = pinkF.process(rawN);
            float fc = 280.0f + 14000.0f * std::exp(-t / 0.035f);
            float cAlpha = std::min(1.0f, 2.0f * 3.14159f * fc / SR);
            lpf_combust += cAlpha * (pN - lpf_combust);
            float combust = saturateAir(lpf_combust * std::exp(-t / 0.12f) * 1.8f);

            float in_barrel = (i < static_cast<size_t>(SR * 0.028f)) ? (shock * 0.9f + combust * 0.6f) : 0.0f;
            float barrelOut = wg_buf[wg_idx];
            wg_buf[wg_idx] = in_barrel - 0.72f * barrelOut * 0.94f;
            wg_idx = (wg_idx + 1) % wg_buf.size();

            float mech = 0.0f;
            if (t >= mech_time && t <= mech_time + 0.055f) {
                float dtm = t - mech_time;
                mech = (std::sin(2.0f * 3.14159f * 950.0f * dtm) * 0.7f +
                        std::sin(2.0f * 3.14159f * 1850.0f * dtm) * 0.5f) * std::exp(-dtm * 120.0f) * 0.60f;
            }

            core[i] = shock * 0.75f + hpfOut * 0.60f + subThump * 1.25f + combust * 0.65f + barrelOut * 0.40f + mech * 0.50f;
        }

        auto wetOutdoor = applyOutdoorRollingReverb(core, 2.40f, 1.15f, 1.2f, rng);
        m_soundBank.heavyMGShotsOutdoor[v].resize(wetOutdoor.size());
        for (size_t i = 0; i < wetOutdoor.size(); ++i) {
            m_soundBank.heavyMGShotsOutdoor[v][i] = std::tanh(wetOutdoor[i] * 0.95f);
        }

        m_soundBank.heavyMGShotsBunker[v] = applyIndoorBunkerReverb(core, rng);
        m_soundBank.heavyMGShotsWarehouse[v] = applyIndoorWarehouseReverb(core, rng);
        m_soundBank.heavyMGShotsTrench[v] = applyTrenchReverb(core, rng);
    }

    // =========================================================================
    // 5. BULLET IMPACTS (Flesh, Steel, Sandbag, Dirt) - 3 Variations
    // =========================================================================
    m_soundBank.impacts.resize(3);
    for (size_t v = 0; v < 3; ++v) {
        PRNG rng(5119u + static_cast<uint32_t>(v) * 293u);
        float dur = 0.28f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.impacts[v].resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float thud = std::sin(2.0f * 3.14159f * (140.0f - t * 80.0f) * t) * std::exp(-t * 28.0f) * 0.85f;
            float snap = (t < 0.0015f) ? (1.0f - t / 0.0015f) * 0.95f : 0.0f;
            float grit = rng.nextFloat() * 0.35f * std::exp(-t * 32.0f);
            m_soundBank.impacts[v][i] = std::tanh((thud + snap + grit) * 0.85f);
        }
    }

    // =========================================================================
    // 6. SPENT SHELL CASINGS (Physical Brass Clink & Surface Impact) - 4 Variations Each
    // =========================================================================
    // 6a. Concrete / Steel: Sharp impact click transient + fast-damped brass clatter
    m_soundBank.shellCasings.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(7129u + static_cast<uint32_t>(v) * 431u);
        float dur = 0.11f; // Quick crisp duration
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.shellCasings[v].resize(totalSamples, 0.0f);
        float f1 = 2650.0f + static_cast<float>(v) * 120.0f;
        float f2 = 4120.0f + static_cast<float>(v) * 160.0f;
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            // High-frequency broadband impact click tap (1.5ms impulse)
            float click = (rng.nextFloat() * 2.0f - 1.0f) * 0.35f * std::exp(-t * 1200.0f);
            // Damped high-frequency brass body ping (rapid damping on ground)
            float ring = (std::sin(2.0f * 3.14159f * f1 * t) * 0.22f +
                          std::sin(2.0f * 3.14159f * f2 * t) * 0.10f) * std::exp(-t * 95.0f);
            // Secondary bounce slap at 26ms
            float t2 = t - 0.026f;
            float slap = (t2 > 0.0f) ? (((rng.nextFloat() * 2.0f - 1.0f) * 0.16f +
                                         std::sin(2.0f * 3.14159f * (f1 * 0.92f) * t2) * 0.10f) * std::exp(-t2 * 130.0f)) : 0.0f;
            m_soundBank.shellCasings[v][i] = std::tanh((click + ring + slap) * 0.40f);
        }
    }

    // 6b. Dirt / Grass / Mud: Soft low-frequency muffled thud + damped soil friction
    m_soundBank.shellCasingsDirt.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(8451u + static_cast<uint32_t>(v) * 277u);
        float dur = 0.06f; // Soft, short organic clatter
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.shellCasingsDirt[v].resize(totalSamples, 0.0f);
        float fLow = 180.0f + static_cast<float>(v) * 25.0f;
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float thud = std::sin(2.0f * 3.14159f * fLow * t) * 0.35f * std::exp(-t * 130.0f);
            float grit = (rng.nextFloat() * 2.0f - 1.0f) * 0.15f * std::exp(-t * 180.0f);
            m_soundBank.shellCasingsDirt[v][i] = std::tanh((thud + grit) * 0.25f);
        }
    }

    // =========================================================================
    // 7. DRY FIRE & WEAPON CLICKS
    // =========================================================================
    {
        float dur = 0.08f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.dryFire.resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float click = (std::sin(2.0f * 3.14159f * 1850.0f * t) * 0.6f + std::sin(2.0f * 3.14159f * 3400.0f * t) * 0.4f) * std::exp(-t * 95.0f);
            m_soundBank.dryFire[i] = click * 0.40f;
        }
    }

    // =========================================================================
    // 8. RELOAD & MAGAZINE INSERTION
    // =========================================================================
    {
        float dur = 0.55f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.reload.resize(totalSamples, 0.0f);
        PRNG rng(7103u);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rustle = rng.nextFloat() * 0.15f * std::exp(-t * 12.0f);
            // Mag release click at 0.05s
            float magOut = (t > 0.05f && t < 0.09f) ? std::sin(2.0f * 3.14159f * 1650.0f * (t - 0.05f)) * std::exp(-(t - 0.05f) * 110.0f) * 0.5f : 0.0f;
            // Mag insert rock at 0.32s
            float magIn = (t > 0.32f && t < 0.42f) ? (std::sin(2.0f * 3.14159f * 850.0f * (t - 0.32f)) * 0.6f + std::sin(2.0f * 3.14159f * 2400.0f * (t - 0.32f)) * 0.4f) * std::exp(-(t - 0.32f) * 75.0f) * 0.7f : 0.0f;
            m_soundBank.reload[i] = std::tanh((rustle + magOut + magIn) * 0.70f);
        }
    }

    // =========================================================================
    // 9. CHAMBER ROUND (Charging Handle Rack)
    // =========================================================================
    {
        float dur = 0.35f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.chamberRound.resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            // Pull back at 0.02s
            float pull = (t > 0.02f && t < 0.09f) ? std::sin(2.0f * 3.14159f * 1950.0f * (t - 0.02f)) * std::exp(-(t - 0.02f) * 80.0f) * 0.45f : 0.0f;
            // Slam forward at 0.14s
            float slam = (t > 0.14f && t < 0.25f) ? (std::sin(2.0f * 3.14159f * 1150.0f * (t - 0.14f)) * 0.65f + std::sin(2.0f * 3.14159f * 3100.0f * (t - 0.14f)) * 0.35f) * std::exp(-(t - 0.14f) * 60.0f) * 0.75f : 0.0f;
            m_soundBank.chamberRound[i] = std::tanh((pull + slam) * 0.80f);
        }
    }

    // =========================================================================
    // 10. SUPERSONIC BULLET FLYBY (Doppler Sonic Crack)
    // =========================================================================
    {
        float dur = 0.25f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.bulletFlyby.resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            // Sharp N-wave snap at t=0.04s
            float snap = (t >= 0.038f && t <= 0.042f) ? (1.0f - (t - 0.038f) / 0.002f * 2.0f) * 0.95f : 0.0f;
            // Rapid Doppler whoosh
            float fDop = 1950.0f - std::tanh((t - 0.04f) * 45.0f) * 1450.0f;
            float whoosh = std::sin(2.0f * 3.14159f * fDop * t) * std::exp(-std::abs(t - 0.04f) * 35.0f) * 0.55f;
            m_soundBank.bulletFlyby[i] = std::tanh((snap + whoosh) * 0.85f);
        }
    }

    // =========================================================================
    // 11. BALLISTIC HELMET PING (Steel & Kevlar Ricochet)
    // =========================================================================
    {
        float dur = 0.65f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.helmetPing.resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float attack = (t < 0.001f) ? (1.0f - t / 0.001f) * 0.95f : 0.0f;
            // High-Q pure steel bell harmonics
            float ring = (std::sin(2.0f * 3.14159f * 2850.0f * t) * 0.6f +
                          std::sin(2.0f * 3.14159f * 5180.0f * t) * 0.35f +
                          std::sin(2.0f * 3.14159f * 7900.0f * t) * 0.20f) * std::exp(-t * 14.0f) * 0.85f;
            m_soundBank.helmetPing[i] = std::tanh((attack + ring) * 0.88f);
        }
    }

    // =========================================================================
    // 12. BODY ARMOR CERAMIC DEFLECT
    // =========================================================================
    {
        float dur = 0.35f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.armorDeflect.resize(totalSamples, 0.0f);
        PRNG rng(8191u);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float shatter = rng.nextFloat() * 0.65f * std::exp(-t * 40.0f);
            float thud = std::sin(2.0f * 3.14159f * (165.0f - t * 90.0f) * t) * std::exp(-t * 22.0f) * 0.75f;
            m_soundBank.armorDeflect[i] = std::tanh((shatter + thud) * 0.80f);
        }
    }

    // =========================================================================
    // 13. FLESH IMPACT (Visceral Wet Cavitation)
    // =========================================================================
    {
        float dur = 0.30f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.fleshImpact.resize(totalSamples, 0.0f);
        PRNG rng(9011u);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float pop = std::sin(2.0f * 3.14159f * (120.0f - t * 75.0f) * t) * std::exp(-t * 30.0f) * 0.90f;
            float wet = rng.nextFloat() * 0.35f * std::exp(-t * 35.0f);
            m_soundBank.fleshImpact[i] = std::tanh((pop + wet) * 0.85f);
        }
    }

    // =========================================================================
    // 14. SPRINT SLIDE & TACTICAL BOOT DRAG
    // =========================================================================
    {
        float dur = 0.45f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.sprintSlide.resize(totalSamples, 0.0f);
        PRNG rng(9323u);
        PinkFilter pf;
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::sin(3.14159f * std::clamp(t / dur, 0.0f, 1.0f));
            float gravel = pf.process(rng.nextFloat()) * env * 0.45f;
            m_soundBank.sprintSlide[i] = gravel;
        }
    }

    // =========================================================================
    // 15. TACTICAL GRENADE PIN & SPOON FLY-OFF
    // =========================================================================
    {
        float dur = 0.18f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.grenadePin.resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float ping = std::sin(2.0f * 3.14159f * 3650.0f * t) * std::exp(-t * 85.0f) * 0.65f;
            float snap = std::sin(2.0f * 3.14159f * 1450.0f * t) * std::exp(-t * 60.0f) * 0.45f;
            m_soundBank.grenadePin[i] = (ping + snap) * 0.60f;
        }
    }

    // =========================================================================
    // 16. GRENADE GROUND BOUNCE
    // =========================================================================
    {
        float dur = 0.22f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.grenadeBounce.resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float thud = std::sin(2.0f * 3.14159f * 180.0f * t) * std::exp(-t * 40.0f) * 0.65f;
            float ring = std::sin(2.0f * 3.14159f * 890.0f * t) * std::exp(-t * 55.0f) * 0.40f;
            m_soundBank.grenadeBounce[i] = std::tanh((thud + ring) * 0.75f);
        }
    }

    // =========================================================================
    // 17. HE GRENADE EXPLOSION (Concussive Detonation & Debris Fallout)
    // =========================================================================
    {
        PRNG rng(9606u);
        PinkFilter pf;
        float coreDur = 0.45f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float shock = (t < 0.003f) ? (1.0f - t / 0.003f) * 1.5f : 0.0f;
            float fDop = 85.0f - t * 45.0f;
            float subBass = std::sin(2.0f * 3.14159f * fDop * t) * std::exp(-t * 4.5f) * 1.25f;
            float rumble = pf.process(rng.nextFloat()) * std::exp(-t * 5.2f) * 0.90f;
            float debris = (t > 0.12f) ? rng.nextFloat() * 0.35f * std::exp(-(t - 0.12f) * 3.5f) : 0.0f;
            core[i] = shock * 0.70f + subBass * 0.85f + rumble * 0.65f + debris * 0.45f;
        }

        auto wet = applyOutdoorRollingReverb(core, 2.20f, 1.10f, 1.4f, rng);
        m_soundBank.grenadeExplosion.resize(wet.size());
        for (size_t i = 0; i < wet.size(); ++i) {
            m_soundBank.grenadeExplosion[i] = std::tanh(wet[i] * 0.92f);
        }
    }

    // =========================================================================
    // 18. ARTILLERY SHELL EXPLOSION (152mm/155mm Heavy Howitzer Blast)
    // =========================================================================
    {
        PRNG rng(9887u);
        PinkFilter pf;
        float coreDur = 0.65f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float shock = (t < 0.005f) ? (1.0f - t / 0.005f) * 2.0f : 0.0f;
            // Seismic 22Hz low-frequency earth-shaker
            float fDop = 75.0f - t * 38.0f;
            float seismic = std::sin(2.0f * 3.14159f * fDop * t) * std::exp(-t * 2.8f) * 1.55f;
            float roar = pf.process(rng.nextFloat()) * std::exp(-t * 3.8f) * 1.20f;
            float earthRumble = (t > 0.08f) ? std::sin(2.0f * 3.14159f * 24.0f * t) * std::exp(-t * 1.8f) * 0.95f : 0.0f;
            core[i] = shock * 0.80f + seismic * 1.10f + roar * 0.75f + earthRumble * 0.85f;
        }

        auto wet = applyOutdoorRollingReverb(core, 3.20f, 1.35f, 0.95f, rng);
        m_soundBank.artilleryExplosion.resize(wet.size());
        for (size_t i = 0; i < wet.size(); ++i) {
            m_soundBank.artilleryExplosion[i] = std::tanh(wet[i] * 0.95f);
        }
    }

    // =========================================================================
    // 19. BALLISTIC BARRIER PENETRATION CRACK (Kinetic Rupture & Exit Shock)
    // =========================================================================
    {
        float dur = 0.28f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.penetrationCrack.resize(totalSamples, 0.0f);
        PRNG rng(7119u);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float snap = (rng.nextFloat() * 2.0f - 1.0f) * std::exp(-t * 65.0f) * 0.95f;
            float freqSweep = 3400.0f - t * 4500.0f;
            float whip = std::sin(2.0f * 3.14159f * std::max(600.0f, freqSweep) * t) * std::exp(-t * 42.0f) * 0.70f;
            float resonance = std::sin(2.0f * 3.14159f * 1650.0f * t) * std::exp(-t * 22.0f) * 0.40f;
            m_soundBank.penetrationCrack[i] = std::tanh(snap + whip + resonance);
        }
    }

    // =========================================================================
    // 20. CONCRETE FOOTSTEPS (4 Organic Boot-on-Pavement Variations)
    // =========================================================================
    m_soundBank.footstepsConcrete.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.16f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsConcrete[v].resize(samples, 0.0f);
        PRNG rng(5101u + static_cast<uint32_t>(v) * 233u);
        PinkFilter pf;
        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            // A. Sharp Viscoelastic Rubber Heel Impact (Low-frequency structural shock)
            float heelImpulse = std::sin(2.0f * 3.14159f * 140.0f * t) * std::exp(-t * 65.0f) * 0.70f;
            // B. High-Frequency Pavement Friction / Micro-Grit Snap
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float gritSnap = rawN * std::exp(-t * 90.0f) * 0.45f;
            // C. Secondary Sole Slap & Weight Transfer (Delayed 14ms)
            float soleSlap = (t > 0.014f) ? std::sin(2.0f * 3.14159f * 220.0f * (t - 0.014f)) * std::exp(-(t - 0.014f) * 75.0f) * 0.40f : 0.0f;
            // D. Tread Texture Release Scuff
            float scuff = pf.process(rawN) * std::exp(-t * 40.0f) * 0.22f;

            m_soundBank.footstepsConcrete[v][i] = std::tanh((heelImpulse + gritSnap + soleSlap + scuff) * 1.15f);
        }
    }

    // =========================================================================
    // 21. DIRT / GRAVEL FOOTSTEPS (4 Organic Earth Variations)
    // =========================================================================
    m_soundBank.footstepsDirt.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.18f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsDirt[v].resize(samples, 0.0f);
        PRNG rng(6201u + static_cast<uint32_t>(v) * 317u);
        PinkFilter pf;
        // Pre-generate 8 asynchronous particulate grain fracture times
        float grainTimes[8];
        float grainPitches[8];
        for (int k = 0; k < 8; ++k) {
            grainTimes[k] = 0.006f + (static_cast<float>(k) * 0.008f) + rng.nextFloat() * 0.006f;
            grainPitches[k] = 600.0f + rng.nextFloat() * 1800.0f;
        }

        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            // A. Low-frequency Soil Deformation Thump (Damped ground displacement)
            float soilThud = std::sin(2.0f * 3.14159f * 75.0f * t) * std::exp(-t * 45.0f) * 0.75f;
            // B. Visceral Particulate Crunch (Multi-grain asynchronous fracture)
            float grainTotal = 0.0f;
            for (int k = 0; k < 8; ++k) {
                if (t >= grainTimes[k] && t < grainTimes[k] + 0.012f) {
                    float dtg = t - grainTimes[k];
                    grainTotal += std::sin(2.0f * 3.14159f * grainPitches[k] * dtg) * std::exp(-dtg * 180.0f) * 0.18f;
                }
            }
            // C. Broad-spectrum Earth Shear Noise
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float earthCrunch = pf.process(rawN) * std::exp(-t * 36.0f) * 0.55f;

            m_soundBank.footstepsDirt[v][i] = std::tanh((soilThud + grainTotal + earthCrunch) * 1.10f);
        }
    }

    // =========================================================================
    // 22. WOOD FOOTSTEPS (4 Organic Timber / Plank Variations)
    // =========================================================================
    m_soundBank.footstepsWood.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.17f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsWood[v].resize(samples, 0.0f);
        PRNG rng(7301u + static_cast<uint32_t>(v) * 401u);
        float f1 = 180.0f + static_cast<float>(v) * 10.0f;
        float f2 = 360.0f + static_cast<float>(v) * 15.0f;
        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float w1 = std::sin(2.0f * 3.14159f * f1 * t) * std::exp(-t * 50.0f) * 0.70f;
            float w2 = std::sin(2.0f * 3.14159f * f2 * t) * std::exp(-t * 65.0f) * 0.35f;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float creak = (t > 0.018f) ? rawN * std::exp(-(t - 0.018f) * 55.0f) * 0.22f : 0.0f;
            m_soundBank.footstepsWood[v][i] = std::tanh((w1 + w2 + creak) * 1.05f);
        }
    }

    // =========================================================================
    // 23. MUD FOOTSTEPS (4 Organic Viscous Slurry Variations)
    // =========================================================================
    m_soundBank.footstepsMud.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.22f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsMud[v].resize(samples, 0.0f);
        PRNG rng(8401u + static_cast<uint32_t>(v) * 509u);
        PinkFilter pf;
        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float suctionPop = std::sin(2.0f * 3.14159f * (50.0f + 65.0f * (1.0f - std::exp(-t * 35.0f))) * t) * std::exp(-t * 28.0f) * 0.70f;
            float squelch = pf.process(rng.nextFloat() * 2.0f - 1.0f) * std::exp(-t * 24.0f) * 0.50f;
            float splash = (t > 0.035f) ? std::sin(2.0f * 3.14159f * 125.0f * (t - 0.035f)) * std::exp(-(t - 0.035f) * 40.0f) * 0.30f : 0.0f;
            m_soundBank.footstepsMud[v][i] = std::tanh((suctionPop + squelch + splash) * 1.05f);
        }
    }

    // =========================================================================
    // 24. WATER FOOTSTEPS (4 Hydrodynamic Splashing Variations)
    // =========================================================================
    m_soundBank.footstepsWater.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.24f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsWater[v].resize(samples, 0.0f);
        PRNG rng(9501u + static_cast<uint32_t>(v) * 613u);
        PinkFilter pf;
        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float entry = std::sin(2.0f * 3.14159f * 240.0f * t) * std::exp(-t * 38.0f) * 0.65f;
            float spray = pf.process(rng.nextFloat() * 2.0f - 1.0f) * std::exp(-t * 22.0f) * 0.48f;
            float bubble = (t > 0.04f) ? std::sin(2.0f * 3.14159f * 480.0f * (t - 0.04f)) * std::exp(-(t - 0.04f) * 45.0f) * 0.35f : 0.0f;
            m_soundBank.footstepsWater[v][i] = std::tanh((entry + spray + bubble) * 1.05f);
        }
    }
    // =========================================================================
    // 25. TACTICAL GEAR RUSTLE (4 Cordura Webbing & Sling Clink Variations)
    // =========================================================================
    m_soundBank.gearRustles.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.15f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.gearRustles[v].resize(samples, 0.0f);
        PRNG rng(10601u + static_cast<uint32_t>(v) * 719u);
        PinkFilter pf;
        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::sin(3.14159f * std::clamp(t / 0.15f, 0.0f, 1.0f));
            float fabric = pf.process(rng.nextFloat()) * env * 0.35f;
            float buckle = (t > 0.02f && t < 0.05f) ? std::sin(2.0f * 3.14159f * 1850.0f * (t - 0.02f)) * std::exp(-(t - 0.02f) * 75.0f) * 0.25f : 0.0f;
            m_soundBank.gearRustles[v][i] = std::tanh((fabric + buckle) * 0.70f);
        }
    }

    std::cout << "[AudioEngine] Procedural physical acoustic weapon & footstep sound bank initialized ("
              << m_soundBank.rifleShotsOutdoor.size() << " rifle, "
              << m_soundBank.sniperShotsOutdoor.size() << " sniper, "
              << m_soundBank.pistolShotsOutdoor.size() << " pistol, "
              << m_soundBank.heavyMGShotsOutdoor.size() << " heavy MG profiles, "
              << "24 physical footstep & gear variations)\n";
}
bool AudioEngine::init() {
    if (m_initialized) return true;

    // Initialize the AAA procedural physical weapon sound bank
    generateProceduralSoundBank();

    ma_context* ctx = new ma_context();
    if (ma_context_init(NULL, 0, NULL, ctx) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to initialize miniaudio context\n";
        delete ctx;
        ctx = nullptr;
    }
    m_context = ctx;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = audioCallback;
    config.pUserData         = this;
    config.playback.pDeviceID = nullptr;

    ma_device* dev = new ma_device();
    if (ma_device_init(ctx, &config, dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to initialize miniaudio playback device\n";
        delete dev;
        return false;
    }

    if (ma_device_start(dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to start audio playback device\n";
        ma_device_uninit(dev);
        delete dev;
        return false;
    }

    m_device = dev;
    m_initialized = true;

    // Initialize Valve Steam Audio binaural HRTF acoustic subsystem
    SteamAudioBridge::instance().init(48000, 1024);

    return true;
}

void AudioEngine::shutdown() {
    if (m_initialized) {
        SteamAudioBridge::instance().shutdown();

        if (m_device) {
            ma_device* dev = static_cast<ma_device*>(m_device);
            ma_device_stop(dev);
            ma_device_uninit(dev);
            delete dev;
            m_device = nullptr;
        }
        if (m_context) {
            ma_context* ctx = static_cast<ma_context*>(m_context);
            ma_context_uninit(ctx);
            delete ctx;
            m_context = nullptr;
        }
        m_initialized = false;
    }
}

std::vector<std::string> AudioEngine::getPlaybackDeviceNames() {
    std::vector<std::string> names;
    names.push_back("Default System Playback Device");
    if (!m_context) return names;

    ma_context* ctx = static_cast<ma_context*>(m_context);
    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(ctx, &pPlaybackInfos, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            names.push_back(pPlaybackInfos[i].name);
        }
    }
    return names;
}

bool AudioEngine::setPlaybackDevice(int deviceIndex) {
    if (!m_initialized || !m_context) return false;
    if (m_currentPlaybackDeviceIndex == deviceIndex && m_device != nullptr) return true;

    ma_context* ctx = static_cast<ma_context*>(m_context);
    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(ctx, &pPlaybackInfos, &playbackCount, nullptr, nullptr) != MA_SUCCESS) {
        return false;
    }

    if (deviceIndex < 0 || deviceIndex > static_cast<int>(playbackCount)) {
        return false;
    }

    // Stop and uninit existing device
    if (m_device) {
        ma_device* oldDev = static_cast<ma_device*>(m_device);
        ma_device_stop(oldDev);
        ma_device_uninit(oldDev);
        delete oldDev;
        m_device = nullptr;
    }

    m_currentPlaybackDeviceIndex = deviceIndex;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = audioCallback;
    config.pUserData         = this;
    if (deviceIndex > 0) {
        config.playback.pDeviceID = &pPlaybackInfos[deviceIndex - 1].id;
    } else {
        config.playback.pDeviceID = nullptr;
    }

    ma_device* dev = new ma_device();
    if (ma_device_init(ctx, &config, dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to switch playback device to index " << deviceIndex << "\n";
        delete dev;
        return false;
    }

    if (ma_device_start(dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to start switched playback device\n";
        ma_device_uninit(dev);
        delete dev;
        return false;
    }

    m_device = dev;
    std::string devName = (deviceIndex == 0) ? "Default System Playback Device" : pPlaybackInfos[deviceIndex - 1].name;
    std::cout << "[Audio Engine] Successfully switched playback output to: " << devName << "\n";
    return true;
}

const std::vector<float>* AudioEngine::selectRifleBuffer(AcousticEnvironment env, size_t variationIdx) {
    const std::vector<std::vector<float>>* bank = &m_soundBank.rifleShotsOutdoor;
    switch (env) {
        case AcousticEnvironment::ConcreteBunker:     bank = &m_soundBank.rifleShotsBunker; break;
        case AcousticEnvironment::IndustrialInterior: bank = &m_soundBank.rifleShotsWarehouse; break;
        case AcousticEnvironment::TrenchNetwork:      bank = &m_soundBank.rifleShotsTrench; break;
        default:                                      bank = &m_soundBank.rifleShotsOutdoor; break;
    }
    if (bank->empty()) return nullptr;
    return &(*bank)[variationIdx % bank->size()];
}

const std::vector<float>* AudioEngine::selectSniperBuffer(AcousticEnvironment env, size_t variationIdx) {
    const std::vector<std::vector<float>>* bank = &m_soundBank.sniperShotsOutdoor;
    switch (env) {
        case AcousticEnvironment::ConcreteBunker:     bank = &m_soundBank.sniperShotsBunker; break;
        case AcousticEnvironment::IndustrialInterior: bank = &m_soundBank.sniperShotsWarehouse; break;
        case AcousticEnvironment::TrenchNetwork:      bank = &m_soundBank.sniperShotsTrench; break;
        default:                                      bank = &m_soundBank.sniperShotsOutdoor; break;
    }
    if (bank->empty()) return nullptr;
    return &(*bank)[variationIdx % bank->size()];
}

const std::vector<float>* AudioEngine::selectPistolBuffer(AcousticEnvironment env, size_t variationIdx) {
    const std::vector<std::vector<float>>* bank = &m_soundBank.pistolShotsOutdoor;
    switch (env) {
        case AcousticEnvironment::ConcreteBunker:     bank = &m_soundBank.pistolShotsBunker; break;
        case AcousticEnvironment::IndustrialInterior: bank = &m_soundBank.pistolShotsWarehouse; break;
        case AcousticEnvironment::TrenchNetwork:      bank = &m_soundBank.pistolShotsTrench; break;
        default:                                      bank = &m_soundBank.pistolShotsOutdoor; break;
    }
    if (bank->empty()) return nullptr;
    return &(*bank)[variationIdx % bank->size()];
}

const std::vector<float>* AudioEngine::selectHmgBuffer(AcousticEnvironment env, size_t variationIdx) {
    const std::vector<std::vector<float>>* bank = &m_soundBank.heavyMGShotsOutdoor;
    switch (env) {
        case AcousticEnvironment::ConcreteBunker:     bank = &m_soundBank.heavyMGShotsBunker; break;
        case AcousticEnvironment::IndustrialInterior: bank = &m_soundBank.heavyMGShotsWarehouse; break;
        case AcousticEnvironment::TrenchNetwork:      bank = &m_soundBank.heavyMGShotsTrench; break;
        default:                                      bank = &m_soundBank.heavyMGShotsOutdoor; break;
    }
    if (bank->empty()) return nullptr;
    return &(*bank)[variationIdx % bank->size()];
}

const std::vector<float>* AudioEngine::selectFootstepBuffer(SoundType type, size_t variationIdx) {
    auto getBuf = [variationIdx](const std::vector<std::vector<float>>& bank) -> const std::vector<float>* {
        if (bank.empty()) return nullptr;
        return &bank[variationIdx % bank.size()];
    };
    switch (type) {
        case SoundType::FootstepConcrete: return getBuf(m_soundBank.footstepsConcrete);
        case SoundType::Footstep:
        case SoundType::FootstepDirt:     return getBuf(m_soundBank.footstepsDirt);
        case SoundType::FootstepWood:     return getBuf(m_soundBank.footstepsWood);
        case SoundType::FootstepMud:      return getBuf(m_soundBank.footstepsMud);
        case SoundType::FootstepWater:    return getBuf(m_soundBank.footstepsWater);
        case SoundType::GearRustle:       return getBuf(m_soundBank.gearRustles);
        default: break;
    }
    return nullptr;
}

void AudioEngine::playSound(SoundType type, float volume, float pitch, AcousticEnvironment env) {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_voiceMutex);

    // Concurrency limiting for impacts (max 2 simultaneous impacts to eliminate repetitive audio spam)
    if (type == SoundType::Impact) {
        int activeImpacts = 0;
        for (const auto& v : m_voices) {
            if (v.active && v.type == SoundType::Impact) {
                activeImpacts++;
            }
        }
        if (activeImpacts >= 2) {
            return;
        }
    }

    ActiveVoice* targetVoice = nullptr;
    float maxProgressRatio = -1.0f;

    for (auto& voice : m_voices) {
        if (!voice.active) {
            targetVoice = &voice;
            break;
        }
        float ratio = voice.progress / (voice.duration > 0.0f ? voice.duration : 1.0f);
        if (ratio > maxProgressRatio) {
            maxProgressRatio = ratio;
            targetVoice = &voice;
        }
    }

    if (targetVoice) {
        targetVoice->type = type;
        targetVoice->sampleBuffer = nullptr;
        targetVoice->hasBinaural = false;
        targetVoice->binauralLeft.clear();
        targetVoice->binauralRight.clear();
        targetVoice->samplePos = 0.0f;
        targetVoice->progress = 0.0f;
        targetVoice->volume = volume;
        targetVoice->pitch = pitch;
        targetVoice->panL = 1.0f;
        targetVoice->panR = 1.0f;
        targetVoice->filterAlpha = 0.88f; // Crisp immediate response
        targetVoice->filterStateL = 0.0f;
        targetVoice->filterStateR = 0.0f;
        targetVoice->is3D = false;
        targetVoice->active = true;

        switch (type) {
        case SoundType::RifleShot:
            targetVoice->sampleBuffer = selectRifleBuffer(env, m_rifleVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.55f;
            break;
        case SoundType::SniperShot:
            targetVoice->sampleBuffer = selectSniperBuffer(env, m_sniperVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 1.35f;
            break;
        case SoundType::PistolShot:
            targetVoice->sampleBuffer = selectPistolBuffer(env, m_pistolVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.32f;
            break;
        case SoundType::DryFire:
            targetVoice->sampleBuffer = &m_soundBank.dryFire;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::Reload:
            targetVoice->sampleBuffer = &m_soundBank.reload;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::ChamberRound:
            targetVoice->sampleBuffer = &m_soundBank.chamberRound;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::BulletFlyby:
            targetVoice->sampleBuffer = &m_soundBank.bulletFlyby;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::HelmetPing:
            targetVoice->sampleBuffer = &m_soundBank.helmetPing;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::ArmorDeflect:
            targetVoice->sampleBuffer = &m_soundBank.armorDeflect;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::FleshImpact:
            targetVoice->sampleBuffer = &m_soundBank.fleshImpact;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::SprintSlide:
            targetVoice->sampleBuffer = &m_soundBank.sprintSlide;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::GrenadePin:
            targetVoice->sampleBuffer = &m_soundBank.grenadePin;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::GrenadeBounce:
            targetVoice->sampleBuffer = &m_soundBank.grenadeBounce;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::GrenadeExplosion:
            targetVoice->sampleBuffer = &m_soundBank.grenadeExplosion;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::PenetrationCrack:
            targetVoice->sampleBuffer = &m_soundBank.penetrationCrack;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::Impact:
            if (!m_soundBank.impacts.empty()) {
                size_t idx = (m_impactVariationCounter++) % m_soundBank.impacts.size();
                targetVoice->sampleBuffer = &m_soundBank.impacts[idx];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.25f;
            }
            break;
        case SoundType::ShellCasing:
            if (!m_soundBank.shellCasings.empty()) {
                size_t idx = (m_casingVariationCounter++) % m_soundBank.shellCasings.size();
                targetVoice->sampleBuffer = &m_soundBank.shellCasings[idx];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.16f;
            }
            break;
        case SoundType::ShellCasingDirt:
            if (!m_soundBank.shellCasingsDirt.empty()) {
                size_t idx = (m_casingVariationCounter++) % m_soundBank.shellCasingsDirt.size();
                targetVoice->sampleBuffer = &m_soundBank.shellCasingsDirt[idx];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.08f;
            }
            break;
        case SoundType::Footstep:
        case SoundType::FootstepDirt:
        case SoundType::FootstepMud:
        case SoundType::FootstepWater:
        case SoundType::FootstepWood:
        case SoundType::FootstepConcrete:
        case SoundType::GearRustle:
            targetVoice->sampleBuffer = selectFootstepBuffer(type, m_footstepVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.16f;
            break;
        case SoundType::Bandage:            targetVoice->duration = 0.50f; break;
        case SoundType::TacticalClick:      targetVoice->duration = 0.06f; break;
        case SoundType::ItemEquip:          targetVoice->duration = 0.28f; break;
        case SoundType::StashMove:          targetVoice->duration = 0.18f; break;
        case SoundType::MoneyRustle:        targetVoice->duration = 0.35f; break;
        case SoundType::DeployHorn:         targetVoice->duration = 1.60f; break;
        case SoundType::Thunder:            targetVoice->duration = 3.50f; break;
        case SoundType::RainAmbience:       targetVoice->duration = 2.00f; break;
        case SoundType::ArtilleryWhistle:   targetVoice->duration = 1.35f; break;
        case SoundType::FortifyBuild:       targetVoice->duration = 0.28f; break;
        case SoundType::RadioChatter:       targetVoice->duration = 0.35f; break;
        case SoundType::AimDownSight:       targetVoice->duration = 0.18f; break;
        case SoundType::AimDownSightOut:    targetVoice->duration = 0.16f; break;
        case SoundType::ItemDrop:           targetVoice->duration = 0.35f; break;
        case SoundType::HeavyBreathing:     targetVoice->duration = 1.20f; break;
        case SoundType::SoldierBarkContact: targetVoice->duration = 0.55f; break;
        case SoundType::SoldierBarkSuppressed: targetVoice->duration = 0.45f; break;
        case SoundType::SoldierBarkWounded: targetVoice->duration = 0.65f; break;
        case SoundType::SoldierBarkReloading: targetVoice->duration = 0.55f; break;
        case SoundType::SoldierBarkIncoming: targetVoice->duration = 0.60f; break;
        case SoundType::SoldierBarkFlanking: targetVoice->duration = 0.55f; break;
        case SoundType::SoldierBarkManDown:  targetVoice->duration = 0.65f; break;
        case SoundType::SoldierBarkFragOut:  targetVoice->duration = 0.50f; break;
        case SoundType::RadioBeepDatalink:  targetVoice->duration = 0.10f; break;
        case SoundType::HeavyMGShot: {
            targetVoice->sampleBuffer = selectHmgBuffer(env, m_hmgVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 2.40f;
            break;
        }
        case SoundType::ArtilleryExplosion: {
            if (!m_soundBank.artilleryExplosion.empty()) {
                targetVoice->sampleBuffer = &m_soundBank.artilleryExplosion;
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 3.20f;
            }
            break;
        }
        }
    }
}

void AudioEngine::playSound3D(SoundType type, const glm::vec3& soundPos, const glm::vec3& listenerPos, float cameraYawRad, float volume, float maxDist, AcousticEnvironment env, float occlusion) {
    if (!m_initialized) return;

    glm::vec3 activeListener = listenerPos;
    float activeYaw = cameraYawRad;
    if (activeListener == glm::vec3(0.0f) && m_lastListenerPos != glm::vec3(0.0f)) {
        activeListener = m_lastListenerPos;
        activeYaw = m_lastCameraYawRad;
    } else if (listenerPos != glm::vec3(0.0f)) {
        m_lastListenerPos = listenerPos;
        m_lastCameraYawRad = cameraYawRad;
    }

    float dist = glm::distance(soundPos, activeListener);
    if (dist > maxDist) return; // Inaudible beyond max distance

    // Natural logarithmic/inverse-square atmospheric falloff
    float atten = 1.0f / (1.0f + 0.12f * dist + 0.0035f * dist * dist);
    // Smooth quadratic ease-out to zero at maxDist (eliminates sudden boundary cutoffs and pops)
    float distNorm = std::clamp(dist / maxDist, 0.0f, 1.0f);
    float distFade = (1.0f - distNorm) * (1.0f - distNorm);
    float finalVol = volume * atten * distFade;

    // Environmental sound transmission loss (muffled transmission through walls/barriers)
    if (occlusion > 0.0f) {
        finalVol *= std::clamp(1.0f - occlusion * 0.65f, 0.20f, 1.0f);
    }
    if (finalVol < 0.010f) return; // Drop imperceptible sounds

    // Atmospheric + environmental occlusion low-pass filtering:
    // High frequencies dissipate rapidly with distance and are blocked by solid obstacles
    float filterAlpha = std::clamp(1.0f / (1.0f + 0.065f * dist + 0.0020f * dist * dist), 0.035f, 0.88f);
    if (occlusion > 0.0f) {
        filterAlpha = std::min(filterAlpha, std::max(0.040f, 0.88f * (1.0f - occlusion)));
    }

    // Directional stereo panning relative to camera / monitor screen orientation (corrected right vector)
    float dx = soundPos.x - activeListener.x;
    float dz = soundPos.z - activeListener.z;
    float horizDist = std::sqrt(dx * dx + dz * dz);
    float pan = 0.0f;
    float fwdDot = 1.0f;
    if (horizDist > 0.001f) {
        glm::vec3 camRight(std::cos(activeYaw), 0.0f, -std::sin(activeYaw));
        glm::vec3 camForward(std::sin(activeYaw), 0.0f, std::cos(activeYaw));
        pan = std::clamp((dx * camRight.x + dz * camRight.z) / horizDist, -1.0f, 1.0f);
        fwdDot = std::clamp((dx * camForward.x + dz * camForward.z) / horizDist, -1.0f, 1.0f);
    }

    // Pinna acoustic shadow: sounds from behind lose high frequencies
    if (fwdDot < 0.0f) {
        float rearFactor = -fwdDot;
        filterAlpha *= (1.0f - rearFactor * 0.40f);
    }

    float panL = std::sqrt(std::clamp((1.0f - pan) * 0.5f, 0.08f, 0.92f));
    float panR = std::sqrt(std::clamp((1.0f + pan) * 0.5f, 0.08f, 0.92f));

    std::lock_guard<std::mutex> lock(m_voiceMutex);

    // Concurrency limiting for gunfire (max 4 simultaneous distant shots to eliminate gunfire spam)
    if (type == SoundType::RifleShot || type == SoundType::PistolShot || type == SoundType::SniperShot || type == SoundType::HeavyMGShot) {
        int activeShots = 0;
        for (const auto& v : m_voices) {
            if (v.active && (v.type == SoundType::RifleShot || v.type == SoundType::PistolShot || v.type == SoundType::SniperShot || v.type == SoundType::HeavyMGShot)) {
                activeShots++;
            }
        }
        if (activeShots >= 4 && dist > 18.0f) {
            return; // Drop distant bot gunfire when battlefield is already noisy
        }
    }

    // Concurrency limiting for bullet impacts (max 2 simultaneous impacts, drop distant spam)
    if (type == SoundType::Impact) {
        int activeImpacts = 0;
        for (const auto& v : m_voices) {
            if (v.active && v.type == SoundType::Impact) {
                activeImpacts++;
            }
        }
        if (activeImpacts >= 2) {
            return; // Drop excess impacts to preserve a clean, atmospheric soundscape
        }
    }

    ActiveVoice* targetVoice = nullptr;
    float maxProgressRatio = -1.0f;

    for (auto& voice : m_voices) {
        if (!voice.active) {
            targetVoice = &voice;
            break;
        }
        float ratio = voice.progress / (voice.duration > 0.0f ? voice.duration : 1.0f);
        if (ratio > maxProgressRatio) {
            maxProgressRatio = ratio;
            targetVoice = &voice;
        }
    }

    if (targetVoice) {
        targetVoice->type = type;
        targetVoice->sampleBuffer = nullptr;
        targetVoice->hasBinaural = false;
        targetVoice->binauralLeft.clear();
        targetVoice->binauralRight.clear();
        targetVoice->samplePos = 0.0f;
        targetVoice->progress = 0.0f;
        targetVoice->volume = finalVol;
        targetVoice->pitch = (dist > 35.0f) ? 0.88f : 1.0f; // Distant gunshots have deeper pitch
        targetVoice->panL = panL;
        targetVoice->panR = panR;
        targetVoice->filterAlpha = filterAlpha;
        targetVoice->filterStateL = 0.0f;
        targetVoice->filterStateR = 0.0f;
        targetVoice->is3D = true;
        targetVoice->active = true;

        switch (type) {
        case SoundType::RifleShot:
            targetVoice->sampleBuffer = selectRifleBuffer(env, m_rifleVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.45f;
            break;
        case SoundType::PistolShot:
            targetVoice->sampleBuffer = selectPistolBuffer(env, m_pistolVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.30f;
            break;
        case SoundType::SniperShot:
            targetVoice->sampleBuffer = selectSniperBuffer(env, m_sniperVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.85f;
            break;
        case SoundType::Impact:
            if (!m_soundBank.impacts.empty()) {
                size_t idx = (m_impactVariationCounter++) % m_soundBank.impacts.size();
                targetVoice->sampleBuffer = &m_soundBank.impacts[idx];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.25f;
            }
            break;
        case SoundType::ShellCasing:
            if (!m_soundBank.shellCasings.empty()) {
                size_t idx = (m_casingVariationCounter++) % m_soundBank.shellCasings.size();
                targetVoice->sampleBuffer = &m_soundBank.shellCasings[idx];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.16f;
            }
            break;
        case SoundType::ShellCasingDirt:
            if (!m_soundBank.shellCasingsDirt.empty()) {
                size_t idx = (m_casingVariationCounter++) % m_soundBank.shellCasingsDirt.size();
                targetVoice->sampleBuffer = &m_soundBank.shellCasingsDirt[idx];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.08f;
            }
            break;
        case SoundType::BulletFlyby:
            targetVoice->sampleBuffer = &m_soundBank.bulletFlyby;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::HelmetPing:
            targetVoice->sampleBuffer = &m_soundBank.helmetPing;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::ArmorDeflect:
            targetVoice->sampleBuffer = &m_soundBank.armorDeflect;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::FleshImpact:
            targetVoice->sampleBuffer = &m_soundBank.fleshImpact;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::SprintSlide:
            targetVoice->sampleBuffer = &m_soundBank.sprintSlide;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::GrenadePin:
            targetVoice->sampleBuffer = &m_soundBank.grenadePin;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::GrenadeBounce:
            targetVoice->sampleBuffer = &m_soundBank.grenadeBounce;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::GrenadeExplosion:
            targetVoice->sampleBuffer = &m_soundBank.grenadeExplosion;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::PenetrationCrack:
            targetVoice->sampleBuffer = &m_soundBank.penetrationCrack;
            targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            break;
        case SoundType::Footstep:
        case SoundType::FootstepDirt:
        case SoundType::FootstepMud:
        case SoundType::FootstepWater:
        case SoundType::FootstepWood:
        case SoundType::FootstepConcrete:
        case SoundType::GearRustle:
            targetVoice->sampleBuffer = selectFootstepBuffer(type, m_footstepVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.16f;
            break;
        case SoundType::Thunder:            targetVoice->duration = 3.50f; break;
        case SoundType::ArtilleryWhistle:   targetVoice->duration = 1.35f; break;
        case SoundType::FortifyBuild:       targetVoice->duration = 0.28f; break;
        case SoundType::ItemDrop:           targetVoice->duration = 0.35f; break;
        case SoundType::SoldierBarkContact: targetVoice->duration = 0.55f; break;
        case SoundType::SoldierBarkSuppressed: targetVoice->duration = 0.45f; break;
        case SoundType::SoldierBarkWounded: targetVoice->duration = 0.65f; break;
        case SoundType::SoldierBarkReloading: targetVoice->duration = 0.55f; break;
        case SoundType::SoldierBarkIncoming: targetVoice->duration = 0.60f; break;
        case SoundType::SoldierBarkFlanking: targetVoice->duration = 0.55f; break;
        case SoundType::SoldierBarkManDown:  targetVoice->duration = 0.65f; break;
        case SoundType::SoldierBarkFragOut:  targetVoice->duration = 0.50f; break;
        case SoundType::RadioBeepDatalink:  targetVoice->duration = 0.10f; break;
        case SoundType::HeavyMGShot: {
            targetVoice->sampleBuffer = selectHmgBuffer(env, m_hmgVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 2.40f;
            break;
        }
        case SoundType::ArtilleryExplosion: {
            if (!m_soundBank.artilleryExplosion.empty()) {
                targetVoice->sampleBuffer = &m_soundBank.artilleryExplosion;
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 3.20f;
            }
            break;
        }
        default:                            targetVoice->duration = 0.40f; break;
        }

        // Real-time equal-power 3D trigonometric spatialization with rear pinna shadow & distance IIR filtering
        targetVoice->hasBinaural = false;
    }
}

AudioEngine::VoiceTelemetrySnapshot AudioEngine::getVoiceSnapshot() {
    VoiceTelemetrySnapshot snapshot;
    snapshot.rainIntensity = m_rainIntensity;
    snapshot.maxVoices = static_cast<int>(m_voices.size());

    std::lock_guard<std::mutex> lock(m_voiceMutex);
    int active = 0;
    for (size_t i = 0; i < m_voices.size(); ++i) {
        const auto& v = m_voices[i];
        if (v.active) {
            active++;
            VoiceTelemetrySnapshot::VoiceDetail vd;
            vd.slot = static_cast<int>(i);
            vd.name = getSoundTypeName(v.type);
            vd.progress = v.progress;
            vd.duration = v.duration;
            vd.volume = v.volume;
            vd.pitch = v.pitch;
            vd.panL = v.panL;
            vd.panR = v.panR;
            vd.filterAlpha = v.filterAlpha;
            vd.is3D = v.is3D;
            snapshot.voices.push_back(vd);
        }
    }
    snapshot.activeCount = active;
    return snapshot;
}

} // namespace Mercenary
