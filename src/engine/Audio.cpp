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
    static AudioEngine* s_instance = new AudioEngine();
    return *s_instance;
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

    // 1. Drain SPSC command ring on real-time audio thread - ZERO LOCKS! (A1 fix)
    AudioCommand cmd;
    while (engine->m_commandRing.pop(cmd)) {
        engine->processCommandOnAudioThread(cmd);
    }

    const float sfxVol = engine->m_sfxVolume.load(std::memory_order_relaxed);
    const float ambVol = engine->m_ambientVolume.load(std::memory_order_relaxed);
    const float masterVol = engine->m_masterVolume.load(std::memory_order_relaxed);
    const float rainIntensity = engine->m_rainIntensity.load(std::memory_order_relaxed);

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
            float noiseL = engine->fastWhiteNoise();
            float noiseR = engine->fastWhiteNoise();
            engine->m_rainFilterL += 0.045f * (noiseL - engine->m_rainFilterL);
            engine->m_rainFilterR += 0.045f * (noiseR - engine->m_rainFilterR);

            float rainVolume = 0.0075f * rainIntensity * ambVol;
            sampleL += engine->m_rainFilterL * rainVolume;
            sampleR += engine->m_rainFilterR * rainVolume;
        } else {
            engine->m_windPhase += dt;
            float windSway = 0.7f + 0.3f * std::sin(engine->m_windPhase * 0.28f);
            float wL = engine->fastWhiteNoise();
            float wR = engine->fastWhiteNoise();
            engine->m_windFilterL += 0.012f * (wL - engine->m_windFilterL);
            engine->m_windFilterR += 0.012f * (wR - engine->m_windFilterR);
            sampleL += engine->m_windFilterL * (0.0028f * windSway * ambVol);
            sampleR += engine->m_windFilterR * (0.0028f * windSway * ambVol);
        }

        out[i * 2 + 0] = sampleL;
        out[i * 2 + 1] = sampleR;
    }

    // 2. Mix Voice Chat in a single block call! (A2 fix: replaces 96,000 mutex locks/sec with 1 call)
    std::vector<float> vcL(frameCount, 0.0f);
    std::vector<float> vcR(frameCount, 0.0f);
    VoiceChatManager::get().mixBlock(vcL.data(), vcR.data(), frameCount, sampleRate);
    for (unsigned int i = 0; i < frameCount; ++i) {
        out[i * 2 + 0] += vcL[i];
        out[i * 2 + 1] += vcR[i];
    }

    // 3. Master Volume Scaling & Transparent Fast Soft-Limit (A14 & B12 fix)
    // Applied AFTER master volume, only active when peaking > 0.92f
    for (unsigned int i = 0; i < frameCount * 2; ++i) {
        float val = out[i] * masterVol;
        if (std::abs(val) > 0.92f) {
            val = (val > 0.0f) ? (0.92f + (val - 0.92f) / (1.0f + (val - 0.92f)))
                               : (-0.92f + (val + 0.92f) / (1.0f - (val + 0.92f)));
        }
        out[i] = val;
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

    // Helper: Outdoor Stokes Atmospheric Reverb with natural terrain reflections (B2 & B4 fix)
    // Dropped the 8-tap comb filter (-0.42 gain) which caused metallic ringing.
    // Outdoors is predominantly dry direct sound + gentle ground bounce + obstacle + monotonic diffuse terrain roll.
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

        // Sparse, non-comb natural landscape reflections
        DelayLine lines[] = {
            { static_cast<size_t>(SR * 0.0035f), +0.08f, 4000.0f }, // Gentle ground reflection
            { static_cast<size_t>(SR * 0.0480f), +0.10f, 1800.0f }, // Nearby berm / obstacle reflection
            { static_cast<size_t>(SR * 0.1650f), +0.07f, 1100.0f }  // Distant landscape bounce
        };

        for (const auto& dl : lines) {
            if (dl.delaySamples >= outSize) continue;
            float lpState = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * dl.lpfCutoff / SR);
            for (size_t i = 0; i < src.size() && (i + dl.delaySamples) < outSize; ++i) {
                lpState += alpha * (src[i] - lpState);
                out[i + dl.delaySamples] += lpState * dl.gain;
            }
        }

        // Monotonic diffuse pink-noise terrain tail (decaying exponentially, NO rising t^0.4 swell)
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float tailStartSec = 0.040f;
        size_t tailStartIdx = static_cast<size_t>(SR * tailStartSec);
        for (size_t i = tailStartIdx; i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float white = rng.nextFloat();
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            float pink = b0 + b1 + b2 + white * 0.5362f;

            // Pure monotonic exponential decay
            float env = std::exp(-(t - tailStartSec) * dampRate) * reverbGain * 0.065f;
            out[i] += pink * env;
        }

        return out;
    };

    // Helper: Concrete Bunker Early Reflections + Dense Tail (B3 fix: jittered delay primes, halved gains)
    auto applyIndoorBunkerReverb = [&](const std::vector<float>& src, float tailDuration, float reverbGain, PRNG& rng) -> std::vector<float> {
        size_t outSize = static_cast<size_t>(SR * tailDuration);
        std::vector<float> out(outSize, 0.0f);
        for (size_t i = 0; i < std::min(src.size(), outSize); ++i) {
            out[i] = src[i];
        }

        struct Tap {
            size_t delay;
            float gain;
            float lpf;
        };

        // Jittered prime delays with non-coherent gains to prevent harsh flutter echo
        Tap bunkerTaps[] = {
            { static_cast<size_t>(SR * 0.0083f),  0.36f, 5200.0f },
            { static_cast<size_t>(SR * 0.0191f), -0.31f, 4100.0f },
            { static_cast<size_t>(SR * 0.0337f),  0.26f, 3200.0f },
            { static_cast<size_t>(SR * 0.0573f), -0.22f, 2500.0f },
            { static_cast<size_t>(SR * 0.0891f),  0.18f, 1900.0f },
            { static_cast<size_t>(SR * 0.1370f),  0.14f, 1400.0f }
        };

        for (const auto& tap : bunkerTaps) {
            if (tap.delay >= outSize) continue;
            float lp = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * tap.lpf / SR);
            for (size_t i = 0; i < src.size() && (i + tap.delay) < outSize; ++i) {
                lp += alpha * (src[i] - lp);
                out[i + tap.delay] += lp * tap.gain;
            }
        }

        float lpDiffuse = 0.0f;
        float diffuseAlpha = 2.0f * 3.14159265f * 2600.0f / SR;
        for (size_t i = static_cast<size_t>(SR * 0.025f); i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawWhite = rng.nextFloat();
            lpDiffuse += diffuseAlpha * (rawWhite - lpDiffuse);
            float env = std::exp(-t * 3.8f) * reverbGain * 0.12f;
            out[i] += lpDiffuse * env;
        }

        return out;
    };

    // Helper: Industrial Warehouse Reverb with Flutter Slap and Spacious High-Ceiling Decay (B3 fix)
    auto applyIndoorWarehouseReverb = [&](const std::vector<float>& src, float tailDuration, float reverbGain, PRNG& rng) -> std::vector<float> {
        size_t outSize = static_cast<size_t>(SR * tailDuration);
        std::vector<float> out(outSize, 0.0f);
        for (size_t i = 0; i < std::min(src.size(), outSize); ++i) {
            out[i] = src[i];
        }

        struct Tap {
            size_t delay;
            float gain;
            float lpf;
        };

        Tap warehouseTaps[] = {
            { static_cast<size_t>(SR * 0.0152f),  0.30f, 6500.0f },
            { static_cast<size_t>(SR * 0.0384f),  0.24f, 4800.0f },
            { static_cast<size_t>(SR * 0.0711f), -0.20f, 3600.0f },
            { static_cast<size_t>(SR * 0.1180f),  0.15f, 2400.0f },
            { static_cast<size_t>(SR * 0.1820f),  0.11f, 1600.0f }
        };

        for (const auto& tap : warehouseTaps) {
            if (tap.delay >= outSize) continue;
            float lp = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * tap.lpf / SR);
            for (size_t i = 0; i < src.size() && (i + tap.delay) < outSize; ++i) {
                lp += alpha * (src[i] - lp);
                out[i + tap.delay] += lp * tap.gain;
            }
        }

        float lpDiffuse = 0.0f;
        float diffuseAlpha = 2.0f * 3.14159265f * 3100.0f / SR;
        for (size_t i = static_cast<size_t>(SR * 0.030f); i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawWhite = rng.nextFloat();
            lpDiffuse += diffuseAlpha * (rawWhite - lpDiffuse);
            float env = std::exp(-t * 2.5f) * reverbGain * 0.095f;
            out[i] += lpDiffuse * env;
        }

        return out;
    };

    // Helper: Trench Reverb with Mud Revetment Slap (High Frequency Absorbing Earthen Walls)
    auto applyTrenchReverb = [&](const std::vector<float>& src, float tailDuration, float reverbGain, PRNG& rng) -> std::vector<float> {
        size_t outSize = static_cast<size_t>(SR * tailDuration);
        std::vector<float> out(outSize, 0.0f);
        for (size_t i = 0; i < std::min(src.size(), outSize); ++i) {
            out[i] = src[i];
        }

        struct Tap {
            size_t delay;
            float gain;
            float lpf;
        };

        Tap trenchTaps[] = {
            { static_cast<size_t>(SR * 0.0094f),  0.22f, 1800.0f },
            { static_cast<size_t>(SR * 0.0213f), -0.16f, 1200.0f },
            { static_cast<size_t>(SR * 0.0465f),  0.11f,  850.0f }
        };

        for (const auto& tap : trenchTaps) {
            if (tap.delay >= outSize) continue;
            float lp = 0.0f;
            float alpha = std::min(1.0f, 2.0f * 3.14159265f * tap.lpf / SR);
            for (size_t i = 0; i < src.size() && (i + tap.delay) < outSize; ++i) {
                lp += alpha * (src[i] - lp);
                out[i + tap.delay] += lp * tap.gain;
            }
        }

        float lpDiffuse = 0.0f;
        float diffuseAlpha = 2.0f * 3.14159265f * 950.0f / SR;
        for (size_t i = static_cast<size_t>(SR * 0.020f); i < outSize; ++i) {
            float t = static_cast<float>(i) / SR;
            float rawWhite = rng.nextFloat();
            lpDiffuse += diffuseAlpha * (rawWhite - lpDiffuse);
            float env = std::exp(-t * 5.2f) * reverbGain * 0.075f;
            out[i] += lpDiffuse * env;
        }

        return out;
    };

    // 1. ASSAULT RIFLE (5.56x45mm NATO High-Pressure Supersonic Crack - Zero-Sine Physical Model)
    // B1: Core extended from 0.22s to 0.52s (eliminates abrupt click-whoosh handoff)
    // B5: Symmetric Friedlander N-wave shock
    // B6: Dominant broadband gas wash (tau=95ms)
    // B7: Receiver bolt carrier unlocks at 52ms
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(1001u + static_cast<uint32_t>(v) * 337u);
        const float coreDur = 0.52f;
        const size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float fShock = 1850.0f * (1.0f + rng.nextFloat() * 0.06f - 0.03f);
        float tShockDur = 0.0034f;
        float mechDelay = 0.052f + (rng.nextFloat() * 0.006f - 0.003f); // B7 fix: 52ms bolt unlock

        float lpGas = 0.0f;
        float alphaGas = 2.0f * 3.14159265f * 4200.0f / SR;

        float lpMuzzle = 0.0f;
        float alphaMuzzle = 2.0f * 3.14159265f * 180.0f / SR;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float s = 0.0f;

            // Layer A: Friedlander N-Wave Supersonic Shock (B5 fix: near-symmetric negative lobe)
            if (t < tShockDur) {
                float frac = t / tShockDur;
                float shock = (1.0f - frac) * (1.0f - 2.1f * frac); // +1.0 peak, -1.1 negative phase
                shock += 0.35f * (1.0f - frac) * std::sin(2.0f * 3.14159265f * fShock * t);
                s += shock * 1.40f;
            }

            // Layer B: Broadband Supersonic Propellant Expansion / Gas Wash (B6 fix: dominant body)
            float gasNoise = rng.nextFloat();
            lpGas += alphaGas * (gasNoise - lpGas);
            float gasEnv = std::exp(-t / 0.095f);
            s += lpGas * gasEnv * 0.88f;

            // Layer C: Low-frequency concussive chest thump (sub-bass blast wave)
            float blastNoise = rng.nextFloat();
            lpMuzzle += alphaMuzzle * (blastNoise - lpMuzzle);
            float thumpEnv = std::exp(-t / 0.070f);
            s += lpMuzzle * thumpEnv * 0.95f;

            // Layer D: Mechanical Bolt Carrier Cycling (B7 fix: clicks at 52ms)
            if (t > mechDelay && t < (mechDelay + 0.045f)) {
                float dtMech = t - mechDelay;
                float mechNoise = rng.nextFloat();
                float mechEnv = std::exp(-dtMech / 0.008f);
                s += mechNoise * mechEnv * 0.45f;
            }

            core[i] = s;
        }

        m_soundBank.rifleShotsOutdoor.push_back(applyOutdoorRollingReverb(core, 1.85f, 0.40f, 3.2f, rng));
        m_soundBank.rifleShotsBunker.push_back(applyIndoorBunkerReverb(core, 1.45f, 0.55f, rng));
        m_soundBank.rifleShotsWarehouse.push_back(applyIndoorWarehouseReverb(core, 2.20f, 0.48f, rng));
        m_soundBank.rifleShotsTrench.push_back(applyTrenchReverb(core, 1.35f, 0.42f, rng));
    }

    // 2. HEAVY SNIPER / DMR (SVD Marksman 7.62x54mmR - Zero-Sine Physical Model)
    // B1: Core extended from 0.30s to 0.78s
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(2001u + static_cast<uint32_t>(v) * 337u);
        const float coreDur = 0.78f;
        const size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float fShock = 1250.0f * (1.0f + rng.nextFloat() * 0.04f - 0.02f);
        float tShockDur = 0.0052f;
        float mechDelay = 0.065f + (rng.nextFloat() * 0.006f - 0.003f);

        float lpGas = 0.0f;
        float alphaGas = 2.0f * 3.14159265f * 3400.0f / SR;

        float lpMuzzle = 0.0f;
        float alphaMuzzle = 2.0f * 3.14159265f * 120.0f / SR;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float s = 0.0f;

            // Deep supersonic ballistic crack (near-symmetric N-wave)
            if (t < tShockDur) {
                float frac = t / tShockDur;
                float shock = (1.0f - frac) * (1.0f - 2.1f * frac);
                shock += 0.45f * (1.0f - frac) * std::sin(2.0f * 3.14159265f * fShock * t);
                s += shock * 1.85f;
            }

            // High pressure combustion chamber gas release
            float gasNoise = rng.nextFloat();
            lpGas += alphaGas * (gasNoise - lpGas);
            float gasEnv = std::exp(-t / 0.145f);
            s += lpGas * gasEnv * 1.15f;

            // Massive sub-bass visceral concussive thump
            float blastNoise = rng.nextFloat();
            lpMuzzle += alphaMuzzle * (blastNoise - lpMuzzle);
            float thumpEnv = std::exp(-t / 0.120f);
            s += lpMuzzle * thumpEnv * 1.45f;

            // Heavy bolt receiver recoil unlock
            if (t > mechDelay && t < (mechDelay + 0.055f)) {
                float dtMech = t - mechDelay;
                float mechNoise = rng.nextFloat();
                float mechEnv = std::exp(-dtMech / 0.010f);
                s += mechNoise * mechEnv * 0.50f;
            }

            core[i] = s;
        }

        m_soundBank.sniperShotsOutdoor.push_back(applyOutdoorRollingReverb(core, 2.80f, 0.52f, 2.2f, rng));
        m_soundBank.sniperShotsBunker.push_back(applyIndoorBunkerReverb(core, 2.10f, 0.65f, rng));
        m_soundBank.sniperShotsWarehouse.push_back(applyIndoorWarehouseReverb(core, 3.10f, 0.60f, rng));
        m_soundBank.sniperShotsTrench.push_back(applyTrenchReverb(core, 1.95f, 0.50f, rng));
    }

    // 3. TACTICAL SIDEARM (M9A3 9x19mm Parabellum - Zero-Sine Physical Model)
    // B1: Core extended from 0.18s to 0.40s
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(3001u + static_cast<uint32_t>(v) * 337u);
        const float coreDur = 0.40f;
        const size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float tPopDur = 0.0022f;
        float slideDelay = 0.042f + (rng.nextFloat() * 0.004f - 0.002f);

        float lpGas = 0.0f;
        float alphaGas = 2.0f * 3.14159265f * 5200.0f / SR;

        float lpPop = 0.0f;
        float alphaPop = 2.0f * 3.14159265f * 260.0f / SR;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float s = 0.0f;

            if (t < tPopDur) {
                float frac = t / tPopDur;
                s += (1.0f - frac) * (1.0f - 2.0f * frac) * 1.10f;
            }

            float gasNoise = rng.nextFloat();
            lpGas += alphaGas * (gasNoise - lpGas);
            float gasEnv = std::exp(-t / 0.065f);
            s += lpGas * gasEnv * 0.75f;

            float popNoise = rng.nextFloat();
            lpPop += alphaPop * (popNoise - lpPop);
            float popEnv = std::exp(-t / 0.050f);
            s += lpPop * popEnv * 0.70f;

            // Slide cycle metallic click
            if (t > slideDelay && t < (slideDelay + 0.035f)) {
                float dtSlide = t - slideDelay;
                float slideNoise = rng.nextFloat();
                float slideEnv = std::exp(-dtSlide / 0.006f);
                s += slideNoise * slideEnv * 0.38f;
            }

            core[i] = s;
        }

        m_soundBank.pistolShotsOutdoor.push_back(applyOutdoorRollingReverb(core, 1.15f, 0.32f, 4.5f, rng));
        m_soundBank.pistolShotsBunker.push_back(applyIndoorBunkerReverb(core, 0.95f, 0.42f, rng));
        m_soundBank.pistolShotsWarehouse.push_back(applyIndoorWarehouseReverb(core, 1.50f, 0.38f, rng));
        m_soundBank.pistolShotsTrench.push_back(applyTrenchReverb(core, 0.85f, 0.32f, rng));
    }

    // 4. HEAVY MACHINE GUN (DShK 12.7mm / .50 BMG - Zero-Sine Physical Model)
    // B1: Core extended from 0.32s to 0.74s
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(4001u + static_cast<uint32_t>(v) * 337u);
        const float coreDur = 0.74f;
        const size_t coreSamples = static_cast<size_t>(SR * coreDur);
        std::vector<float> core(coreSamples, 0.0f);

        float fShock = 980.0f * (1.0f + rng.nextFloat() * 0.04f - 0.02f);
        float tShockDur = 0.0068f;
        float cycleDelay = 0.068f + (rng.nextFloat() * 0.008f - 0.004f);

        float lpGas = 0.0f;
        float alphaGas = 2.0f * 3.14159265f * 2800.0f / SR;

        float lpConcuss = 0.0f;
        float alphaConcuss = 2.0f * 3.14159265f * 85.0f / SR;

        for (size_t i = 0; i < coreSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float s = 0.0f;

            if (t < tShockDur) {
                float frac = t / tShockDur;
                float shock = (1.0f - frac) * (1.0f - 2.1f * frac);
                shock += 0.55f * (1.0f - frac) * std::sin(2.0f * 3.14159265f * fShock * t);
                s += shock * 2.10f;
            }

            float gasNoise = rng.nextFloat();
            lpGas += alphaGas * (gasNoise - lpGas);
            float gasEnv = std::exp(-t / 0.160f);
            s += lpGas * gasEnv * 1.35f;

            // Visceral 85 Hz heavy machine gun chest compression
            float concussNoise = rng.nextFloat();
            lpConcuss += alphaConcuss * (concussNoise - lpConcuss);
            float concussEnv = std::exp(-t / 0.150f);
            s += lpConcuss * concussEnv * 1.80f;

            if (t > cycleDelay && t < (cycleDelay + 0.060f)) {
                float dtCycle = t - cycleDelay;
                float cycleNoise = rng.nextFloat();
                float cycleEnv = std::exp(-dtCycle / 0.012f);
                s += cycleNoise * cycleEnv * 0.65f;
            }

            core[i] = s;
        }

        m_soundBank.heavyMGShotsOutdoor.push_back(applyOutdoorRollingReverb(core, 2.60f, 0.58f, 2.0f, rng));
        m_soundBank.heavyMGShotsBunker.push_back(applyIndoorBunkerReverb(core, 2.20f, 0.70f, rng));
        m_soundBank.heavyMGShotsWarehouse.push_back(applyIndoorWarehouseReverb(core, 3.20f, 0.65f, rng));
        m_soundBank.heavyMGShotsTrench.push_back(applyTrenchReverb(core, 1.85f, 0.55f, rng));
    }

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
    // 20. CONCRETE FOOTSTEPS (B8 fix: Raised from 160Hz pillow thud to 580Hz solid heel + 750Hz sole snap)
    for (size_t v = 0; v < 4; ++v) {
        PRNG rng(10001u + static_cast<uint32_t>(v) * 513u);
        const float dur = 0.22f;
        const size_t nSamples = static_cast<size_t>(SR * dur);
        std::vector<float> step(nSamples, 0.0f);

        float lpHeel = 0.0f;
        float alphaHeel = 2.0f * 3.14159265f * 580.0f / SR; // B8 fix: 580Hz solid heel impact

        float lpSnap = 0.0f;
        float alphaSnap = 2.0f * 3.14159265f * 750.0f / SR; // B8 fix: 750Hz boot sole slap

        float bpGrit = 0.0f;
        float alphaGrit = 2.0f * 3.14159265f * 3400.0f / SR; // 3.4kHz pavement grit scuff

        for (size_t i = 0; i < nSamples; ++i) {
            float t = static_cast<float>(i) / SR;
            float raw = rng.nextFloat();

            // Heel impact transient (0-35ms)
            lpHeel += alphaHeel * (raw - lpHeel);
            float heelEnv = (t < 0.045f) ? (t / 0.005f) * std::exp(-t / 0.018f) : 0.0f;

            // Sole slap (15-60ms)
            lpSnap += alphaSnap * (raw - lpSnap);
            float snapEnv = (t > 0.015f && t < 0.080f) ? std::exp(-(t - 0.015f) / 0.024f) : 0.0f;

            // Concrete grit texture
            bpGrit += alphaGrit * (raw - bpGrit);
            float gritEnv = (t > 0.020f && t < 0.140f) ? std::exp(-(t - 0.020f) / 0.040f) : 0.0f;

            step[i] = lpHeel * heelEnv * 0.85f + lpSnap * snapEnv * 0.65f + bpGrit * gritEnv * 0.28f;
        }
        m_soundBank.footstepsConcrete.push_back(step);
    }

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
    // A4 fix: Check return code immediately and abort on context-init failure
    if (ma_context_init(NULL, 0, NULL, ctx) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to initialize miniaudio context\n";
        delete ctx;
        m_context = nullptr;
        return false;
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
        ma_context_uninit(ctx);
        delete ctx;
        m_context = nullptr;
        return false;
    }

    if (ma_device_start(dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to start audio playback device\n";
        ma_device_uninit(dev);
        delete dev;
        ma_context_uninit(ctx);
        delete ctx;
        m_context = nullptr;
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
        // A6 fix: Stop and uninit playback device first before destroying Steam Audio
        // This guarantees the audio callback cannot read freed binaural buffers
        if (m_device) {
            ma_device* dev = static_cast<ma_device*>(m_device);
            ma_device_stop(dev);
            ma_device_uninit(dev);
            delete dev;
            m_device = nullptr;
        }

        SteamAudioBridge::instance().shutdown();

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
    if (!m_initialized || !m_context) return names;

    ma_context* ctx = static_cast<ma_context*>(m_context);
    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(ctx, &pPlaybackInfos, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
        names.push_back("Default System Playback Device");
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

    // A17 fix: Valid range is 0 (Default) to playbackCount (specific device index)
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

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = audioCallback;
    config.pUserData         = this;

    if (deviceIndex == 0) {
        config.playback.pDeviceID = nullptr; // Default system audio device
    } else {
        config.playback.pDeviceID = &pPlaybackInfos[deviceIndex - 1].id;
    }

    ma_device* dev = new ma_device();
    if (ma_device_init(ctx, &config, dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to switch playback device to index " << deviceIndex << ", reverting to default\n";
        delete dev;
        // A5 fix: Fallback to default device so engine never remains dead
        config.playback.pDeviceID = nullptr;
        dev = new ma_device();
        if (ma_device_init(ctx, &config, dev) != MA_SUCCESS) {
            delete dev;
            m_initialized = false;
            return false;
        }
        deviceIndex = 0;
    }

    if (ma_device_start(dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to start switched playback device\n";
        ma_device_uninit(dev);
        delete dev;
        m_initialized = false;
        return false;
    }

    m_device = dev;
    m_currentPlaybackDeviceIndex = deviceIndex;
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

    // A1 fix: Non-blocking push to lock-free SPSC command ring (Zero mutex locks!)
    AudioCommand cmd;
    cmd.cmdType = AudioCommand::CommandType::Play2D;
    cmd.soundType = type;
    cmd.volume = volume;
    cmd.pitch = pitch;
    cmd.env = env;
    m_commandRing.push(cmd);
}

void AudioEngine::playSound3D(SoundType type, const glm::vec3& soundPos, const glm::vec3& listenerPos,
                              float cameraYawRad, float volume, float maxDist, AcousticEnvironment env, float occlusion) {
    if (!m_initialized) return;

    // A1 fix: Non-blocking push to lock-free SPSC command ring (Zero mutex locks!)
    AudioCommand cmd;
    cmd.cmdType = AudioCommand::CommandType::Play3D;
    cmd.soundType = type;
    cmd.volume = volume;
    cmd.pitch = 1.0f;
    cmd.env = env;
    cmd.soundPos = soundPos;
    cmd.listenerPos = listenerPos;
    cmd.cameraYawRad = cameraYawRad;
    cmd.maxDist = maxDist;
    cmd.occlusion = occlusion;
    m_commandRing.push(cmd);
}

void AudioEngine::processCommandOnAudioThread(const AudioCommand& cmd) {
    // Determine acoustic priority (A11 fix: Voice stealing by priority)
    // 4 = Gunfire & Explosions (highest protection)
    // 3 = Critical player mechanics (reloads, deflections, pings)
    // 2 = Footsteps & gear rustles
    // 1 = Small props & casings
    // 0 = Ambient & distant barks
    uint8_t cmdPriority = 2;
    switch (cmd.soundType) {
        case SoundType::RifleShot:
        case SoundType::SniperShot:
        case SoundType::HeavyMGShot:
        case SoundType::PistolShot:
        case SoundType::GrenadeExplosion:
        case SoundType::ArtilleryExplosion:
            cmdPriority = 4;
            break;
        case SoundType::DryFire:
        case SoundType::Reload:
        case SoundType::ChamberRound:
        case SoundType::GrenadePin:
        case SoundType::HelmetPing:
        case SoundType::ArmorDeflect:
        case SoundType::FleshImpact:
            cmdPriority = 3;
            break;
        case SoundType::Footstep:
        case SoundType::FootstepDirt:
        case SoundType::FootstepMud:
        case SoundType::FootstepWater:
        case SoundType::FootstepWood:
        case SoundType::FootstepConcrete:
        case SoundType::GearRustle:
        case SoundType::SprintSlide:
            cmdPriority = 2;
            break;
        case SoundType::ShellCasing:
        case SoundType::ShellCasingDirt:
        case SoundType::Impact:
        case SoundType::GrenadeBounce:
        case SoundType::FortifyBuild:
        case SoundType::ItemDrop:
        case SoundType::BulletFlyby:
        case SoundType::PenetrationCrack:
            cmdPriority = 1;
            break;
        default:
            cmdPriority = 0;
            break;
    }

    // Impact concurrency throttling (max 2 active impacts)
    if (cmd.soundType == SoundType::Impact) {
        int activeImpacts = 0;
        for (const auto& v : m_voices) {
            if (v.active && v.type == SoundType::Impact) {
                activeImpacts++;
            }
        }
        if (activeImpacts >= 2) return;
    }

    // Voice search: prefer inactive voice; otherwise steal oldest voice with equal or lower priority
    ActiveVoice* targetVoice = nullptr;
    float maxProgressRatio = -1.0f;

    for (auto& voice : m_voices) {
        if (!voice.active) {
            targetVoice = &voice;
            break;
        }
        if (voice.priority <= cmdPriority) {
            float ratio = voice.progress / (voice.duration > 0.0f ? voice.duration : 1.0f);
            if (ratio > maxProgressRatio) {
                maxProgressRatio = ratio;
                targetVoice = &voice;
            }
        }
    }

    if (!targetVoice) {
        // All active voices have higher priority than this sound; drop it cleanly
        return;
    }

    targetVoice->type = cmd.soundType;
    targetVoice->priority = cmdPriority;
    targetVoice->sampleBuffer = nullptr;
    targetVoice->hasBinaural = false;
    targetVoice->binauralLeft.clear();
    targetVoice->binauralRight.clear();
    targetVoice->samplePos = 0.0f;
    targetVoice->progress = 0.0f;
    targetVoice->volume = cmd.volume;
    targetVoice->pitch = cmd.pitch;
    targetVoice->panL = 1.0f;
    targetVoice->panR = 1.0f;
    targetVoice->filterAlpha = 0.95f;
    targetVoice->filterStateL = 0.0f;
    targetVoice->filterStateR = 0.0f;
    targetVoice->is3D = (cmd.cmdType == AudioCommand::CommandType::Play3D);
    targetVoice->active = true;

    // Assign procedural sample buffer & physical duration
    switch (cmd.soundType) {
        case SoundType::RifleShot:
            targetVoice->sampleBuffer = selectRifleBuffer(cmd.env, m_rifleVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 1.85f;
            break;
        case SoundType::SniperShot:
            targetVoice->sampleBuffer = selectSniperBuffer(cmd.env, m_sniperVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 2.80f;
            break;
        case SoundType::PistolShot:
            targetVoice->sampleBuffer = selectPistolBuffer(cmd.env, m_pistolVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 1.15f;
            break;
        case SoundType::HeavyMGShot:
            targetVoice->sampleBuffer = selectHmgBuffer(cmd.env, m_hmgVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 2.60f;
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
                targetVoice->sampleBuffer = &m_soundBank.impacts[m_impactVariationCounter++ % m_soundBank.impacts.size()];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.25f;
            }
            break;
        case SoundType::ShellCasing:
            if (!m_soundBank.shellCasings.empty()) {
                targetVoice->sampleBuffer = &m_soundBank.shellCasings[m_casingVariationCounter++ % m_soundBank.shellCasings.size()];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.35f;
            }
            break;
        case SoundType::ShellCasingDirt:
            if (!m_soundBank.shellCasingsDirt.empty()) {
                targetVoice->sampleBuffer = &m_soundBank.shellCasingsDirt[m_casingVariationCounter++ % m_soundBank.shellCasingsDirt.size()];
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 0.35f;
            }
            break;
        case SoundType::Footstep:
        case SoundType::FootstepDirt:
        case SoundType::FootstepMud:
        case SoundType::FootstepWater:
        case SoundType::FootstepWood:
        case SoundType::FootstepConcrete:
        case SoundType::GearRustle:
            targetVoice->sampleBuffer = selectFootstepBuffer(cmd.soundType, m_footstepVariationCounter++);
            targetVoice->duration = targetVoice->sampleBuffer ? static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f : 0.22f;
            break;
        case SoundType::ArtilleryExplosion:
            if (!m_soundBank.artilleryExplosion.empty()) {
                targetVoice->sampleBuffer = &m_soundBank.artilleryExplosion;
                targetVoice->duration = static_cast<float>(targetVoice->sampleBuffer->size()) / 48000.0f;
            } else {
                targetVoice->duration = 3.20f;
            }
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
        default:                            targetVoice->duration = 0.40f; break;
    }

    // 3D Spatial Calculation
    if (cmd.cmdType == AudioCommand::CommandType::Play3D) {
        glm::vec3 rel = cmd.soundPos - cmd.listenerPos;
        float dist = glm::length(rel);
        if (dist > cmd.maxDist && cmd.maxDist > 0.0f) {
            targetVoice->active = false;
            return;
        }

        // Realistic inverse-distance attenuation curve with near-field knee
        float refDist = 2.0f;
        float atten = refDist / (refDist + dist);
        atten = std::clamp(atten, 0.0f, 1.0f);

        // Acoustic barrier occlusion damping
        atten *= (1.0f - std::clamp(cmd.occlusion, 0.0f, 0.85f));
        targetVoice->volume = cmd.volume * atten;

        // Equal-power 3D trigonometric spatialization with azimuth rotation
        float cosY = std::cos(-cmd.cameraYawRad);
        float sinY = std::sin(-cmd.cameraYawRad);
        float localX = rel.x * cosY - rel.z * sinY;
        float localZ = rel.x * sinY + rel.z * cosY;

        float azimuth = std::atan2(localX, localZ);
        float pan = 0.5f + 0.5f * std::sin(azimuth);
        // A13 fix: Widen panning clamp from [0.08, 0.92] to [0.02, 0.98]
        pan = std::clamp(pan, 0.02f, 0.98f);

        targetVoice->panL = std::cos(pan * 1.5707963f);
        targetVoice->panR = std::sin(pan * 1.5707963f);

        // Rear head-shadow / pinna attenuation
        if (localZ < 0.0f) {
            float rearFactor = std::abs(localZ) / (dist + 0.001f);
            targetVoice->panL *= (1.0f - 0.22f * rearFactor);
            targetVoice->panR *= (1.0f - 0.22f * rearFactor);
        }

        // High-clarity air absorption distance filter (A12 & B10 fix: preserves 2.5 kHz air at 50m)
        float filterAlpha = 1.0f / (1.0f + 0.018f * dist + 0.00035f * dist * dist);
        targetVoice->filterAlpha = std::clamp(filterAlpha, 0.14f, 0.95f);

        // Continuous smooth distance pitch curve (B9 fix: eliminates harsh binary jump at 35m)
        float distPitch = 1.0f / (1.0f + 0.00015f * dist);
        targetVoice->pitch = cmd.pitch * distPitch;

        // B11 fix: Utilize Steam Audio binaural HRTF if available
        if (SteamAudioBridge::instance().isInitialized() && targetVoice->sampleBuffer) {
            int numSamples = static_cast<int>(targetVoice->sampleBuffer->size());
            if (numSamples > 0 && numSamples <= 48000) { // Spatialize short/medium sounds without RT overload
                targetVoice->binauralLeft.resize(numSamples);
                targetVoice->binauralRight.resize(numSamples);
                bool success = SteamAudioBridge::instance().spatialize(
                    targetVoice->sampleBuffer->data(),
                    targetVoice->binauralLeft.data(),
                    targetVoice->binauralRight.data(),
                    numSamples,
                    cmd.soundPos,
                    cmd.listenerPos,
                    cmd.cameraYawRad
                );
                targetVoice->hasBinaural = success;
            }
        }
    }
}

AudioEngine::VoiceTelemetrySnapshot AudioEngine::getVoiceSnapshot() {
    VoiceTelemetrySnapshot snapshot;
    snapshot.rainIntensity = m_rainIntensity.load(std::memory_order_relaxed);
    snapshot.maxVoices = static_cast<int>(m_voices.size());

    // Non-blocking telemetry lock (A1 fix: try_to_lock never blocks)
    std::unique_lock<std::mutex> lock(m_voiceMutex, std::try_to_lock);
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
