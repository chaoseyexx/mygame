with open('src/game/Player.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# Target 1: Replace section 1 (Body Heading Yaw) with input sampling + decoupled idle deadzone
target_yaw = """    // 1. Calculate Body Heading Yaw (Hugs wall in cover unless aiming)
    if (m_inCover && !m_isAiming) {
        float targetYaw = std::atan2(m_coverNormal.x, m_coverNormal.z);
        if (glm::length(m_velocity) > 0.2f) {
            float wallMoveDot = glm::dot(m_velocity, m_coverWallDir);
            glm::vec3 angledHeading = glm::normalize(m_coverNormal * 0.70f + m_coverWallDir * (wallMoveDot > 0.0f ? 0.35f : -0.35f));
            targetYaw = std::atan2(angledHeading.x, angledHeading.z);
        }
        float deltaYaw = targetYaw - m_yaw;
        while (deltaYaw > 3.14159265f) deltaYaw -= 2.0f * 3.14159265f;
        while (deltaYaw < -3.14159265f) deltaYaw += 2.0f * 3.14159265f;
        m_yaw += deltaYaw * std::clamp(dt * 20.0f, 0.0f, 1.0f);
    } else {
        float targetYaw = m_yaw;
        if (m_stats.isSprinting && glm::length(m_velocity) > 2.5f) {
            targetYaw = std::atan2(m_velocity.x, m_velocity.z);
        } else {
            float cRad = glm::radians(cameraYaw);
            glm::vec3 camFwd(-std::sin(cRad), 0.0f, -std::cos(cRad));
            targetYaw = std::atan2(camFwd.x, camFwd.z);
        }
        float deltaYaw = targetYaw - m_yaw;
        while (deltaYaw > 3.14159265f) deltaYaw -= 2.0f * 3.14159265f;
        while (deltaYaw < -3.14159265f) deltaYaw += 2.0f * 3.14159265f;
        m_yaw += deltaYaw * std::clamp(dt * 26.0f, 0.0f, 1.0f);
    }
    while (m_yaw > 3.14159265f) m_yaw -= 6.2831853f;
    while (m_yaw < -3.14159265f) m_yaw += 6.2831853f;"""

repl_yaw = """    // Pre-sample movement & weapon inputs for decoupled idle camera free-look
    glm::vec3 inputDir(0.0f);
    if (!captureKb) {
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveForward)) || window.isKeyPressed(GLFW_KEY_W)) inputDir.z -= 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveBackward)) || window.isKeyPressed(GLFW_KEY_S)) inputDir.z += 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveLeft)) || window.isKeyPressed(GLFW_KEY_A)) inputDir.x -= 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveRight)) || window.isKeyPressed(GLFW_KEY_D)) inputDir.x += 1.0f;
    }
    bool isMoving = glm::length(inputDir) > 0.01f;
    bool aimInput = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) && !uiWantsMouse;
    bool leftClickFire = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !uiWantsMouse;

    // 1. Calculate Body Heading Yaw (Hugs wall in cover unless aiming)
    if (m_inCover && !m_isAiming) {
        float targetYaw = std::atan2(m_coverNormal.x, m_coverNormal.z);
        if (glm::length(m_velocity) > 0.2f) {
            float wallMoveDot = glm::dot(m_velocity, m_coverWallDir);
            glm::vec3 angledHeading = glm::normalize(m_coverNormal * 0.70f + m_coverWallDir * (wallMoveDot > 0.0f ? 0.35f : -0.35f));
            targetYaw = std::atan2(angledHeading.x, angledHeading.z);
        }
        float deltaYaw = targetYaw - m_yaw;
        while (deltaYaw > 3.14159265f) deltaYaw -= 2.0f * 3.14159265f;
        while (deltaYaw < -3.14159265f) deltaYaw += 2.0f * 3.14159265f;
        m_yaw += deltaYaw * std::clamp(dt * 20.0f, 0.0f, 1.0f);
    } else {
        float cRad = glm::radians(cameraYaw);
        glm::vec3 camFwd(-std::sin(cRad), 0.0f, -std::cos(cRad));
        float camLookYaw = std::atan2(camFwd.x, camFwd.z);

        if (m_isAiming || aimInput || leftClickFire) {
            // Actively aiming or shooting: operator squares up directly with camera crosshair
            float deltaYaw = camLookYaw - m_yaw;
            while (deltaYaw > 3.14159265f) deltaYaw -= 2.0f * 3.14159265f;
            while (deltaYaw < -3.14159265f) deltaYaw += 2.0f * 3.14159265f;
            m_yaw += deltaYaw * std::clamp(dt * 26.0f, 0.0f, 1.0f);
        } else if (isMoving) {
            // Moving / traversing: body faces velocity if sprinting, or camera forward
            float targetYaw = (m_stats.isSprinting && glm::length(m_velocity) > 2.5f)
                ? std::atan2(m_velocity.x, m_velocity.z)
                : camLookYaw;
            float deltaYaw = targetYaw - m_yaw;
            while (deltaYaw > 3.14159265f) deltaYaw -= 2.0f * 3.14159265f;
            while (deltaYaw < -3.14159265f) deltaYaw += 2.0f * 3.14159265f;
            m_yaw += deltaYaw * std::clamp(dt * 22.0f, 0.0f, 1.0f);
        } else {
            // Standing idle in Low-Ready: Weapon & body stay steady!
            // Free-look camera: operator only steps/turns if camera rotates beyond 68 degrees deadzone
            float diffCam = camLookYaw - m_yaw;
            while (diffCam > 3.14159265f) diffCam -= 2.0f * 3.14159265f;
            while (diffCam < -3.14159265f) diffCam += 2.0f * 3.14159265f;
            constexpr float IDLE_DEADZONE = 1.1868f; // 68 degrees
            if (std::abs(diffCam) > IDLE_DEADZONE) {
                float sign = (diffCam > 0.0f) ? 1.0f : -1.0f;
                float excess = diffCam - sign * IDLE_DEADZONE;
                m_yaw += excess * std::clamp(dt * 10.0f, 0.0f, 1.0f);
            }
        }
    }
    while (m_yaw > 3.14159265f) m_yaw -= 6.2831853f;
    while (m_yaw < -3.14159265f) m_yaw += 6.2831853f;"""

assert target_yaw in code, "target_yaw not found in Player.cpp"
code = code.replace(target_yaw, repl_yaw, 1)

# Target 2: At line 759, change `bool aimInput = ...` to `aimInput = ...`
target_aimInput = "    bool aimInput = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) && !uiWantsMouse;"
repl_aimInput = "    aimInput = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) && !uiWantsMouse;"
assert target_aimInput in code, "target_aimInput not found in Player.cpp"
code = code.replace(target_aimInput, repl_aimInput, 1)

# Target 3: At line 934, reuse existing `inputDir` and `isMoving`
target_move = """    // 3. Movement calculation (Camera-Relative Direction)
    glm::vec3 inputDir(0.0f);
    if (!captureKb) {
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveForward)) || window.isKeyPressed(GLFW_KEY_W)) inputDir.z -= 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveBackward)) || window.isKeyPressed(GLFW_KEY_S)) inputDir.z += 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveLeft)) || window.isKeyPressed(GLFW_KEY_A)) inputDir.x -= 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveRight)) || window.isKeyPressed(GLFW_KEY_D)) inputDir.x += 1.0f;
    }

    bool isMoving = glm::length(inputDir) > 0.01f;"""

repl_move = """    // 3. Movement calculation (Camera-Relative Direction)
    // (inputDir and isMoving were pre-sampled above for decoupled idle heading)"""

assert target_move in code, "target_move not found in Player.cpp"
code = code.replace(target_move, repl_move, 1)

# Target 4: At line 1005, change `bool leftClickFire = ...` to `leftClickFire = ...`
target_fire = "    bool leftClickFire = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !uiWantsMouse;"
repl_fire = "    leftClickFire = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !uiWantsMouse;"
assert target_fire in code, "target_fire not found in Player.cpp"
code = code.replace(target_fire, repl_fire, 1)

with open('src/game/Player.cpp', 'w', encoding='utf-8', newline='') as f:
    f.write(code)
print("Successfully applied steady idle gun and decoupled camera free-look in Player.cpp")
