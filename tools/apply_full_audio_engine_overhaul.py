import sys

print("Applying Full Audio Engine Overhaul Patch with exact section markers...")

with open('src/engine/Audio.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# -------------------------------------------------------------
# 1. Update Reverbs in generateProceduralSoundBank()
# -------------------------------------------------------------
old_reverbs_start = "    // Helper: Outdoor Stokes Atmospheric Reverb with dynamic high-frequency air damping"
old_reverbs_end = "    // 1. ASSAULT RIFLE (M4A1 5.56mm & AK-74M 5.45mm)"

idx1 = text.find(old_reverbs_start)
idx2 = text.find(old_reverbs_end)
assert idx1 != -1, "Could not find old_reverbs_start"
assert idx2 != -1, "Could not find old_reverbs_end"

new_reverbs = """    // Helper: Outdoor Stokes Atmospheric Reverb with natural terrain reflections (B2 & B4 fix)
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

"""

text = text[:idx1] + new_reverbs + text[idx2:]
print("1. Reverbs updated successfully.")

# -------------------------------------------------------------
# 2. Update Gunfire Physical Modeling (Rifle, Sniper, Pistol, Heavy MG)
# -------------------------------------------------------------
old_guns_start = "    // 1. ASSAULT RIFLE (M4A1 5.56mm & AK-74M 5.45mm)"
old_guns_end = "    // 5. BULLET IMPACTS (Flesh, Steel, Sandbag, Dirt)"

idx_g1 = text.find(old_guns_start)
idx_g2 = text.find(old_guns_end)
assert idx_g1 != -1 and idx_g2 != -1, "Could not find old gunfire modeling block"

new_guns = """    // 1. ASSAULT RIFLE (5.56x45mm NATO High-Pressure Supersonic Crack - Zero-Sine Physical Model)
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

"""

text = text[:idx_g1] + new_guns + text[idx_g2:]
print("2. Gunfire physical modeling updated successfully.")

# -------------------------------------------------------------
# 3. Update Footsteps EQ (B8 fix: Concrete 580Hz lowpass + 750Hz slap)
# -------------------------------------------------------------
old_concrete_start = "    // 20. CONCRETE FOOTSTEPS (4 Organic Boot-on-Pavement Variations - Zero-Sine Physical Model)"
old_concrete_end = "    // 21. DIRT / GRAVEL FOOTSTEPS (4 Organic Earth Variations - Zero-Sine Physical Model)"

idx_c1 = text.find(old_concrete_start)
idx_c2 = text.find(old_concrete_end)
assert idx_c1 != -1 and idx_c2 != -1, "Could not find old concrete footstep block"

new_concrete = """    // 20. CONCRETE FOOTSTEPS (B8 fix: Raised from 160Hz pillow thud to 580Hz solid heel + 750Hz sole snap)
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

"""

text = text[:idx_c1] + new_concrete + text[idx_c2:]
print("3. Footsteps EQ updated successfully.")

# -------------------------------------------------------------
# 4. Update init(), shutdown(), and setPlaybackDevice() (A4, A5, A6, A17)
# -------------------------------------------------------------
old_life_start = "bool AudioEngine::init() {"
old_life_end = "const std::vector<float>* AudioEngine::selectRifleBuffer"

idx_l1 = text.find(old_life_start)
idx_l2 = text.find(old_life_end)
assert idx_l1 != -1 and idx_l2 != -1, "Could not find old lifecycle block"

new_life = """bool AudioEngine::init() {
    if (m_initialized) return true;

    // Initialize the AAA procedural physical weapon sound bank
    generateProceduralSoundBank();

    ma_context* ctx = new ma_context();
    // A4 fix: Check return code immediately and abort on context-init failure
    if (ma_context_init(NULL, 0, NULL, ctx) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to initialize miniaudio context\\n";
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
        std::cerr << "[Audio Error] Failed to initialize miniaudio playback device\\n";
        delete dev;
        ma_context_uninit(ctx);
        delete ctx;
        m_context = nullptr;
        return false;
    }

    if (ma_device_start(dev) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to start audio playback device\\n";
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
        std::cerr << "[Audio Error] Failed to switch playback device to index " << deviceIndex << ", reverting to default\\n";
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
        std::cerr << "[Audio Error] Failed to start switched playback device\\n";
        ma_device_uninit(dev);
        delete dev;
        m_initialized = false;
        return false;
    }

    m_device = dev;
    m_currentPlaybackDeviceIndex = deviceIndex;
    std::string devName = (deviceIndex == 0) ? "Default System Playback Device" : pPlaybackInfos[deviceIndex - 1].name;
    std::cout << "[Audio Engine] Successfully switched playback output to: " << devName << "\\n";
    return true;
}

"""

text = text[:idx_l1] + new_life + text[idx_l2:]
print("4. Lifecycle updated successfully.")

# -------------------------------------------------------------
# 5. Overhaul playSound, playSound3D, processCommandOnAudioThread, getVoiceSnapshot
# -------------------------------------------------------------
old_play_start = "void AudioEngine::playSound(SoundType type, float volume, float pitch, AcousticEnvironment env) {"
idx_p = text.find(old_play_start)
assert idx_p != -1, "Could not find old playSound block"

new_playback_section = """void AudioEngine::playSound(SoundType type, float volume, float pitch, AcousticEnvironment env) {
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
"""

text = text[:idx_p] + new_playback_section
print("5. Playback and command processing overhaul completed.")

with open('src/engine/Audio.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Audio.cpp overhaul written successfully!")
