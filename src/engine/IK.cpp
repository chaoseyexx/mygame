#include "IK.hpp"

namespace Mercenary {

glm::quat InverseKinematics::lookAt(const glm::vec3& origin, const glm::vec3& target, const glm::vec3& up) {
    glm::vec3 diff = target - origin;
    float len = glm::length(diff);
    if (len < 0.0001f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    glm::vec3 forward = diff / len;

    glm::vec3 right = glm::cross(up, forward);
    if (glm::length(right) < 0.0001f) {
        right = glm::cross(glm::vec3(0, 0, 1), forward);
    }
    right = glm::normalize(right);
    glm::vec3 realUp = glm::cross(forward, right);

    glm::mat3 rotMat;
    rotMat[0] = right;
    rotMat[1] = realUp;
    rotMat[2] = forward;

    return glm::toQuat(rotMat);
}

bool InverseKinematics::solveTwoBoneIK(
    const glm::vec3& rootPos,
    const glm::vec3& targetPos,
    const glm::vec3& poleVector,
    float upperLength,
    float lowerLength,
    glm::vec3& outMidPos,
    glm::quat& outUpperRot,
    glm::quat& outLowerRot
) {
    glm::vec3 toTarget = targetPos - rootPos;
    float dist = glm::length(toTarget);
    if (dist < 0.0001f) {
        outMidPos = rootPos + glm::vec3(0, -upperLength, 0);
        return false;
    }

    float maxDist = (upperLength + lowerLength) * 0.999f;
    float minDist = std::abs(upperLength - lowerLength) * 1.001f;
    float clampedDist = std::clamp(dist, minDist, maxDist);

    glm::vec3 dir = toTarget / dist;

    // Law of Cosines for triangle angles
    float cosAlpha = (upperLength * upperLength + clampedDist * clampedDist - lowerLength * lowerLength) /
                     (2.0f * upperLength * clampedDist);
    cosAlpha = std::clamp(cosAlpha, -1.0f, 1.0f);
    float alpha = std::acos(cosAlpha);

    // Plane normal defined by root-to-target and pole vector
    glm::vec3 toPole = poleVector - rootPos;
    glm::vec3 planeNormal = glm::cross(dir, toPole);
    if (glm::length(planeNormal) < 0.001f) {
        planeNormal = glm::cross(dir, glm::vec3(0, 1, 0));
        if (glm::length(planeNormal) < 0.001f) {
            planeNormal = glm::cross(dir, glm::vec3(1, 0, 0));
        }
    }
    planeNormal = glm::normalize(planeNormal);

    // Vector in bend plane perpendicular to dir pointing towards pole
    glm::vec3 bendDir = glm::normalize(glm::cross(planeNormal, dir));

    // Calculate mid joint (elbow/knee) position
    float along = upperLength * cosAlpha;
    float outDist = upperLength * std::sin(alpha);
    outMidPos = rootPos + dir * along + bendDir * outDist;

    // Orient upper bone towards mid, and lower bone towards target
    outUpperRot = lookAt(rootPos, outMidPos, planeNormal);
    outLowerRot = lookAt(outMidPos, targetPos, planeNormal);

    return true;
}

} // namespace Mercenary
