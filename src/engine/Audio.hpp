#pragma once
#include "Types.hpp"
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>

namespace Mercenary {

enum class SoundType {
    RifleShot,
    PistolShot,
    SniperShot,
    DryFire,
    Reload,
    Footstep,
    FootstepDirt,
    FootstepMud,
    FootstepWater,
    FootstepWood,
    FootstepConcrete,
    GearRustle,
    Bandage,
    Impact,
    TacticalClick,
    ItemEquip,
    StashMove,
    MoneyRustle,
    DeployHorn,
    Thunder,
    RainAmbience,
    ArtilleryWhistle,
    ArtilleryExplosion,
    FortifyBuild,
    RadioChatter,
    AimDownSight,
    AimDownSightOut,
    ItemDrop,
    ShellCasing,
    ShellCasingDirt,
    HeavyBreathing,
    ChamberRound,
    BulletFlyby,
    SoldierBarkContact,
    SoldierBarkSuppressed,
    SoldierBarkWounded,
    SoldierBarkReloading,
    SoldierBarkIncoming,
    RadioBeepDatalink,
    HeavyMGShot,
    HelmetPing,
    ArmorDeflect,
    FleshImpact,
    SprintSlide,
    GrenadePin,
    GrenadeBounce,
    GrenadeExplosion,
    SoldierBarkFlanking,
    SoldierBarkManDown,
    SoldierBarkFragOut,
    PenetrationCrack
};

inline const char* getSoundTypeName(SoundType type) {
    switch (type) {
        case SoundType::RifleShot:          return "RifleShot";
        case SoundType::PistolShot:         return "PistolShot";
        case SoundType::SniperShot:         return "SniperShot";
        case SoundType::DryFire:            return "DryFire";
        case SoundType::Reload:             return "Reload";
        case SoundType::Footstep:           return "Footstep (Generic)";
        case SoundType::FootstepDirt:       return "Footstep (Dirt)";
        case SoundType::FootstepMud:        return "Footstep (Mud)";
        case SoundType::FootstepWater:      return "Footstep (Water)";
        case SoundType::FootstepWood:       return "Footstep (Wood)";
        case SoundType::FootstepConcrete:   return "Footstep (Concrete)";
        case SoundType::GearRustle:         return "GearRustle";
        case SoundType::Bandage:            return "Bandage";
        case SoundType::Impact:             return "Impact";
        case SoundType::TacticalClick:      return "TacticalClick";
        case SoundType::ItemEquip:          return "ItemEquip";
        case SoundType::StashMove:          return "StashMove";
        case SoundType::MoneyRustle:        return "MoneyRustle";
        case SoundType::DeployHorn:         return "DeployHorn";
        case SoundType::Thunder:            return "Thunder";
        case SoundType::RainAmbience:       return "RainAmbience";
        case SoundType::ArtilleryWhistle:   return "ArtilleryWhistle";
        case SoundType::ArtilleryExplosion: return "ArtilleryExplosion";
        case SoundType::FortifyBuild:       return "FortifyBuild";
        case SoundType::RadioChatter:       return "RadioChatter";
        case SoundType::AimDownSight:       return "AimDownSight";
        case SoundType::AimDownSightOut:    return "AimDownSightOut";
        case SoundType::ItemDrop:           return "ItemDrop";
        case SoundType::ShellCasing:        return "ShellCasing (Brass Concrete Ping)";
        case SoundType::ShellCasingDirt:    return "ShellCasingDirt (Brass Soil Thud)";
        case SoundType::HeavyBreathing:     return "HeavyBreathing";
        case SoundType::ChamberRound:       return "ChamberRound";
        case SoundType::BulletFlyby:        return "BulletFlyby (Sonic Snap)";
        case SoundType::SoldierBarkContact: return "SoldierBark (Contact Front)";
        case SoundType::SoldierBarkSuppressed: return "SoldierBark (Suppressed)";
        case SoundType::SoldierBarkWounded: return "SoldierBark (Medic Wounded)";
        case SoundType::SoldierBarkReloading: return "SoldierBark (Reloading Cover)";
        case SoundType::SoldierBarkIncoming: return "SoldierBark (Incoming Artillery)";
        case SoundType::RadioBeepDatalink:  return "RadioDatalink (Target Spotted)";
        case SoundType::HeavyMGShot:        return "HeavyMGShot (12.7mm DShK)";
        case SoundType::HelmetPing:         return "HelmetPing (Deflection)";
        case SoundType::ArmorDeflect:       return "ArmorDeflect (Body Armor Stop)";
        case SoundType::FleshImpact:        return "FleshImpact (Tissue Damage)";
        case SoundType::SprintSlide:        return "SprintSlide (Ground Friction)";
        case SoundType::GrenadePin:         return "GrenadePin (Lever Release)";
        case SoundType::GrenadeBounce:      return "GrenadeBounce (Iron Casing)";
        case SoundType::GrenadeExplosion:   return "GrenadeExplosion (HE Blast)";
        case SoundType::SoldierBarkFlanking:return "SoldierBark (Flanking Maneuver)";
        case SoundType::SoldierBarkManDown: return "SoldierBark (Man Down)";
        case SoundType::SoldierBarkFragOut: return "SoldierBark (Frag Out)";
        case SoundType::PenetrationCrack:   return "PenetrationCrack (Barrier Rupture)";
    }
    return "Unknown";
}

struct AcousticWaveEvent {
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

struct AudioCommand {
    enum class CommandType : uint8_t { Play2D, Play3D, StopAll } cmdType = CommandType::Play2D;
    SoundType soundType = SoundType::Footstep;
    float volume = 1.0f;
    float pitch = 1.0f;
    AcousticEnvironment env = AcousticEnvironment::OpenField;
    glm::vec3 soundPos{0.0f};
    glm::vec3 listenerPos{0.0f};
    float cameraYawRad = 0.0f;
    float maxDist = 180.0f;
    float occlusion = 0.0f;
};

class AudioCommandRing {
public:
    static constexpr size_t Capacity = 256;
    static constexpr size_t Mask = Capacity - 1;

