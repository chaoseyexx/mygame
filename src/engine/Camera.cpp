#include "Camera.hpp"
#include "game/World.hpp"
#include <algorithm>
#include <cmath>

namespace Mercenary {

Camera::Camera() {
    resetToOrbit();
}

void Camera::resetToOrbit() {
    m_mode = CameraMode::Orbit;
    m_pitch = 5.0f;
    m_targetPitch = 5.0f;
    m_yaw = 0.0f;
    m_targetYaw = 0.0f;
    m_fov = 44.0f;
    m_zoom = 2.85f;
    m_targetZoom = 2.85f;
    m_minZoom = 1.0f;
    m_maxZoom = 5.0f;
    m_currentShoulderX = 0.0f;
    m_currentShoulderY = 0.0f;
}

void Camera::resetToThirdPerson() {
    m_mode = CameraMode::ThirdPerson;
    m_pitch = 6.0f;
    m_targetPitch = 6.0f;
    m_yaw = 180.0f;
    m_targetYaw = 180.0f;
    m_baseFov = 78.0f;
    m_fov = 78.0f;
    m_zoom = 2.4f;
    m_targetZoom = 2.4f;
    m_currentDist = 2.4f;
    m_minZoom = 1.2f;
    m_maxZoom = 4.8f;
    m_shoulderRight = true;
    m_currentShoulderX = 0.44f;
    m_currentShoulderY = 0.10f;
}

void Camera::handleScroll(float scrollDelta, bool invertScroll) {
    if (std::abs(scrollDelta) > 0.001f) {
        float factor = invertScroll ? -0.35f : 0.35f;
        m_targetZoom -= scrollDelta * factor;
        m_targetZoom = std::clamp(m_targetZoom, m_minZoom, m_maxZoom);
    }
}

void Camera::setMode(CameraMode mode) {
    if (m_mode == mode) return;
    m_mode = mode;
    switch (mode) {
    case CameraMode::ThirdPerson:
        resetToThirdPerson();
        break;
    case CameraMode::Orbit:
        resetToOrbit();
        break;
    case CameraMode::TopDownTactical:
        m_targetPitch = 75.0f;
        m_targetYaw = 0.0f;
        m_fov = 60.0f;
        m_targetZoom = 15.0f;
        break;
    }
}

glm::vec3 Camera::getForwardVector() const {
    float yawRad = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);
    glm::vec3 camForward;
    camForward.x = -std::sin(yawRad) * std::cos(pitchRad);
    camForward.y = -std::sin(pitchRad);
    camForward.z = -std::cos(yawRad) * std::cos(pitchRad);
    return glm::normalize(camForward);
}

glm::vec3 Camera::getRightVector() const {
    float yawRad = glm::radians(m_yaw);
    return glm::normalize(glm::vec3(std::cos(yawRad), 0.0f, -std::sin(yawRad)));
}

glm::vec3 Camera::getUpVector() const {
    return glm::normalize(glm::cross(getRightVector(), getForwardVector()));
}

