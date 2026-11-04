cmake_minimum_required(VERSION 3.20)
project(MercenaryEngine C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimize release binaries and strip symbols to minimize binary footprint
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
endif()

# GLFW build options
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)

# GLAD
set(GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad)
add_library(glad STATIC
    ${GLAD_DIR}/src/gl.c
)
target_include_directories(glad PUBLIC
    ${GLAD_DIR}/include
)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLAD_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
)

# Tracy Profiler (Real-time CPU/GPU/Memory Telemetry)
set(TRACY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tracy/public)
option(ENABLE_TRACY "Enable Tracy real-time profiling" OFF)

if(ENABLE_TRACY)
    add_library(tracy STATIC ${TRACY_DIR}/TracyClient.cpp)
    target_compile_definitions(tracy PUBLIC TRACY_ENABLE)
    target_include_directories(tracy PUBLIC ${TRACY_DIR})
else()
    add_library(tracy INTERFACE)
    target_include_directories(tracy INTERFACE ${TRACY_DIR})
endif()

# Jolt Physics (Rigid Bodies, Vehicles, Ragdolls, Collision)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/JoltPhysics/Build)

# Recast & Detour Navigation (Battlefield AI Navmesh & Dynamic Pathfinding)
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/recastnavigation)

# meshoptimizer (Mesh Cache Optimization, Overdraw Reduction, Vertex Fetch & LOD)
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/meshoptimizer)

# KTX-Software (KTX2 GPU Texture Compression & Basis Universal Transcoding)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_PY OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS_CTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE STRING "" FORCE)
add_subdirectory(third_party/KTX-Software)

# =========================================================================
# 1. MercenaryCore (Headless Shared Core Library)
# Used by: Dedicated Shard Server, Main Game Client, Map Editor, Launcher
# Contains: Networking, Steam Integration, Spatial Relevancy Grid, Math
# Zero Graphics / Windowing / Audio dependencies!
# =========================================================================
set(CORE_SOURCES
    "src/engine/Network.cpp"
    "src/engine/Network.hpp"
    "src/engine/NetworkTypes.hpp"
    "src/engine/SpatialGrid.hpp"
    "src/engine/SteamIntegration.cpp"
    "src/engine/SteamIntegration.hpp"
)

add_library(MercenaryCore STATIC ${CORE_SOURCES})
target_include_directories(MercenaryCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/public
    ${TRACY_DIR}
)
target_link_libraries(MercenaryCore PUBLIC
    ws2_32
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/redistributable_bin/win64/steam_api64.lib
    tracy
)

# =========================================================================
# 2. MercenaryEngine (Full Client Game & Graphics Engine)
# Used by: Main Game, Map Editor, Launcher
# Contains: PBR Renderer, Shaders, Procedural Rig, Audio, Ballistics, World, ImGui
# =========================================================================
file(GLOB_RECURSE CLIENT_ENGINE_SOURCES
    "src/engine/Audio.cpp"
    "src/engine/SteamAudioBridge.cpp"
    "src/engine/Camera.cpp"
    "src/engine/CharacterRig.cpp"
    "src/engine/PhysicsEngine.cpp"
    "src/engine/NavMeshManager.cpp"
    "src/engine/IK.cpp"
    "src/engine/Mesh.cpp"
    "src/engine/PostProcess.cpp"
    "src/engine/ProceduralMesh.cpp"
    "src/engine/Renderer.cpp"
    "src/engine/Shader.cpp"
    "src/engine/ShadowMap.cpp"
    "src/engine/SkinnedSoldier.cpp"
    "src/engine/Texture.cpp"
    "src/engine/VoiceChat.cpp"
    "src/engine/WeaponRigConfig.cpp"
    "src/engine/Window.cpp"
    "src/game/*.cpp"
    "src/game/*.hpp"
)