    bool push(const AudioCommand& cmd) {
        size_t w = m_writeIndex.load(std::memory_order_relaxed);
        size_t r = m_readIndex.load(std::memory_order_acquire);
        if ((w - r) >= Capacity) return false;
        m_ring[w & Mask] = cmd;
        m_writeIndex.store(w + 1, std::memory_order_release);
        return true;
    }

    bool pop(AudioCommand& cmd) {
        size_t r = m_readIndex.load(std::memory_order_relaxed);
        size_t w = m_writeIndex.load(std::memory_order_acquire);
        if (r == w) return false;
        cmd = m_ring[r & Mask];
        m_readIndex.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    AudioCommand m_ring[Capacity];
    std::atomic<size_t> m_writeIndex{0};
    std::atomic<size_t> m_readIndex{0};
};

class AudioEngine {
public:
    static AudioEngine& get();

    bool init();
    void shutdown();
    void playSound(SoundType type, float volume = 1.0f, float pitch = 1.0f, AcousticEnvironment env = AcousticEnvironment::OpenField);
    void playSound3D(SoundType type, const glm::vec3& soundPos, const glm::vec3& listenerPos, float cameraYawRad, float volume = 1.0f, float maxDist = 180.0f, AcousticEnvironment env = AcousticEnvironment::OpenField, float occlusion = 0.0f);

    void setRainIntensity(float intensity) { m_rainIntensity.store(std::clamp(intensity, 0.0f, 1.0f)); }
    float getRainIntensity() const { return m_rainIntensity.load(); }

