cmake_minimum_required(VERSION 3.20)
project(LiftSim VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# The simulation core and its tests have no third-party dependencies.
# Configure with -DLIFTSIM_BUILD_APP=OFF to build only those (used by CI).
option(LIFTSIM_BUILD_APP "Build the OpenGL application" ON)

# --- Simulation core: plain C++, no graphics, fully unit-testable -----------
add_library(liftsim_core STATIC
    src/sim/MotionController.cpp
    src/sim/DoorOperator.cpp
    src/sim/Car.cpp
    src/sim/Traffic.cpp
    src/sim/ElevatorSystem.cpp
)
target_include_directories(liftsim_core PUBLIC src)
target_compile_options(liftsim_core PRIVATE -Wall -Wextra)

if(LIFTSIM_BUILD_APP)
include(cmake/Dependencies.cmake)   # SDL2 + GLM

# --- Third-party code committed to the repo ---------------------------------
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

add_library(stb STATIC external/stb/stb_impl.cpp)
target_include_directories(stb PUBLIC external/stb)

# --- Real-time application ---------------------------------------------------
add_executable(LiftSim
    src/main.cpp
    src/app/Application.cpp
    src/render/Shader.cpp
    src/render/Mesh.cpp
    src/render/OrbitCamera.cpp
    src/render/Renderer.cpp
    src/render/InstanceBatch.cpp
    src/view/BuildingLayout.cpp
    src/view/SceneBuilder.cpp
    src/view/PeopleView.cpp
    src/view/Environment.cpp
    src/ui/TextRenderer.cpp
    src/ui/Hud.cpp
    src/ui/Inspector.cpp
)
target_include_directories(LiftSim PRIVATE src)
target_link_libraries(LiftSim PRIVATE liftsim_core glad stb glm::glm SDL2::SDL2-static SDL2::SDL2main opengl32)

# Link the MinGW runtime statically so the .exe runs outside an MSYS2 shell.
if(MINGW)
    target_link_options(LiftSim PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Shaders are loaded at run time; copy them on every build so edits are picked up.
add_custom_target(liftsim_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets)
add_dependencies(LiftSim liftsim_assets)
endif()

# --- Tests -------------------------------------------------------------------
enable_testing()
add_executable(liftsim_tests tests/SimTests.cpp)
target_link_libraries(liftsim_tests PRIVATE liftsim_core)
if(MINGW)
    target_link_options(liftsim_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME simulation COMMAND liftsim_tests)

# --- Traffic study (headless KPI sweep) -------------------------------------
add_executable(liftsim_kpi tools/KpiStudy.cpp)
target_link_libraries(liftsim_kpi PRIVATE liftsim_core)
if(MINGW)
    target_link_options(liftsim_kpi PRIVATE -static -static-libgcc -static-libstdc++)
endif()
