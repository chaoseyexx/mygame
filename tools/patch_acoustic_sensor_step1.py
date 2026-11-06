import sys

print("Patching Audio.hpp and Audio.cpp for Acoustic Wave System (Step 1)...")

with open('src/engine/Audio.hpp', 'r', encoding='utf-8') as f:
    hpp = f.read()

# 1. Add AcousticWaveEvent before struct AudioCommand
old_marker = "struct AudioCommand {"
assert old_marker in hpp, "Could not find struct AudioCommand"

wave_structs = """struct AcousticWaveEvent {
    uint32_t id = 0;
    SoundType soundType = SoundType::Footstep;
    glm::vec3 origin{0.0f};
    float initialDb = 70.0f;
    float maxRadius = 180.0f;
    float currentRadius = 0.0f;
    float speed = 80.0f; // Visual propagation speed in m/s
    float lifetime = 0.0f;
    float maxLifetime = 2.0f;
    AcousticEnvironment env = AcousticEnvironment::OpenField;
    bool isPlayerSource = false;
    glm::vec4 color{0.0f, 0.88f, 1.0f, 0.85f};
};

inline float getSoundTypeBaseDb(SoundType type) {
    switch (type) {
        case SoundType::RifleShot:          return 118.0f;
        case SoundType::SniperShot:         return 126.0f;
        case SoundType::HeavyMGShot:        return 128.0f;
        case SoundType::PistolShot:         return 110.0f;
        case SoundType::GrenadeExplosion:   return 135.0f;
        case SoundType::ArtilleryExplosion: return 142.0f;
        case SoundType::PenetrationCrack:   return 102.0f;
        case SoundType::BulletFlyby:        return 96.0f;
        case SoundType::HelmetPing:         return 88.0f;
        case SoundType::ArmorDeflect:       return 84.0f;
        case SoundType::FleshImpact:        return 78.0f;
        case SoundType::SprintSlide:        return 74.0f;
        case SoundType::Footstep:
        case SoundType::FootstepConcrete:
        case SoundType::FootstepWood:       return 62.0f;
        case SoundType::FootstepDirt:
        case SoundType::FootstepMud:        return 58.0f;
        case SoundType::FootstepWater:      return 68.0f;
        case SoundType::GearRustle:         return 50.0f;
        case SoundType::Reload:
        case SoundType::ChamberRound:       return 60.0f;
        case SoundType::DryFire:
        case SoundType::TacticalClick:      return 52.0f;
        case SoundType::ShellCasing:
        case SoundType::ShellCasingDirt:    return 48.0f;
        case SoundType::GrenadePin:
        case SoundType::GrenadeBounce:      return 56.0f;
        case SoundType::ItemEquip:
        case SoundType::ItemDrop:           return 54.0f;
        default:                            return 65.0f;
    }
}

inline glm::vec4 getSoundTypeAcousticColor(SoundType type) {
    switch (type) {
        case SoundType::RifleShot:
        case SoundType::SniperShot:
        case SoundType::HeavyMGShot:
        case SoundType::PistolShot:
        case SoundType::GrenadeExplosion:
        case SoundType::ArtilleryExplosion:
            return glm::vec4(1.0f, 0.22f, 0.15f, 0.95f); // Concussive Red/Orange

        case SoundType::Footstep:
        case SoundType::FootstepConcrete:
        case SoundType::FootstepDirt:
        case SoundType::FootstepMud:
        case SoundType::FootstepWood:
        case SoundType::FootstepWater:
        case SoundType::SprintSlide:
        case SoundType::GearRustle:
            return glm::vec4(0.0f, 0.88f, 1.0f, 0.85f); // Stealth Cyan / Sonar Blue

        case SoundType::DryFire:
        case SoundType::Reload:
        case SoundType::ChamberRound:
        case SoundType::TacticalClick:
        case SoundType::ShellCasing:
        case SoundType::ShellCasingDirt:
        case SoundType::GrenadePin:
        case SoundType::GrenadeBounce:
        case SoundType::ItemEquip:
        case SoundType::ItemDrop:
        case SoundType::HelmetPing:
        case SoundType::ArmorDeflect:
        case SoundType::Impact:
            return glm::vec4(1.0f, 0.85f, 0.15f, 0.90f); // Mechanical Gold / Amber

        default:
            return glm::vec4(0.2f, 1.0f, 0.5f, 0.85f); // Voice / Tactical Emerald Green
    }
}

"""

hpp = hpp.replace(old_marker, wave_structs + old_marker, 1)

# 2. Add public methods to AudioEngine in Audio.hpp
pub_target = "    bool setPlaybackDevice(int deviceIndex);"
assert pub_target in hpp, "Could not find pub_target"