add_library(MercenaryEngine STATIC ${CLIENT_ENGINE_SOURCES})
target_include_directories(MercenaryEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLAD_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/public
    ${TRACY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/JoltPhysics
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/recastnavigation/Recast/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/recastnavigation/Detour/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/recastnavigation/DetourCrowd/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/recastnavigation/DetourTileCache/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/KTX-Software/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/KTX-Software/external/basis_universal/zstd
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/steam-audio-sdk/steamaudio/include
)
target_link_libraries(MercenaryEngine PUBLIC
    MercenaryCore
    glad
    glfw
    imgui
    opengl32
    gdi32
    shell32
    tracy
    Jolt
    Recast
    Detour
    DetourCrowd
    DetourTileCache
    meshoptimizer
    ktx
    wininet
    dwmapi
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/steam-audio-sdk/steamaudio/lib/windows-x64/phonon.lib
)

# =========================================================================
# TARGET EXECUTABLES
# =========================================================================

# 1. Main Game Executable
add_executable(MercenaryGame WIN32 "src/main.cpp")
target_link_libraries(MercenaryGame PRIVATE MercenaryEngine)

# 2. Tactical Map Editor Executable
file(GLOB_RECURSE EDITOR_SOURCES
    "src/editor/*.cpp"
    "src/editor/*.h"
    "src/editor/*.hpp"
)
add_executable(MercenaryEditor WIN32 ${EDITOR_SOURCES})
target_link_libraries(MercenaryEditor PRIVATE MercenaryEngine)

# 3. Standalone Bootstrap Game Launcher Executable (Zero External Dynamic DLL Dependencies)
file(GLOB_RECURSE LAUNCHER_SOURCES
    "src/launcher/*.cpp"
    "src/launcher/*.h"
    "src/launcher/*.hpp"
)
add_executable(MercenaryLauncher WIN32 ${LAUNCHER_SOURCES})
target_link_options(MercenaryLauncher PRIVATE -static-libgcc -static-libstdc++)
target_link_libraries(MercenaryLauncher PRIVATE
    glad
    glfw
    imgui
    gdi32
    shell32
    ole32
    uuid
    wininet
    ws2_32
)
target_include_directories(MercenaryLauncher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLAD_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

# 4. Dedicated Persistent Multi-Session Hub Server & GUI Control Center
add_executable(MercenaryServer
    "src/server/ServerMain.cpp"
    "src/server/HubServer.cpp"
    "src/server/DatabaseEngine.cpp"
    "src/server/ServerGUI.cpp"
    "third_party/sqlite/sqlite3.c"
)
target_include_directories(MercenaryServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
)
target_compile_definitions(MercenaryServer PRIVATE
    SQLITE_ENABLE_WAL=1
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0
)
target_link_options(MercenaryServer PRIVATE -static-libgcc -static-libstdc++)
target_link_libraries(MercenaryServer PRIVATE
    glad
    glfw
    imgui
    ws2_32
    winmm
    wininet
    dwmapi
    gdi32
    shell32
    shlwapi
)

# 5. Standalone Vercel-Style Ingress Edge Relay Microserver
add_executable(EdgeRelay "src/edge/EdgeRelay.cpp")
target_include_directories(EdgeRelay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_compile_definitions(EdgeRelay PRIVATE MERCENARY_EDGE_RELAY=1)
target_link_options(EdgeRelay PRIVATE -static-libgcc -static-libstdc++)
target_link_libraries(EdgeRelay PRIVATE
    ws2_32
    winmm
)

# Copy Steam & Steam Audio runtime binaries & AppID config to output directory
foreach(TGT MercenaryGame MercenaryEditor)
    add_custom_command(TARGET ${TGT} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/redistributable_bin/win64/steam_api64.dll"
            "$<TARGET_FILE_DIR:${TGT}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/steam_appid.txt"
            "$<TARGET_FILE_DIR:${TGT}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steam-audio-sdk/steamaudio/lib/windows-x64/phonon.dll"
            "$<TARGET_FILE_DIR:${TGT}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steam-audio-sdk/steamaudio/lib/windows-x64/TrueAudioNext.dll"
            "$<TARGET_FILE_DIR:${TGT}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steam-audio-sdk/steamaudio/lib/windows-x64/GPUUtilities.dll"
            "$<TARGET_FILE_DIR:${TGT}>"
    )
endforeach()

# Mirror executables to project root so launching from root or build/ always runs the latest build
add_custom_command(TARGET MercenaryGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/maps"
        "$<TARGET_FILE_DIR:MercenaryGame>/assets/maps"
)
add_custom_command(TARGET MercenaryLauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MercenaryLauncher>"
        "${CMAKE_CURRENT_SOURCE_DIR}/MercenaryLauncher.exe"
)
add_custom_command(TARGET MercenaryGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MercenaryGame>"
        "${CMAKE_CURRENT_SOURCE_DIR}/MercenaryGame.exe"
)
add_custom_command(TARGET MercenaryServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MercenaryServer>"
        "${CMAKE_CURRENT_SOURCE_DIR}/MercenaryServer.exe"
)
