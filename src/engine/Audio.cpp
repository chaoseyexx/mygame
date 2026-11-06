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
                        float pulse = (t <= 0.0f) ? 0.0f : (t / 0.012f) * std::exp(1.0f - t / 0.012f) * 0.85f;
                        float grit = whiteNoise() * std::exp(-t * 45.0f) * 0.45f;
                        soundSample = std::tanh(pulse + grit) * 0.35f;
                        break;
                    }
                    case SoundType::FootstepMud: {
                        float suction = (t <= 0.0f) ? 0.0f : (t / 0.018f) * std::exp(1.0f - t / 0.018f) * 0.75f;
                        float squelch = whiteNoise() * std::exp(-t * 30.0f) * 0.40f;
                        soundSample = std::tanh(suction + squelch) * 0.35f;
                        break;
                    }
                    case SoundType::FootstepWater: {
                        float pulse = (t <= 0.0f) ? 0.0f : (t / 0.014f) * std::exp(1.0f - t / 0.014f) * 0.65f;
                        float splash = whiteNoise() * std::exp(-t * 32.0f) * 0.55f;
                        soundSample = std::tanh(pulse + splash) * 0.35f;
                        break;
                    }
                    case SoundType::FootstepWood: {
                        float tap = (t <= 0.0f) ? 0.0f : (t / 0.009f) * std::exp(1.0f - t / 0.009f) * 0.80f;
                        float creak = whiteNoise() * std::exp(-t * 48.0f) * 0.35f;
                        soundSample = std::tanh(tap + creak) * 0.35f;
                        break;
                    }
                    case SoundType::FootstepConcrete: {
                        float heel = (t <= 0.0f) ? 0.0f : (t / 0.009f) * std::exp(1.0f - t / 0.009f) * 0.85f;
                        float grit = whiteNoise() * std::exp(-t * 80.0f) * 0.50f;
                        soundSample = std::tanh(heel + grit) * 0.35f;
                        break;
                    }
                    case SoundType::GearRustle: {
                        float u = std::clamp(t / 0.16f, 0.0f, 1.0f);
                        float env = 4.0f * u * (1.0f - u);
                        soundSample = whiteNoise() * env * 0.09f;
                        break;
                    }
                    case SoundType::ArtilleryWhistle: {
                        float env = std::clamp(t / 1.1f, 0.05f, 1.0f) * std::exp(-t * 0.8f);
                        soundSample = whiteNoise() * env * 0.25f;
                        break;
                    }
                    case SoundType::ArtilleryExplosion: {
                        float boom = (t <= 0.0f) ? 0.0f : (t / 0.012f) * std::exp(1.0f - t / 0.012f) * 1.5f;
                        float roar = whiteNoise() * std::exp(-t * 4.5f) * 0.75f;
                        soundSample = std::tanh(boom + roar) * 0.75f;
                        break;
                    }
                    case SoundType::FortifyBuild: {
                        float strike = (t <= 0.0f) ? 0.0f : (t / 0.007f) * std::exp(1.0f - t / 0.007f) * 0.85f;
                        float burlap = whiteNoise() * 0.25f * std::exp(-t * 22.0f);
                        soundSample = std::tanh(strike + burlap) * 0.50f;
                        break;
                    }
                    case SoundType::RadioChatter: {
                        float env = (t < 0.08f) ? (t / 0.08f) : std::exp(-(t - 0.08f) * 16.0f);
                        soundSample = whiteNoise() * env * 0.20f;
                        break;
                    }
                    case SoundType::Bandage: {
                        float env = (t < 0.22f) ? (t / 0.05f) : std::exp(-(t - 0.22f) * 12.0f);
                        soundSample = whiteNoise() * env * 0.12f;
                        break;
                    }
                    case SoundType::TacticalClick: {
                        float click = (t < 0.0015f ? 0.75f : 0.0f) + whiteNoise() * std::exp(-t * 180.0f) * 0.35f;
                        soundSample = click * 0.25f;
                        break;
                    }
                    case SoundType::ItemEquip: {
                        float rustle = whiteNoise() * std::exp(-t * 60.0f) * 0.35f;
                        float t2 = t - 0.07f;
                        float slap = (t2 > 0.0f) ? whiteNoise() * std::exp(-t2 * 90.0f) * 0.40f : 0.0f;
                        soundSample = (rustle + slap) * 0.40f;
                        break;
                    }
                    case SoundType::StashMove: {
                        float u = std::clamp(t / 0.16f, 0.0f, 1.0f);
                        float env = 4.0f * u * (1.0f - u) * std::exp(-t * 14.0f);
                        soundSample = whiteNoise() * env * 0.30f;
                        break;
                    }
                    case SoundType::MoneyRustle: {
                        float env1 = std::exp(-t * 30.0f) * whiteNoise() * 0.20f;
                        float t2 = t - 0.12f;
                        float coin = (t2 > 0.0f) ? std::exp(-t2 * 60.0f) * whiteNoise() * 0.25f : 0.0f;
                        soundSample = (env1 + coin) * 0.38f;
                        break;
                    }
                    case SoundType::DeployHorn: {
                        float env = (t < 0.25f) ? (t / 0.25f) : std::exp(-(t - 0.25f) * 1.2f);
                        float blast = ((t <= 0.0f) ? 0.0f : (t / 0.025f) * std::exp(1.0f - t / 0.025f) * 0.85f) + whiteNoise() * 0.25f;
                        soundSample = std::tanh(blast * env) * 0.60f;
                        break;
                    }
                    case SoundType::Thunder: {
                        float env = (t < 0.4f) ? (t / 0.4f) : std::exp(-(t - 0.4f) * 0.75f);
                        float rumble = ((t <= 0.0f) ? 0.0f : (t / 0.045f) * std::exp(1.0f - t / 0.045f) * 0.85f) + whiteNoise() * 0.40f;
                        soundSample = std::tanh(rumble * env) * 0.55f;
                        break;
                    }
                    case SoundType::RainAmbience: {
                        float rainDrop = (whiteNoise() > 0.92f) ? (whiteNoise() - 0.92f) * 1.8f : 0.0f;
                        float windDrone = whiteNoise() * 0.08f;
                        soundSample = (rainDrop * 0.40f + windDrone) * 0.35f;
                        break;
                    }
                    case SoundType::AimDownSight: {
                        float u = std::clamp(t / 0.16f, 0.0f, 1.0f);
                        float fabric = whiteNoise() * 4.0f * u * (1.0f - u) * 0.14f;
                        float click = (t > 0.06f) ? whiteNoise() * std::exp(-(t - 0.06f) * 110.0f) * 0.15f : 0.0f;
                        soundSample = fabric + click;
                        break;
                    }
                    case SoundType::AimDownSightOut: {
                        float u = std::clamp(t / 0.14f, 0.0f, 1.0f);
                        soundSample = whiteNoise() * 4.0f * u * (1.0f - u) * 0.09f;
                        break;
                    }
                    case SoundType::ItemDrop: {
                        float thud = (t <= 0.0f) ? 0.0f : (t / 0.010f) * std::exp(1.0f - t / 0.010f) * 0.75f;
                        float rattle = (t > 0.05f) ? whiteNoise() * std::exp(-(t - 0.05f) * 45.0f) * 0.30f : 0.0f;
                        float debris = whiteNoise() * std::exp(-t * 30.0f) * 0.25f;
                        soundSample = std::tanh((thud + rattle + debris) * 0.55f);
                        break;
                    }
                    case SoundType::HeavyBreathing: {
                        float phase = std::fmod(t * 1.35f, 1.0f);
                        float breathEnv = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
                        float airFlow = breathEnv * whiteNoise() * 0.16f;
                        soundSample = airFlow * std::exp(-t * 2.2f);
                        break;
                    }
                    case SoundType::SoldierBarkContact:
                    case SoundType::SoldierBarkSuppressed:
                    case SoundType::SoldierBarkWounded:
                    case SoundType::SoldierBarkFragOut:
                    case SoundType::SoldierBarkReloading:
                    case SoundType::SoldierBarkIncoming:
                    case SoundType::SoldierBarkFlanking:
                    case SoundType::SoldierBarkManDown:
                    {
                        float env = std::exp(-t * 2.4f);
                        soundSample = whiteNoise() * env * 0.18f;
                        break;
                    }
                    case SoundType::RadioBeepDatalink: {
                        float env = (t < 0.015f) ? (t / 0.015f) : std::exp(-(t - 0.015f) * 35.0f);
                        soundSample = whiteNoise() * env * 0.25f;
                        break;
                    }
                    case SoundType::HeavyMGShot: {
                        float boom = (t <= 0.0f) ? 0.0f : (t / 0.008f) * std::exp(1.0f - t / 0.008f) * 1.4f;
                        float crack = whiteNoise() * std::exp(-t * 22.0f) * 0.75f;
                        float mech = (t > 0.04f && t < 0.08f) ? whiteNoise() * 0.35f : 0.0f;
                        soundSample = std::tanh(boom + crack + mech) * 0.85f;
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

    // 2-Pole Biquad IIR Filter for natural acoustic bandwidth shaping without harmonic pitch
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void setLowpass(float fc, float Q, float fs = SR) {
            float w0 = 2.0f * 3.14159265f * fc / fs;
            float alpha = std::sin(w0) / (2.0f * Q);
            float cosw = std::cos(w0);
            float a0 = 1.0f + alpha;
            b0 = ((1.0f - cosw) * 0.5f) / a0;
            b1 = (1.0f - cosw) / a0;
            b2 = ((1.0f - cosw) * 0.5f) / a0;
            a1 = (-2.0f * cosw) / a0;
            a2 = (1.0f - alpha) / a0;
        }

        void setBandpass(float fc, float Q, float fs = SR) {
            float w0 = 2.0f * 3.14159265f * fc / fs;
            float alpha = std::sin(w0) / (2.0f * Q);
            float cosw = std::cos(w0);
            float a0 = 1.0f + alpha;
            b0 = alpha / a0;
            b1 = 0.0f;
            b2 = -alpha / a0;
            a1 = (-2.0f * cosw) / a0;
            a2 = (1.0f - alpha) / a0;
        }

        float process(float in) {
            float out = in * b0 + z1;
            z1 = in * b1 - a1 * out + z2;
            z2 = in * b2 - a2 * out;
            return out;
        }
    };

    // Unipolar kinetic pressure pulse (a single physical compression wave, 0% sine oscillation)
    auto unipolarPulse = [](float t, float tau) -> float {
        if (t <= 0.0f) return 0.0f;
        float x = t / tau;
        return x * std::exp(1.0f - x);
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
            float env = std::pow(t, 0.25f) * std::exp(-t * 9.5f); // Pure stochastic decay (zero sinusoidal pitch ringing)
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
    // 1. ASSAULT RIFLE (M4A1 5.56mm & AK-74M 5.45mm) - 4 Organic Variations (Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.rifleShotsOutdoor.resize(4);
    m_soundBank.rifleShotsBunker.resize(4);
    m_soundBank.rifleShotsWarehouse.resize(4);
    m_soundBank.rifleShotsTrench.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(1001u + static_cast<uint32_t>(v) * 337u);
        PinkFilter pinkF;
        Biquad bqBlast;
        bqBlast.setLowpass(95.0f + static_cast<float>(v) * 6.0f, 1.1f, SR);
        Biquad bqMid;
        bqMid.setBandpass(240.0f, 0.9f, SR);

        float coreDur = 0.22f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float tau_pulse = 0.0035f + static_cast<float>(v) * 0.0003f;
        float t_rise = 0.00003f;
        float t_pos = 0.00028f;
        float mech_time = 0.015f;

        float hpf_bite = 0.0f;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;

            // A. Supersonic Mach 2.5 N-Wave Shock Crack (Zero sine)
            float shock = 0.0f;
            if (t < t_rise) {
                shock = t / t_rise;
            } else if (t < t_pos) {
                shock = 1.0f - 2.0f * (t - t_rise) / (t_pos - t_rise);
            } else if (t < 0.0030f) {
                shock = -0.40f * std::exp(-(t - t_pos) / 0.0007f);
            }

            // B. High-Frequency Mach Crackle (Aperiodic filtered noise)
            float rawN = rng.nextFloat();
            float biteAlpha = std::min(1.0f, 2.0f * 3.14159f * 4200.0f / SR);
            hpf_bite += biteAlpha * (rawN - hpf_bite);
            float crackle = (rawN - hpf_bite) * std::exp(-t / 0.007f) * 1.10f;

            // C. Concussive Unipolar Air-Blast + Biquad Low-Pass Noise (NO SINE WAVE!)
            float pulse = unipolarPulse(t, tau_pulse) * 1.65f;
            float pN = pinkF.process(rawN);
            float blastNoise = bqBlast.process(pN) * std::exp(-t / 0.055f) * 2.8f;
            float midBody = bqMid.process(rawN) * std::exp(-t / 0.035f) * 1.2f;
            float concussion = std::tanh((pulse + blastNoise + midBody) * 1.35f);

            // D. Expanding Combustion Gas Wash
            float gasEnv = (1.0f - std::exp(-t / 0.0003f)) * std::exp(-t / 0.045f);
            float gasWash = std::tanh(pN * gasEnv * 2.2f) * 0.45f;

            // E. Mechanical Receiver Steel Friction (Stochastic impacts, zero tones)
            float mech = 0.0f;
            if (t >= mech_time && t <= mech_time + 0.035f) {
                float dtm = t - mech_time;
                float click1 = std::exp(-dtm * 320.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.45f;
                float click2 = (dtm > 0.012f) ? std::exp(-(dtm - 0.012f) * 280.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.38f : 0.0f;
                mech = click1 + click2;
            }

            core[i] = shock * 0.75f + crackle * 0.65f + concussion * 1.30f + gasWash * 0.40f + mech * 0.35f;
        }

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
    // 2. HEAVY SNIPER / DMR (SVD Marksman 7.62x54mmR) - 4 Organic Variations (Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.sniperShotsOutdoor.resize(4);
    m_soundBank.sniperShotsBunker.resize(4);
    m_soundBank.sniperShotsWarehouse.resize(4);
    m_soundBank.sniperShotsTrench.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(2741u + static_cast<uint32_t>(v) * 619u);
        PinkFilter pinkF;
        Biquad bqBlast;
        bqBlast.setLowpass(68.0f + static_cast<float>(v) * 4.0f, 1.2f, SR);
        Biquad bqBody;
        bqBody.setBandpass(180.0f, 0.9f, SR);

        float coreDur = 0.30f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float tau_pulse = 0.0055f + static_cast<float>(v) * 0.0004f;
        float t_rise = 0.00003f;
        float t_pos  = 0.00040f;
        float mech_time = 0.022f;
        float hpf_bite = 0.0f;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;

            float shock = 0.0f;
            if (t < t_rise) {
                shock = (t / t_rise) * 1.55f;
            } else if (t < t_pos) {
                shock = (1.0f - 2.0f * (t - t_rise) / (t_pos - t_rise)) * 1.55f;
            } else if (t < 0.0045f) {
                shock = -0.55f * std::exp(-(t - t_pos) / 0.0010f);
            }

            float rawN = rng.nextFloat();
            float biteAlpha = std::min(1.0f, 2.0f * 3.14159f * 3800.0f / SR);
            hpf_bite += biteAlpha * (rawN - hpf_bite);
            float crackle = (rawN - hpf_bite) * std::exp(-t / 0.010f) * 1.25f;

            float pulse = unipolarPulse(t, tau_pulse) * 2.2f;
            float pN = pinkF.process(rawN);
            float blastNoise = bqBlast.process(pN) * std::exp(-t / 0.080f) * 3.4f;
            float body = bqBody.process(rawN) * std::exp(-t / 0.050f) * 1.5f;
            float concussion = std::tanh((pulse + blastNoise + body) * 1.45f);

            float gasEnv = (1.0f - std::exp(-t / 0.0002f)) * std::exp(-t / 0.065f);
            float gasWash = std::tanh(pN * gasEnv * 2.6f) * 0.55f;

            float mech = 0.0f;
            if (t >= mech_time && t <= mech_time + 0.045f) {
                float dtm = t - mech_time;
                mech = std::exp(-dtm * 240.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.50f;
            }

            core[i] = shock * 0.85f + crackle * 0.75f + concussion * 1.45f + gasWash * 0.50f + mech * 0.40f;
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
    // 3. TACTICAL SIDEARM (M9A3 9x19mm Parabellum) - 4 Organic Variations (Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.pistolShotsOutdoor.resize(4);
    m_soundBank.pistolShotsBunker.resize(4);
    m_soundBank.pistolShotsWarehouse.resize(4);
    m_soundBank.pistolShotsTrench.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(3911u + static_cast<uint32_t>(v) * 431u);
        PinkFilter pinkF;
        Biquad bqBlast;
        bqBlast.setLowpass(130.0f, 1.0f, SR);

        float coreDur = 0.14f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float tau_pulse = 0.0022f;
        float t_rise = 0.00004f;
        float t_pos = 0.00025f;
        float mech_time = 0.010f;
        float hpf_bite = 0.0f;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;

            float shock = 0.0f;
            if (t < t_rise) {
                shock = (t / t_rise) * 1.10f;
            } else if (t < t_pos) {
                shock = (1.0f - 2.0f * (t - t_rise) / (t_pos - t_rise)) * 1.10f;
            } else if (t < 0.0025f) {
                shock = -0.30f * std::exp(-(t - t_pos) / 0.0006f);
            }

            float rawN = rng.nextFloat();
            float biteAlpha = std::min(1.0f, 2.0f * 3.14159f * 4800.0f / SR);
            hpf_bite += biteAlpha * (rawN - hpf_bite);
            float crackle = (rawN - hpf_bite) * std::exp(-t / 0.006f) * 0.95f;

            float pulse = unipolarPulse(t, tau_pulse) * 1.4f;
            float pN = pinkF.process(rawN);
            float blastNoise = bqBlast.process(pN) * std::exp(-t / 0.038f) * 2.2f;
            float concussion = std::tanh((pulse + blastNoise) * 1.25f);

            float gasEnv = (1.0f - std::exp(-t / 0.0002f)) * std::exp(-t / 0.030f);
            float gasWash = std::tanh(pN * gasEnv * 1.8f) * 0.35f;

            float mech = 0.0f;
            if (t >= mech_time && t <= mech_time + 0.025f) {
                float dtm = t - mech_time;
                mech = std::exp(-dtm * 350.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.40f;
            }

            core[i] = shock * 0.70f + crackle * 0.60f + concussion * 1.15f + gasWash * 0.35f + mech * 0.35f;
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

            // Massive Concussive 12.7mm Low-End (Zero-Sine Unipolar Shock & Deep Resonance)
            float subThump = unipolarPulse(t, 0.0075f) * 1.55f;

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
                mech = (rawN * 0.65f) * std::exp(-dtm * 140.0f);
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
    // 5. BULLET IMPACTS (Flesh, Steel, Sandbag, Dirt) - 3 Variations (Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.impacts.resize(3);
    for (size_t v = 0; v < 3; ++v) {
        PRNG rng(5119u + static_cast<uint32_t>(v) * 293u);
        PinkFilter pf;
        Biquad bqThud;
        bqThud.setLowpass(150.0f, 0.9f, SR);
        float dur = 0.24f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.impacts[v].resize(totalSamples, 0.0f);
        float tau_impact = 0.005f + static_cast<float>(v) * 0.0015f;
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float thudPulse = unipolarPulse(t, tau_impact) * 0.90f;
            float thudBody = bqThud.process(pf.process(rawN)) * std::exp(-t * 36.0f) * 1.2f;
            float snap = (t < 0.0015f) ? (1.0f - t / 0.0015f) * 0.95f : 0.0f;
            float grit = rawN * 0.35f * std::exp(-t * 32.0f);
            m_soundBank.impacts[v][i] = std::tanh((thudPulse + thudBody + snap + grit) * 0.90f);
        }
    }

    // =========================================================================
    // 6. SPENT SHELL CASINGS (Physical Brass Clink & Surface Impact) - 4 Variations Each (Zero-Sine)
    // =========================================================================
    // 6a. Concrete / Steel: Sharp impact click transient + fast-damped brass clatter (0% sine)
    m_soundBank.shellCasings.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(7129u + static_cast<uint32_t>(v) * 431u);
        PinkFilter pf;
        Biquad bqBrass;
        bqBrass.setBandpass(2800.0f + static_cast<float>(v) * 180.0f, 1.4f, SR);
        float dur = 0.11f; // Quick crisp duration
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.shellCasings[v].resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            // High-frequency broadband impact click tap (1.5ms impulse)
            float click = rawN * 0.40f * std::exp(-t * 1200.0f);
            // Damped high-frequency brass body ping via bandpass noise
            float ring = bqBrass.process(rawN) * std::exp(-t * 85.0f) * 0.45f;
            // Secondary bounce slap at 26ms
            float t2 = t - 0.026f;
            float slap = (t2 > 0.0f) ? (rawN * 0.20f * std::exp(-t2 * 110.0f)) : 0.0f;
            m_soundBank.shellCasings[v][i] = std::tanh((click + ring + slap) * 0.45f);
        }
    }

    // 6b. Dirt / Grass / Mud: Soft low-frequency muffled thud + damped soil friction (0% sine)
    m_soundBank.shellCasingsDirt.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(8451u + static_cast<uint32_t>(v) * 277u);
        PinkFilter pf;
        Biquad bqSoil;
        bqSoil.setLowpass(160.0f, 0.85f, SR);
        float dur = 0.06f; // Soft, short organic clatter
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.shellCasingsDirt[v].resize(totalSamples, 0.0f);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float thud = unipolarPulse(t, 0.006f) * 0.40f + bqSoil.process(pf.process(rawN)) * std::exp(-t * 110.0f) * 0.50f;
            float scuff = rawN * 0.15f * std::exp(-t * 80.0f);
            m_soundBank.shellCasingsDirt[v][i] = std::tanh((thud + scuff) * 0.40f);
        }
    }

    // =========================================================================
    // 7. DRY FIRE & WEAPON CLICKS (Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.08f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.dryFire.resize(totalSamples, 0.0f);
        PRNG rng(4411u);
        Biquad bqClick;
        bqClick.setBandpass(2200.0f, 1.2f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float snap = (t < 0.001f) ? (1.0f - t / 0.001f) * 0.85f : 0.0f;
            float click = bqClick.process(rawN) * std::exp(-t * 110.0f) * 0.65f;
            m_soundBank.dryFire[i] = std::tanh((snap + click) * 0.50f);
        }
    }

    // =========================================================================
    // 8. RELOAD & MAGAZINE INSERTION (Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.55f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.reload.resize(totalSamples, 0.0f);
        PRNG rng(7103u);
        PinkFilter pf;
        Biquad bqLatch;
        bqLatch.setBandpass(1800.0f, 1.0f, SR);
        Biquad bqWell;
        bqWell.setLowpass(320.0f, 0.9f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float rustle = pf.process(rawN) * 0.18f * std::exp(-t * 10.0f);
            // Mag release click at 0.05s
            float magOut = 0.0f;
            if (t > 0.05f && t < 0.09f) {
                float dt = t - 0.05f;
                magOut = bqLatch.process(rawN) * std::exp(-dt * 130.0f) * 0.65f;
            }
            // Mag insert rock at 0.32s
            float magIn = 0.0f;
            if (t > 0.32f && t < 0.42f) {
                float dt = t - 0.32f;
                float seatingThump = unipolarPulse(dt, 0.008f) * 0.75f;
                float guideScuff = bqWell.process(rawN) * std::exp(-dt * 65.0f) * 0.85f;
                magIn = seatingThump + guideScuff;
            }
            m_soundBank.reload[i] = std::tanh((rustle + magOut + magIn) * 0.70f);
        }
    }

    // =========================================================================
    // 9. CHAMBER ROUND (Charging Handle Rack - Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.35f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.chamberRound.resize(totalSamples, 0.0f);
        PRNG rng(8821u);
        PinkFilter pf;
        Biquad bqSteel;
        bqSteel.setBandpass(1400.0f, 1.1f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            // Pull back at 0.02s
            float pull = 0.0f;
            if (t > 0.02f && t < 0.09f) {
                float dt = t - 0.02f;
                pull = bqSteel.process(rawN) * std::exp(-dt * 85.0f) * 0.55f;
            }
            // Slam forward at 0.14s
            float slam = 0.0f;
            if (t > 0.14f && t < 0.25f) {
                float dt = t - 0.14f;
                float boltThump = unipolarPulse(dt, 0.006f) * 0.90f;
                float lockRattle = pf.process(rawN) * std::exp(-dt * 70.0f) * 0.60f;
                slam = boltThump + lockRattle;
            }
            m_soundBank.chamberRound[i] = std::tanh((pull + slam) * 0.80f);
        }
    }

    // =========================================================================
    // 10. SUPERSONIC BULLET FLYBY (Doppler Sonic Crack - Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.25f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.bulletFlyby.resize(totalSamples, 0.0f);
        PRNG rng(6511u);
        PinkFilter pf;
        Biquad bqWhoosh;
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            // Sharp N-wave snap at t=0.04s
            float snap = (t >= 0.038f && t <= 0.042f) ? (1.0f - (t - 0.038f) / 0.002f * 2.0f) * 0.95f : 0.0f;
            // Rapid Doppler whoosh via swept bandpass noise
            float fc = 800.0f + 1600.0f * (1.0f - std::clamp((t - 0.04f) * 15.0f, 0.0f, 1.0f));
            bqWhoosh.setBandpass(fc, 0.9f, SR);
            float whoosh = bqWhoosh.process(pf.process(rawN)) * std::exp(-std::abs(t - 0.04f) * 35.0f) * 0.85f;
            m_soundBank.bulletFlyby[i] = std::tanh((snap + whoosh) * 0.85f);
        }
    }

    // =========================================================================
    // 11. BALLISTIC HELMET PING (Steel & Kevlar Ricochet - Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.55f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.helmetPing.resize(totalSamples, 0.0f);
        PRNG rng(7733u);
        Biquad bqPing1, bqPing2;
        bqPing1.setBandpass(3200.0f, 2.2f, SR);
        bqPing2.setBandpass(5800.0f, 2.0f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float attack = (t < 0.001f) ? (1.0f - t / 0.001f) * 0.95f : 0.0f;
            float ring = (bqPing1.process(rawN) * 0.6f + bqPing2.process(rawN) * 0.4f) * std::exp(-t * 22.0f) * 0.85f;
            m_soundBank.helmetPing[i] = std::tanh((attack + ring) * 0.88f);
        }
    }

    // =========================================================================
    // 12. BODY ARMOR CERAMIC DEFLECT (Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.35f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.armorDeflect.resize(totalSamples, 0.0f);
        PRNG rng(8191u);
        PinkFilter pf;
        Biquad bqPlate;
        bqPlate.setLowpass(220.0f, 0.9f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float shatter = rawN * 0.65f * std::exp(-t * 40.0f);
            float thud = unipolarPulse(t, 0.008f) * 0.85f + bqPlate.process(pf.process(rawN)) * std::exp(-t * 26.0f) * 0.70f;
            m_soundBank.armorDeflect[i] = std::tanh((shatter + thud) * 0.80f);
        }
    }

    // =========================================================================
    // 13. FLESH IMPACT (Visceral Wet Cavitation - Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.30f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.fleshImpact.resize(totalSamples, 0.0f);
        PRNG rng(9011u);
        PinkFilter pf;
        Biquad bqCav;
        bqCav.setLowpass(160.0f, 0.85f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float pop = unipolarPulse(t, 0.007f) * 0.95f + bqCav.process(pf.process(rawN)) * std::exp(-t * 32.0f) * 0.80f;
            float wet = rawN * 0.35f * std::exp(-t * 35.0f);
            m_soundBank.fleshImpact[i] = std::tanh((pop + wet) * 0.85f);
        }
    }

    // =========================================================================
    // 14. SPRINT SLIDE & TACTICAL BOOT DRAG (Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.45f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.sprintSlide.resize(totalSamples, 0.0f);
        PRNG rng(9323u);
        PinkFilter pf;
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float u = std::clamp(t / dur, 0.0f, 1.0f);
            float env = 4.0f * u * (1.0f - u);
            float gravel = pf.process(rng.nextFloat() * 2.0f - 1.0f) * env * 0.45f;
            m_soundBank.sprintSlide[i] = gravel;
        }
    }

    // =========================================================================
    // 15. TACTICAL GRENADE PIN & SPOON FLY-OFF (Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.18f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.grenadePin.resize(totalSamples, 0.0f);
        PRNG rng(5512u);
        Biquad bqSpoon;
        bqSpoon.setBandpass(2600.0f, 1.3f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float ping = bqSpoon.process(rawN) * std::exp(-t * 85.0f) * 0.70f;
            float snap = (t < 0.0015f) ? (1.0f - t / 0.0015f) * 0.85f : 0.0f;
            m_soundBank.grenadePin[i] = (ping + snap) * 0.60f;
        }
    }

    // =========================================================================
    // 16. GRENADE GROUND BOUNCE (Zero-Sine Physical Model)
    // =========================================================================
    {
        float dur = 0.22f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.grenadeBounce.resize(totalSamples, 0.0f);
        PRNG rng(6621u);
        PinkFilter pf;
        Biquad bqCastIron;
        bqCastIron.setBandpass(650.0f, 1.2f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float thud = unipolarPulse(t, 0.007f) * 0.75f;
            float ring = bqCastIron.process(pf.process(rawN)) * std::exp(-t * 45.0f) * 0.55f;
            m_soundBank.grenadeBounce[i] = std::tanh((thud + ring) * 0.75f);
        }
    }

    // =========================================================================
    // 17. HE GRENADE EXPLOSION (Concussive Detonation & Debris Fallout - Zero-Sine)
    // =========================================================================
    {
        PRNG rng(9606u);
        PinkFilter pf;
        Biquad bqDet;
        bqDet.setLowpass(85.0f, 1.0f, SR);
        float coreDur = 0.45f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float shock = unipolarPulse(t, 0.005f) * 2.2f;
            float subBass = bqDet.process(pf.process(rawN)) * std::exp(-t * 4.5f) * 1.50f;
            float rumble = pf.process(rawN) * std::exp(-t * 5.2f) * 0.90f;
            float debris = (t > 0.12f) ? rawN * 0.35f * std::exp(-(t - 0.12f) * 3.5f) : 0.0f;
            core[i] = shock * 0.75f + subBass * 0.85f + rumble * 0.65f + debris * 0.45f;
        }

        auto wet = applyOutdoorRollingReverb(core, 2.20f, 1.10f, 1.4f, rng);
        m_soundBank.grenadeExplosion.resize(wet.size());
        for (size_t i = 0; i < wet.size(); ++i) {
            m_soundBank.grenadeExplosion[i] = std::tanh(wet[i] * 0.92f);
        }
    }

    // =========================================================================
    // 18. ARTILLERY SHELL EXPLOSION (152mm/155mm Heavy Howitzer Blast - Zero-Sine)
    // =========================================================================
    {
        PRNG rng(9887u);
        PinkFilter pf;
        Biquad bqSeismic;
        bqSeismic.setLowpass(50.0f, 1.1f, SR);
        float coreDur = 0.65f;
        size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float shock = unipolarPulse(t, 0.008f) * 2.8f;
            float seismic = bqSeismic.process(pf.process(rawN)) * std::exp(-t * 2.8f) * 1.80f;
            float roar = pf.process(rawN) * std::exp(-t * 3.8f) * 1.20f;
            float earthRumble = (t > 0.08f) ? pf.process(rawN) * 0.85f * std::exp(-t * 1.8f) : 0.0f;
            core[i] = shock * 0.80f + seismic * 1.10f + roar * 0.75f + earthRumble * 0.85f;
        }

        auto wet = applyOutdoorRollingReverb(core, 3.20f, 1.35f, 0.95f, rng);
        m_soundBank.artilleryExplosion.resize(wet.size());
        for (size_t i = 0; i < wet.size(); ++i) {
            m_soundBank.artilleryExplosion[i] = std::tanh(wet[i] * 0.95f);
        }
    }

    // =========================================================================
    // 19. BALLISTIC BARRIER PENETRATION CRACK (Kinetic Rupture & Exit Shock - Zero-Sine)
    // =========================================================================
    {
        float dur = 0.28f;
        size_t totalSamples = static_cast<size_t>(SR * dur);
        m_soundBank.penetrationCrack.resize(totalSamples, 0.0f);
        PRNG rng(7119u);
        PinkFilter pf;
        Biquad bqWhip;
        bqWhip.setBandpass(1800.0f, 1.0f, SR);
        for (size_t i = 0; i < totalSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float snap = rawN * std::exp(-t * 65.0f) * 0.95f;
            float whip = bqWhip.process(rawN) * std::exp(-t * 42.0f) * 0.75f;
            float resonance = pf.process(rawN) * std::exp(-t * 22.0f) * 0.40f;
            m_soundBank.penetrationCrack[i] = std::tanh(snap + whip + resonance);
        }
    }

    // =========================================================================
    // 20. CONCRETE FOOTSTEPS (4 Organic Boot-on-Pavement Variations - Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.footstepsConcrete.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.16f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsConcrete[v].resize(samples, 0.0f);
        PRNG rng(5101u + static_cast<uint32_t>(v) * 233u);
        PinkFilter pf;
        Biquad bqHeel;
        bqHeel.setLowpass(160.0f, 0.85f, SR);
        Biquad bqSlap;
        bqSlap.setBandpass(260.0f, 0.90f, SR);

        float tau_heel = 0.009f;

        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float pN = pf.process(rawN);

            // A. Viscoelastic Rubber Sole Impact (Unipolar displacement pulse + damped low-pass noise, 0% sine)
            float heelPulse = unipolarPulse(t, tau_heel) * 0.85f;
            float heelBody = bqHeel.process(pN) * std::exp(-t * 55.0f) * 1.4f;

            // B. High-Frequency Pavement Friction / Micro-Grit Snap
            float gritSnap = rawN * std::exp(-t * 95.0f) * 0.55f;

            // C. Secondary Sole Slap (Delayed 14ms)
            float soleSlap = (t > 0.014f) ? bqSlap.process(rawN) * std::exp(-(t - 0.014f) * 70.0f) * 0.65f : 0.0f;

            // D. Tread Texture Release Scuff
            float scuff = pN * std::exp(-t * 38.0f) * 0.25f;

            m_soundBank.footstepsConcrete[v][i] = std::tanh((heelPulse + heelBody + gritSnap + soleSlap + scuff) * 1.15f);
        }
    }

    // =========================================================================
    // 21. DIRT / GRAVEL FOOTSTEPS (4 Organic Earth Variations - Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.footstepsDirt.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.18f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsDirt[v].resize(samples, 0.0f);
        PRNG rng(6201u + static_cast<uint32_t>(v) * 317u);
        PinkFilter pf;
        Biquad bqSoil;
        bqSoil.setLowpass(90.0f, 0.85f, SR);
        Biquad bqCrunch;
        bqCrunch.setBandpass(380.0f, 0.80f, SR);

        float tau_thud = 0.012f;

        // 8 asynchronous particulate grain fracture times
        float grainTimes[8];
        for (int k = 0; k < 8; ++k) {
            grainTimes[k] = 0.005f + (static_cast<float>(k) * 0.009f) + rng.nextFloat() * 0.005f;
        }

        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float pN = pf.process(rawN);

            // A. Soil Displacement (Unipolar pulse + damped low-frequency soil resonance, 0% sine)
            float soilPulse = unipolarPulse(t, tau_thud) * 0.80f;
            float soilBody = bqSoil.process(pN) * std::exp(-t * 42.0f) * 1.35f;

            // B. Granular Earth / Pebble Crunch (Broadband stochastic particulate clicks, zero sine)
            float grainClicks = 0.0f;
            for (int k = 0; k < 8; ++k) {
                if (t >= grainTimes[k] && t < grainTimes[k] + 0.008f) {
                    float dtg = t - grainTimes[k];
                    grainClicks += (rng.nextFloat() * 2.0f - 1.0f) * std::exp(-dtg * 320.0f) * 0.26f;
                }
            }

            // C. Broad-spectrum Earth Shear Texture
            float crunch = bqCrunch.process(rawN) * std::exp(-t * 32.0f) * 0.55f;

            m_soundBank.footstepsDirt[v][i] = std::tanh((soilPulse + soilBody + grainClicks + crunch) * 1.10f);
        }
    }

    // =========================================================================
    // 22. WOOD FOOTSTEPS (4 Organic Timber / Plank Variations - Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.footstepsWood.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.17f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsWood[v].resize(samples, 0.0f);
        PRNG rng(7301u + static_cast<uint32_t>(v) * 401u);
        PinkFilter pf;
        Biquad bqWood1;
        bqWood1.setBandpass(175.0f, 1.1f, SR);
        Biquad bqWood2;
        bqWood2.setBandpass(350.0f, 0.9f, SR);

        float tau_tap = 0.008f;

        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float pN = pf.process(rawN);

            float tapPulse = unipolarPulse(t, tau_tap) * 0.75f;
            float cavity1 = bqWood1.process(pN) * std::exp(-t * 48.0f) * 1.4f;
            float cavity2 = bqWood2.process(rawN) * std::exp(-t * 62.0f) * 0.65f;
            float creak = (t > 0.016f) ? rawN * std::exp(-(t - 0.016f) * 45.0f) * 0.28f : 0.0f;

            m_soundBank.footstepsWood[v][i] = std::tanh((tapPulse + cavity1 + cavity2 + creak) * 1.05f);
        }
    }

    // =========================================================================
    // 23. MUD FOOTSTEPS (4 Organic Viscous Slurry Variations - Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.footstepsMud.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.22f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsMud[v].resize(samples, 0.0f);
        PRNG rng(8401u + static_cast<uint32_t>(v) * 509u);
        PinkFilter pf;
        Biquad bqSuction;
        bqSuction.setBandpass(85.0f, 0.95f, SR);

        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float pN = pf.process(rawN);

            float suction = bqSuction.process(pN) * std::exp(-t * 26.0f) * 1.4f;
            float squelch = rawN * std::exp(-t * 22.0f) * 0.50f;
            float splatter = (t > 0.030f) ? (rng.nextFloat() * 2.0f - 1.0f) * std::exp(-(t - 0.030f) * 45.0f) * 0.35f : 0.0f;

            m_soundBank.footstepsMud[v][i] = std::tanh((suction + squelch + splatter) * 1.05f);
        }
    }

    // =========================================================================
    // 24. WATER FOOTSTEPS (4 Hydrodynamic Splashing Variations - Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.footstepsWater.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.24f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.footstepsWater[v].resize(samples, 0.0f);
        PRNG rng(9501u + static_cast<uint32_t>(v) * 613u);
        PinkFilter pf;
        Biquad bqSplash;
        bqSplash.setBandpass(220.0f, 0.9f, SR);

        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float pN = pf.process(rawN);

            float entry = bqSplash.process(pN) * std::exp(-t * 35.0f) * 1.35f;
            float spray = rawN * std::exp(-t * 24.0f) * 0.45f;
            float bubble = (t > 0.035f) ? pf.process(rawN) * std::exp(-(t - 0.035f) * 40.0f) * 0.35f : 0.0f;

            m_soundBank.footstepsWater[v][i] = std::tanh((entry + spray + bubble) * 1.05f);
        }
    }
    // =========================================================================
    // 25. TACTICAL GEAR RUSTLE (4 Cordura Webbing & Sling Clink Variations - Zero-Sine Physical Model)
    // =========================================================================
    m_soundBank.gearRustles.resize(4);
    for (size_t v = 0; v < 4; ++v) {
        float dur = 0.15f;
        size_t samples = static_cast<size_t>(SR * dur);
        m_soundBank.gearRustles[v].resize(samples, 0.0f);
        PRNG rng(10601u + static_cast<uint32_t>(v) * 719u);
        PinkFilter pf;
        Biquad bqBuckle;
        bqBuckle.setBandpass(1650.0f, 1.2f, SR);
        for (size_t i = 0; i < samples; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawN = rng.nextFloat() * 2.0f - 1.0f;
            float u = std::clamp(t / 0.15f, 0.0f, 1.0f);
            float env = 4.0f * u * (1.0f - u);
            float fabric = pf.process(rawN) * env * 0.35f;
            float buckle = (t > 0.02f && t < 0.05f) ? bqBuckle.process(rawN) * std::exp(-(t - 0.02f) * 90.0f) * 0.25f : 0.0f;
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
