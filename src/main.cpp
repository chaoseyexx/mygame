#include "game/GameApp.hpp"
#include "game/World.hpp"
#include <iostream>
#include <string>
#include <cassert>
#ifdef _WIN32
#include <windows.h>
#endif

static int runCollisionTests() {
    std::cout << "========================================\n";
    std::cout << "  MERCENARY COLLISION AND APERTURE SUITE\n";
    std::cout << "========================================\n";

    Mercenary::World world;
    
    // Add RuinedWall at origin
    Mercenary::WorldObstacle wall;
    wall.type = Mercenary::ObstacleType::RuinedWall;
    wall.position = glm::vec3(0.0f, 0.0f, 0.0f);
    wall.size = glm::vec3(5.0f, 2.8f, 0.60f);
    wall.yaw = 0.0f;
    wall.pitch = 0.0f;
    wall.roll = 0.0f;
    wall.blocksLineOfSight = true;
    wall.destructible = false;
    wall.health = 500.0f;
    world.getObstacles().push_back(wall);

    // Add Calibrated FreightBoxcar
    Mercenary::WorldObstacle boxcar;
    boxcar.type = Mercenary::ObstacleType::FreightBoxcar;
    boxcar.position = glm::vec3(50.0f, 0.0f, 0.0f);
    boxcar.size = glm::vec3(13.50f, 3.40f, 3.10f);
    boxcar.yaw = 0.0f;
    boxcar.blocksLineOfSight = true;
    world.getObstacles().push_back(boxcar);

    // Add Low Curb / Pallet step
    Mercenary::WorldObstacle curb;
    curb.type = Mercenary::ObstacleType::CargoPallet;
    curb.position = glm::vec3(100.0f, 0.0f, 0.0f);
    curb.size = glm::vec3(2.0f, 0.25f, 1.0f);
    curb.yaw = 0.0f;
    curb.blocksLineOfSight = true;
    world.getObstacles().push_back(curb);

    glm::vec3 hitPoint, hitNormal;
    Mercenary::WorldObstacle* hitObs = nullptr;

    // Test 1: Raycast through RuinedWall window aperture (X=0.0, Y=1.70, from Z=5 to Z=-5)
    bool hitWindow = world.raycast(glm::vec3(0.0f, 1.70f, 5.0f), glm::vec3(0.0f, 1.70f, -5.0f), hitPoint, hitNormal, hitObs);
    std::cout << "[TEST 1] Window Aperture Raycast: " << (!hitWindow ? "PASS (Clear LOS through window aperture)" : "FAIL") << "\n";
    assert(!hitWindow && "Window aperture must allow bullets and LOS to pass without collision!");

    // Test 2: Raycast striking RuinedWall left pillar (X=-2.0, Y=1.70, from Z=5 to Z=-5)
    bool hitPillar = world.raycast(glm::vec3(-2.0f, 1.70f, 5.0f), glm::vec3(-2.0f, 1.70f, -5.0f), hitPoint, hitNormal, hitObs);
    std::cout << "[TEST 2] Left Brick Pillar Raycast: " << (hitPillar ? "PASS (Solid pillar impact detected)" : "FAIL") << "\n";
    assert(hitPillar && "Left pillar must block raycast!");

    // Test 3: Raycast striking RuinedWall lower sill (X=0.0, Y=0.60, from Z=5 to Z=-5)
    bool hitSill = world.raycast(glm::vec3(0.0f, 0.60f, 5.0f), glm::vec3(0.0f, 0.60f, -5.0f), hitPoint, hitNormal, hitObs);
    std::cout << "[TEST 3] Window Sill Raycast: " << (hitSill ? "PASS (Solid sill impact detected)" : "FAIL") << "\n";
    assert(hitSill && "Window sill must block raycast!");

    // Test 4: Raycast striking RuinedWall upper lintel (X=0.0, Y=2.40, from Z=5 to Z=-5)
    bool hitLintel = world.raycast(glm::vec3(0.0f, 2.40f, 5.0f), glm::vec3(0.0f, 2.40f, -5.0f), hitPoint, hitNormal, hitObs);
    std::cout << "[TEST 4] Upper Lintel Beam Raycast: " << (hitLintel ? "PASS (Solid lintel impact detected)" : "FAIL") << "\n";
    assert(hitLintel && "Upper lintel must block raycast!");

    // Test 5: Calibrated FreightBoxcar edge raycast at X=55.5 (5.5m from center, old 8.5m box missed this!)
    bool hitBoxcar = world.raycast(glm::vec3(55.5f, 1.5f, 5.0f), glm::vec3(55.5f, 1.5f, -5.0f), hitPoint, hitNormal, hitObs);
    std::cout << "[TEST 5] Calibrated 13.5m Boxcar Outer Extent: " << (hitBoxcar ? "PASS (Calibrated collision active)" : "FAIL") << "\n";
    assert(hitBoxcar && "Calibrated 13.5m boxcar must register hit at 5.5m!");

    // Test 6: 35cm Step-over mantling verification
    glm::vec3 playerPos(100.0f, 0.0f, 0.40f);
    world.checkCollision(playerPos, 0.35f);
    bool stepOverPassed = (playerPos.y >= 0.24f);
    std::cout << "[TEST 6] 35cm Step-over Mantling: " << (stepOverPassed ? "PASS (Player smoothly stepped up onto curb)" : "FAIL") << " (pos.y = " << playerPos.y << ")\n";
    assert(stepOverPassed && "Player must step up onto low obstacle!");

    std::cout << "========================================\n";
    std::cout << "  ALL 6 COLLISION AUDIT TESTS PASSED!   \n";
    std::cout << "========================================\n";
    return 0;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Enable Per-Monitor V2 High-DPI Awareness dynamically to eliminate DWM bitmap virtualization
    // and ensure Discord / OBS desktop stream captures the OpenGL window seamlessly
    typedef BOOL (WINAPI *PFN_SetProcessDpiAwarenessContext)(void*);
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        auto pfn = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pfn) {
            pfn((void*)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        }
    }
#endif
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test-collision") {
            return runCollisionTests();
        }
    }
#ifdef _WIN32
    // If launched from an existing terminal, attach stdout; only spawn a console if explicitly requested
    bool requestConsole = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--console" || arg == "-c") {
            requestConsole = true;
            break;
        }
    }
    if (requestConsole) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            AllocConsole();
        }
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
    } else {
        FILE* fp = nullptr;
        freopen_s(&fp, "mercenary_client.log", "w", stdout);
        freopen_s(&fp, "mercenary_client.log", "a", stderr);
    }
#endif

    Mercenary::GameApp app;
    if (!app.init(argc, argv)) {
        return -1;
    }
    app.run();
    app.shutdown();
    return 0;
}