void Camera::update(float dt, const glm::vec3& targetPos, const glm::vec2& mouseScreenPos, int screenW, int screenH, bool isAiming, int adsZoomLevel, bool isSprinting, float moveSpeed, const World* world, float eyeHeight, float strafeVelocity, bool inCover, float coverLean) {
    (void)mouseScreenPos;
    (void)screenW;
    (void)screenH;

    // 1. Smoothly interpolate zoom
    m_zoom = glm::mix(m_zoom, m_targetZoom, std::clamp(dt * 12.0f, 0.0f, 1.0f));

    // Dynamic Camera Banking & Roll (Athletic tactical strafe and turn inertia)
    float targetRoll = std::clamp(-strafeVelocity * 0.35f, -2.2f, 2.2f);
    float turnYawRate = (m_targetYaw - m_yaw);
    targetRoll += std::clamp(turnYawRate * 0.035f, -1.8f, 1.8f);
    m_currentRoll = glm::mix(m_currentRoll, targetRoll, std::clamp(dt * 12.0f, 0.0f, 1.0f));

    // Dynamic operator stance eye-height follow (Standing 1.55m -> Crouch 1.05m -> Prone 0.38m)
    m_currentEyeHeight = glm::mix(m_currentEyeHeight, eyeHeight, std::clamp(dt * 11.0f, 0.0f, 1.0f));

    // 2. Smoothly interpolate pitch & yaw towards target with shortest-arc angular wrapping
    float diffYaw = m_targetYaw - m_yaw;
    while (diffYaw > 180.0f) diffYaw -= 360.0f;
    while (diffYaw < -180.0f) diffYaw += 360.0f;
    m_yaw += diffYaw * std::clamp(dt * 30.0f, 0.0f, 1.0f);
    while (m_yaw >= 360.0f) m_yaw -= 360.0f;
    while (m_yaw < 0.0f) m_yaw += 360.0f;

    m_pitch = glm::mix(m_pitch, m_targetPitch, std::clamp(dt * 30.0f, 0.0f, 1.0f));
    m_pitch = std::clamp(m_pitch, -65.0f, 75.0f);

    if (m_mode == CameraMode::Orbit) {
        float targetFOV = 44.0f;
        m_fov = glm::mix(m_fov, targetFOV, std::clamp(dt * 12.0f, 0.0f, 1.0f));

        m_currentLookAt = glm::mix(m_currentLookAt, targetPos, std::clamp(dt * m_followSpeed, 0.0f, 1.0f));

        float pitchRad = glm::radians(m_pitch);
        float yawRad = glm::radians(m_yaw);

        glm::vec3 offset;
        offset.x = m_zoom * std::cos(pitchRad) * std::sin(yawRad);
        offset.y = m_zoom * std::sin(pitchRad);
        offset.z = m_zoom * std::cos(pitchRad) * std::cos(yawRad);

        m_position = m_currentLookAt + offset;
        m_target = m_currentLookAt;
        return;
    }

    // 3. Dynamic FOV, Distance & Shoulder Framing for Modern Tactical TPS
    float targetFOV = m_baseFov;
    float targetDist = m_zoom;
    float targetShoulderX = m_shoulderRight ? 0.44f : -0.44f;
    float targetShoulderY = 0.10f;

    if (isSprinting && !isAiming && moveSpeed > 3.0f) {
        targetFOV = m_baseFov + 10.0f;     // Athletic sprint wide tactical perspective
        targetDist += 0.35f;
    }

    // Tactical Cover-Aware Camera Framing
    if (inCover) {
        targetDist = isAiming ? 1.45f : 1.90f; // Closer over-the-shoulder tactical view
        targetShoulderY += 0.04f;             // Slightly higher to survey over barriers
        if (std::abs(coverLean) > 0.001f) {
            targetShoulderX += coverLean * 0.42f; // Shift shoulder outward toward exposed corner
        }
    }

    if (eyeHeight < 0.50f) {
        // Organic prone stance camera framing
        targetDist += 0.35f;
        targetShoulderY -= 0.06f;
    } else if (eyeHeight < 1.20f) {
        // Organic crouch stance camera framing
        targetShoulderY -= 0.03f;
    }

    if (isAiming) {
        if (adsZoomLevel == 2) {
            targetFOV = 28.0f; // 4x/6x Precision Scope: high magnification
            targetDist = 0.95f;
            targetShoulderX *= 0.65f;
            targetShoulderY += 0.05f;
        } else if (adsZoomLevel == 1) {
            targetFOV = 48.0f; // 2.5x Combat ACOG: medium combat zoom
            targetDist = 1.25f;
            targetShoulderX *= 0.72f;
            targetShoulderY += 0.04f;
        } else {
            targetFOV = 66.0f; // 1.0x CQB Reflex: snappy close-in over shoulder
            targetDist = 1.45f;
            targetShoulderX *= 0.80f;
            targetShoulderY += 0.02f;
        }
    }

    m_currentShoulderX = glm::mix(m_currentShoulderX, targetShoulderX, std::clamp(dt * 14.0f, 0.0f, 1.0f));
    m_currentShoulderY = glm::mix(m_currentShoulderY, targetShoulderY, std::clamp(dt * 14.0f, 0.0f, 1.0f));

    float fovSpeed = isAiming ? 18.0f : 10.0f;
    m_fov = glm::mix(m_fov, targetFOV, std::clamp(dt * fovSpeed, 0.0f, 1.0f));

    // Damped smooth follow distance (eliminates sudden sprint distance snapping)
    m_currentDist = glm::mix(m_currentDist, targetDist, std::clamp(dt * 8.0f, 0.0f, 1.0f));

    // 4. Orientation vectors
    glm::vec3 camForward = getForwardVector();
    glm::vec3 camRight = getRightVector();

    // 5. Pivot point dynamically tracking operator upper torso / head across stances
    glm::vec3 pivot = targetPos + glm::vec3(0.0f, m_currentEyeHeight, 0.0f);
    m_currentLookAt = glm::mix(m_currentLookAt, pivot, std::clamp(dt * m_followSpeed, 0.0f, 1.0f));

    glm::vec3 shoulderPos = m_currentLookAt + camRight * m_currentShoulderX + glm::vec3(0.0f, m_currentShoulderY, 0.0f);

    // 6. Camera Position along backwards line-of-sight using smoothed distance
    glm::vec3 desiredCamPos = shoulderPos - camForward * m_currentDist;

    // 6b. Spring-Arm Obstacle Raycast: Prevent clipping through solid walls/buildings
    if (world) {
        glm::vec3 hitPoint(0.0f), hitNormal(0.0f);
        WorldObstacle* hitObs = nullptr;
        if (const_cast<World*>(world)->raycast(shoulderPos, desiredCamPos, hitPoint, hitNormal, hitObs)) {
            float hitDist = glm::distance(shoulderPos, hitPoint);
            float safeDist = std::max(0.30f, hitDist - 0.20f);
            desiredCamPos = shoulderPos - camForward * safeDist;
        }
    }

    // 7. Ground / Terrain collision buffer (Spring-arm compression prevents vertical pops)
    float groundLevel = World::sampleTerrainHeight(desiredCamPos.x, desiredCamPos.z) + 0.35f;
    if (desiredCamPos.y < groundLevel) {
        float penetration = groundLevel - desiredCamPos.y;
        desiredCamPos += camForward * (penetration * 1.25f);
        float newGround = World::sampleTerrainHeight(desiredCamPos.x, desiredCamPos.z) + 0.35f;
        if (desiredCamPos.y < newGround) {
            desiredCamPos.y = newGround;
        }
    }

    // 8. Screen shake trauma & athletic movement head bob
    glm::vec3 shakeOffset(0.0f);
    if (m_trauma > 0.001f) {
        m_shakeTimer += dt;
        float shakeMag = m_trauma * m_trauma;
        shakeOffset.x = (std::sin(m_shakeTimer * 52.0f) * 0.35f + std::sin(m_shakeTimer * 31.0f) * 0.20f) * shakeMag;
        shakeOffset.y = (std::cos(m_shakeTimer * 44.0f) * 0.25f) * shakeMag;
        shakeOffset.z = (std::cos(m_shakeTimer * 46.0f) * 0.35f + std::cos(m_shakeTimer * 27.0f) * 0.20f) * shakeMag;
        m_trauma = std::max(0.0f, m_trauma - dt * 1.8f);
    }

    if (moveSpeed > 0.6f && !isAiming) {
        m_sprintBobTimer += dt * (isSprinting ? 12.8f : 7.4f);
        float bobIntensity = isSprinting ? 0.024f : 0.010f;
        float bobY = -std::abs(std::sin(m_sprintBobTimer)) * bobIntensity;
        float bobSide = std::sin(m_sprintBobTimer * 0.5f) * (bobIntensity * 0.55f);
        shakeOffset += camRight * bobSide + glm::vec3(0.0f, bobY, 0.0f);
    }

    m_position = desiredCamPos + shakeOffset;
    m_target = m_position + camForward * 50.0f;
}