    // Volume & Dynamic Acoustic Balance Controls
    void setMasterVolume(float vol) { m_masterVolume.store(std::clamp(vol, 0.0f, 1.0f)); }
    float getMasterVolume() const { return m_masterVolume.load(); }
    void setSfxVolume(float vol) { m_sfxVolume.store(std::clamp(vol, 0.0f, 1.0f)); }
    float getSfxVolume() const { return m_sfxVolume.load(); }
    void setAmbientVolume(float vol) { m_ambientVolume.store(std::clamp(vol, 0.0f, 1.0f)); }
    float getAmbientVolume() const { return m_ambientVolume.load(); }

    // Hardware Output Device Enumeration & Dynamic Switching
    std::vector<std::string> getPlaybackDeviceNames();
    int getCurrentPlaybackDeviceIndex() const { return m_currentPlaybackDeviceIndex; }
    bool setPlaybackDevice(int deviceIndex);

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
    float getAcousticVisualSpeed() const { return m_acousticWaveVisualSpeed; }

    struct VoiceTelemetrySnapshot {
        int activeCount = 0;
        int maxVoices = 32;
        float rainIntensity = 0.0f;
        struct VoiceDetail {
            int slot = 0;
            const char* name = "";
            float progress = 0.0f;
            float duration = 0.0f;
            float volume = 0.0f;
            float pitch = 1.0f;
            float panL = 1.0f;
            float panR = 1.0f;
            float filterAlpha = 1.0f;
            bool is3D = false;
        };
        std::vector<VoiceDetail> voices;
    };

    VoiceTelemetrySnapshot getVoiceSnapshot();

private:
    AudioEngine();
    ~AudioEngine();

    struct ActiveVoice {
        SoundType type = SoundType::Footstep;
        const std::vector<float>* sampleBuffer = nullptr;
        std::vector<float> binauralLeft;
        std::vector<float> binauralRight;
        bool hasBinaural = false;
        float samplePos = 0.0f;
        float progress = 0.0f;
        float duration = 0.0f;
        float volume = 1.0f;
        float pitch = 1.0f;
        float panL = 1.0f;
        float panR = 1.0f;
        float filterAlpha = 0.85f;
        float filterStateL = 0.0f;
        float filterStateR = 0.0f;
        bool is3D = false;
        bool active = false;
        uint8_t priority = 2; // 0=Ambient, 1=Casing/Distant, 2=Footstep, 3=Gunfire, 4=Player
    };

    void* m_context = nullptr; // ma_context pointer
    void* m_device = nullptr; // ma_device pointer
    int m_currentPlaybackDeviceIndex = 0;
    std::atomic<float> m_masterVolume{1.0f};
    std::atomic<float> m_sfxVolume{0.85f};
    std::atomic<float> m_ambientVolume{0.70f};
    mutable std::mutex m_acousticWaveMutex;
    std::vector<AcousticWaveEvent> m_acousticWaves;
    uint32_t m_nextWaveId = 1;
    float m_acousticWaveVisualSpeed = 80.0f; // Configurable visual propagation speed
    glm::vec3 m_lastListenerPos{0.0f};
    float m_lastCameraYawRad = 0.0f;
    AudioCommandRing m_commandRing;
    std::vector<ActiveVoice> m_voices;
    std::mutex m_voiceMutex; // Only for external telemetry snapshot
    uint32_t m_audioSeed = 13371989u;
    inline float fastWhiteNoise() {
        m_audioSeed = m_audioSeed * 1664525u + 1013904223u;
        return static_cast<float>(static_cast<int32_t>(m_audioSeed)) * (1.0f / 2147483648.0f);
    }
    void processCommandOnAudioThread(const AudioCommand& cmd);
    bool m_initialized = false;
    std::atomic<float> m_rainIntensity{0.0f};
    float m_rainFilterL = 0.0f;
    float m_rainFilterR = 0.0f;
    float m_windFilterL = 0.0f;
    float m_windFilterR = 0.0f;
    float m_windPhase = 0.0f;

