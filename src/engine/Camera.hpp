#pragma once
#include "Types.hpp"
#include <algorithm>

namespace Mercenary {

class World;

enum class CameraMode {
    ThirdPerson,       // Modern over-the-shoulder tactical TPS
    Orbit,             // Centered orbit inspection (Main Menu, Character Select, Customizer)
    TopDownTactical,   // High tactical overview
    FreeOrbit = Orbit  // Backward compatibility alias
};

class Camera {
public:
    Camera();

    void update(float dt, const glm::vec3& targetPos, const glm::vec2& mouseScreenPos, int screenW, int screenH, bool isAiming = false, int adsZoomLevel = 0, bool isSprinting = false, float moveSpeed = 0.0f, const World* world = nullptr, float eyeHeight = 1.55f, float strafeVelocity = 0.0f);
    void handleScroll(float scrollDelta, bool invertScroll = false);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    glm::mat4 getViewProjectionMatrix(float aspectRatio) const;

    glm::vec3 getForwardVector() const;
    glm::vec3 getRightVector() const;
    glm::vec3 getUpVector() const;

    // Raycast from 2D screen coordinate to 3D world plane at height Y
    glm::vec3 screenToWorldGround(const glm::vec2& screenPos, int screenW, int screenH, float groundY = 0.0f) const;
    void getRay(const glm::vec2& screenPos, int screenW, int screenH, glm::vec3& outOrigin, glm::vec3& outDir) const;

    // Project 3D world coordinate to 2D screen coordinate (returns false if behind camera)
    bool worldToScreen(const glm::vec3& worldPos, float aspectRatio, float screenW, float screenH, glm::vec2& outScreenPos) const;

    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getTarget() const { return m_target; }
    float getZoom() const { return m_zoom; }
    float getTargetZoom() const { return m_targetZoom; }

    void setPitch(float pitchDegrees) { m_pitch = pitchDegrees; m_targetPitch = pitchDegrees; }
    void setYaw(float yawDegrees) { m_yaw = yawDegrees; m_targetYaw = yawDegrees; }
    void setTargetPitch(float pitchDegrees) { m_targetPitch = std::clamp(pitchDegrees, -65.0f, 75.0f); }
    void setTargetYaw(float yawDegrees) { m_targetYaw = yawDegrees; }
    float getPitch() const { return m_pitch; }
    float getYaw() const { return m_yaw; }
    float getTargetPitch() const { return m_targetPitch; }
    float getTargetYaw() const { return m_targetYaw; }

    void rotateYaw(float deltaDegrees) {
        m_targetYaw += deltaDegrees;
        while (m_targetYaw >= 360.0f) m_targetYaw -= 360.0f;
        while (m_targetYaw < 0.0f) m_targetYaw += 360.0f;
    }
    void rotatePitch(float deltaDegrees) {
        m_targetPitch = std::clamp(m_targetPitch + deltaDegrees, -65.0f, 75.0f);
    }

    void setZoom(float zoom) {
        m_zoom = std::clamp(zoom, m_minZoom, m_maxZoom);
        m_targetZoom = m_zoom;
    }
    void setTargetZoom(float zoom) {
        m_targetZoom = std::clamp(zoom, m_minZoom, m_maxZoom);
    }

    void setFOV(float fov) { m_baseFov = std::clamp(fov, 20.0f, 110.0f); }
    void setBaseFOV(float fov) { m_baseFov = std::clamp(fov, 20.0f, 110.0f); }
    float getFOV() const { return m_fov; }
    float getBaseFOV() const { return m_baseFov; }

    void setMode(CameraMode mode);
    CameraMode getMode() const { return m_mode; }

    void snapToTarget(const glm::vec3& target) {
        m_currentLookAt = target + glm::vec3(0.0f, 1.55f, 0.0f);
        m_target = target;
        m_currentDist = m_zoom;
    }

    void resetToThirdPerson();
    void resetToOrbit();
    void resetToIsometric() { resetToThirdPerson(); }

    void toggleShoulder() { m_shoulderRight = !m_shoulderRight; }
    bool isShoulderRight() const { return m_shoulderRight; }
    void setShoulderRight(bool right) { m_shoulderRight = right; }

    void addTrauma(float trauma) {
        m_trauma = std::min(m_trauma + trauma, 1.0f);
    }
    float getTrauma() const { return m_trauma; }

private:
    glm::vec3 m_position{0.0f, 2.0f, -2.5f};
    glm::vec3 m_target{0.0f, 1.5f, 50.0f};
    glm::vec3 m_currentLookAt{0.0f, 1.5f, 0.0f};

    CameraMode m_mode = CameraMode::ThirdPerson;
    float m_pitch = 6.0f;        // Gentle eye-level horizon framing
    float m_targetPitch = 6.0f;
    float m_yaw = 180.0f;        // Facing forward downrange
    float m_targetYaw = 180.0f;
    float m_fov = 78.0f;         // Tactical TPS field of view
    float m_baseFov = 78.0f;     // User base setting field of view

    float m_zoom = 2.4f;         // Follow distance (meters)
    float m_targetZoom = 2.4f;
    float m_currentDist = 2.4f;  // Smooth damped follow distance
    float m_minZoom = 1.2f;      // Tight close-quarters zoom
    float m_maxZoom = 4.8f;      // Extended wide TPS view

    bool m_shoulderRight = true; // Right vs left shoulder peek
    float m_currentShoulderX = 0.44f;
    float m_currentShoulderY = 0.10f;

    float m_followSpeed = 16.0f; // Responsive character tracking
    float m_trauma = 0.0f;
    float m_shakeTimer = 0.0f;
    float m_sprintBobTimer = 0.0f;
    float m_currentEyeHeight = 1.55f;
    float m_currentRoll = 0.0f;
};

} // namespace Mercenary