glm::mat4 Camera::getViewMatrix() const {
    if (m_mode == CameraMode::Orbit) {
        return glm::lookAt(m_position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    glm::vec3 camForward = getForwardVector();
    glm::vec3 baseUp = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(m_currentRoll) > 0.01f) {
        glm::mat4 rollMat = glm::rotate(glm::mat4(1.0f), glm::radians(m_currentRoll), camForward);
        baseUp = glm::vec3(rollMat * glm::vec4(baseUp, 0.0f));
    }
    return glm::lookAt(m_position, m_position + camForward, baseUp);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, 0.15f, 2000.0f);
}

glm::mat4 Camera::getViewProjectionMatrix(float aspectRatio) const {
    return getProjectionMatrix(aspectRatio) * getViewMatrix();
}

void Camera::getRay(const glm::vec2& screenPos, int screenW, int screenH, glm::vec3& outOrigin, glm::vec3& outDir) const {
    float ndcX = (2.0f * screenPos.x) / (float)screenW - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y) / (float)screenH;
    float aspect = (float)screenW / (float)screenH;
    glm::mat4 invVP = glm::inverse(getViewProjectionMatrix(aspect));
    glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    if (nearPoint.w != 0.0f) nearPoint /= nearPoint.w;
    if (farPoint.w != 0.0f)  farPoint /= farPoint.w;
    outOrigin = glm::vec3(nearPoint);
    outDir = glm::normalize(glm::vec3(farPoint) - outOrigin);
}

glm::vec3 Camera::screenToWorldGround(const glm::vec2& screenPos, int screenW, int screenH, float groundY) const {
    glm::vec3 rayOrigin, rayDir;
    getRay(screenPos, screenW, screenH, rayOrigin, rayDir);
    if (std::abs(rayDir.y) > 0.001f) {
        float t = (groundY - rayOrigin.y) / rayDir.y;
        if (t > 0.0f && t < 350.0f) {
            return rayOrigin + rayDir * t;
        }
    }
    return rayOrigin + rayDir * 80.0f;
}

bool Camera::worldToScreen(const glm::vec3& worldPos, float aspectRatio, float screenW, float screenH, glm::vec2& outScreenPos) const {
    glm::mat4 vp = getViewProjectionMatrix(aspectRatio);
    glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.05f) {
        return false;
    }
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) {
        return false;
    }
    outScreenPos.x = (ndc.x * 0.5f + 0.5f) * screenW;
    outScreenPos.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * screenH;
    return true;
}

} // namespace Mercenary