    // AAA Procedural Physical Sound Banks with Environmental Acoustic Simulation
    struct ProceduralSoundBank {
        // 1. Assault Rifle Profiles (Outdoor Open-Air, Concrete Bunker Slap, Warehouse Echo, Trench Slap)
        std::vector<std::vector<float>> rifleShotsOutdoor;
        std::vector<std::vector<float>> rifleShotsBunker;
        std::vector<std::vector<float>> rifleShotsWarehouse;
        std::vector<std::vector<float>> rifleShotsTrench;

        // 2. Heavy Sniper / Marksman Profiles
        std::vector<std::vector<float>> sniperShotsOutdoor;
        std::vector<std::vector<float>> sniperShotsBunker;
        std::vector<std::vector<float>> sniperShotsWarehouse;
        std::vector<std::vector<float>> sniperShotsTrench;

        // 3. Sidearm / Pistol Profiles
        std::vector<std::vector<float>> pistolShotsOutdoor;
        std::vector<std::vector<float>> pistolShotsBunker;
        std::vector<std::vector<float>> pistolShotsWarehouse;
        std::vector<std::vector<float>> pistolShotsTrench;

        // 4. Heavy Machine Gun (12.7mm DShK) Profiles
        std::vector<std::vector<float>> heavyMGShotsOutdoor;
        std::vector<std::vector<float>> heavyMGShotsBunker;
        std::vector<std::vector<float>> heavyMGShotsWarehouse;
        std::vector<std::vector<float>> heavyMGShotsTrench;

        // Dynamic Battlefield Props & Mechanics
        std::vector<std::vector<float>> impacts;          // 3 organic variations
        std::vector<std::vector<float>> shellCasings;     // 4 organic brass concrete/steel variations
        std::vector<std::vector<float>> shellCasingsDirt; // 4 organic dirt/grass/mud muffled variations
        std::vector<float> dryFire;
        std::vector<float> reload;
        std::vector<float> chamberRound;
        std::vector<float> bulletFlyby;
        std::vector<float> helmetPing;
        std::vector<float> armorDeflect;
        std::vector<float> fleshImpact;
        std::vector<float> sprintSlide;
        std::vector<float> grenadePin;
        std::vector<float> grenadeBounce;
        std::vector<float> grenadeExplosion;
        std::vector<float> penetrationCrack;
        std::vector<float> artilleryExplosion;

        // 5. Physical Footstep Sample Banks (4 organic variations per surface)
        std::vector<std::vector<float>> footstepsConcrete;
        std::vector<std::vector<float>> footstepsDirt;
        std::vector<std::vector<float>> footstepsWood;
        std::vector<std::vector<float>> footstepsMud;
        std::vector<std::vector<float>> footstepsWater;
        std::vector<std::vector<float>> gearRustles;
    } m_soundBank;

    const std::vector<float>* selectRifleBuffer(AcousticEnvironment env, size_t variationIdx);
    const std::vector<float>* selectSniperBuffer(AcousticEnvironment env, size_t variationIdx);
    const std::vector<float>* selectPistolBuffer(AcousticEnvironment env, size_t variationIdx);
    const std::vector<float>* selectHmgBuffer(AcousticEnvironment env, size_t variationIdx);
    const std::vector<float>* selectFootstepBuffer(SoundType type, size_t variationIdx);

    size_t m_rifleVariationCounter = 0;
    size_t m_hmgVariationCounter = 0;
    size_t m_sniperVariationCounter = 0;
    size_t m_pistolVariationCounter = 0;
    size_t m_impactVariationCounter = 0;
    size_t m_casingVariationCounter = 0;
    size_t m_footstepVariationCounter = 0;

    void generateProceduralSoundBank();

    static void audioCallback(struct ma_device* pDevice, void* pOutput, const void* pInput, unsigned int frameCount);
};

} // namespace Mercenary