pub_methods = """    bool setPlaybackDevice(int deviceIndex);

    // Real-Time F3 Acoustic Propagation & Sensor Tracking
    void recordAcousticWave(SoundType type, const glm::vec3& origin, float volume, float maxDist, AcousticEnvironment env, bool isPlayer = false);
    void updateAcoustics(float dt);
    std::vector<AcousticWaveEvent> getActiveAcousticWaves() const;
    void setListener(const glm::vec3& pos, float yawRad) {
        m_lastListenerPos = pos;
        m_lastCameraYawRad = yawRad;
    }
    const glm::vec3& getListenerPos() const { return m_lastListenerPos; }
    float getListenerYawRad() const { return m_lastCameraYawRad; }
    void setAcousticVisualSpeed(float speed) { m_acousticWaveVisualSpeed = std::clamp(speed, 20.0f, 343.0f); }
    float getAcousticVisualSpeed() const { return m_acousticWaveVisualSpeed; }"""

hpp = hpp.replace(pub_target, pub_methods, 1)

# 3. Add private members to AudioEngine in Audio.hpp
priv_target = "    glm::vec3 m_lastListenerPos{0.0f};"
assert priv_target in hpp, "Could not find priv_target"

priv_members = """    mutable std::mutex m_acousticWaveMutex;
    std::vector<AcousticWaveEvent> m_acousticWaves;
    uint32_t m_nextWaveId = 1;
    float m_acousticWaveVisualSpeed = 80.0f; // Configurable visual propagation speed
    glm::vec3 m_lastListenerPos{0.0f};"""

hpp = hpp.replace(priv_target, priv_members, 1)

with open('src/engine/Audio.hpp', 'w', encoding='utf-8') as f:
    f.write(hpp)

print("Audio.hpp updated successfully.")

# 4. Implement recordAcousticWave, updateAcoustics, and hook playSound/playSound3D in Audio.cpp
with open('src/engine/Audio.cpp', 'r', encoding='utf-8') as f:
    cpp = f.read()

# Replace playSound & playSound3D
old_play_block = """void AudioEngine::playSound(SoundType type, float volume, float pitch, AcousticEnvironment env) {
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
}"""

assert old_play_block in cpp, "Could not find old_play_block"

new_play_block = """void AudioEngine::recordAcousticWave(SoundType type, const glm::vec3& origin, float volume, float maxDist, AcousticEnvironment env, bool isPlayer) {
    if (!m_initialized) return;

    AcousticWaveEvent wave;
    wave.id = m_nextWaveId++;
    wave.soundType = type;
    wave.origin = origin;
    wave.initialDb = getSoundTypeBaseDb(type) * std::clamp(volume, 0.2f, 1.5f);
    wave.maxRadius = (maxDist > 5.0f) ? maxDist : 40.0f;
    wave.currentRadius = 0.5f;
    wave.speed = m_acousticWaveVisualSpeed;
    wave.lifetime = 0.0f;
    wave.maxLifetime = wave.maxRadius / wave.speed;
    wave.env = env;
    wave.isPlayerSource = isPlayer;
    wave.color = getSoundTypeAcousticColor(type);

    std::lock_guard<std::mutex> lock(m_acousticWaveMutex);
    if (m_acousticWaves.size() >= 64) {
        m_acousticWaves.erase(m_acousticWaves.begin());
    }
    m_acousticWaves.push_back(wave);
}

void AudioEngine::updateAcoustics(float dt) {
    std::lock_guard<std::mutex> lock(m_acousticWaveMutex);
    for (auto it = m_acousticWaves.begin(); it != m_acousticWaves.end(); ) {
        it->lifetime += dt;
        it->currentRadius = it->lifetime * it->speed;
        if (it->currentRadius >= it->maxRadius || it->lifetime >= it->maxLifetime) {
            it = m_acousticWaves.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<AcousticWaveEvent> AudioEngine::getActiveAcousticWaves() const {
    std::lock_guard<std::mutex> lock(m_acousticWaveMutex);
    return m_acousticWaves;
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

    // Record acoustic wave for F3 debug visualizer
    float estRange = 35.0f;
    if (type == SoundType::RifleShot || type == SoundType::SniperShot || type == SoundType::HeavyMGShot) estRange = 180.0f;
    else if (type == SoundType::PistolShot) estRange = 95.0f;
    recordAcousticWave(type, m_lastListenerPos, volume, estRange, env, true);
}

void AudioEngine::playSound3D(SoundType type, const glm::vec3& soundPos, const glm::vec3& listenerPos,
                              float cameraYawRad, float volume, float maxDist, AcousticEnvironment env, float occlusion) {
    if (!m_initialized) return;

    m_lastListenerPos = listenerPos;
    m_lastCameraYawRad = cameraYawRad;

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

    // Record acoustic wave for F3 debug visualizer
    recordAcousticWave(type, soundPos, volume, maxDist, env, false);
}"""

cpp = cpp.replace(old_play_block, new_play_block, 1)

with open('src/engine/Audio.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp)

print("Audio.cpp updated successfully with Step 1!")
