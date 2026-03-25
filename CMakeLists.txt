cmake_minimum_required(VERSION 3.14)
project(EarthAndMoonOrbits VERSION 1.0 LANGUAGES C CXX)

# ------------------------------------------------------------
# Global Settings
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable / disable viewer (ON locally, OFF in CI)
option(BUILD_VIEWER "Build OpenGL viewer (requires OpenGL, GLFW, GLM)" ON)

# Put all executables in build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Project include directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# ------------------------------------------------------------
# External Dependencies (core always needed)
# ------------------------------------------------------------
find_package(CURL REQUIRED)

# Viewer-only dependencies (only searched if viewer enabled)
if (BUILD_VIEWER)
    find_package(OpenGL)
    find_package(glfw3)
    find_package(glm)

    if (NOT OpenGL_FOUND OR NOT GLFW3_FOUND OR NOT GLM_FOUND)
        message(WARNING "OpenGL, GLFW3, or GLM not found. Disabling viewer.")
        set(BUILD_VIEWER OFF CACHE BOOL "Build OpenGL viewer" FORCE)
    endif()
endif()

# ------------------------------------------------------------
# GLAD (only needed for viewer, but harmless to always build)
# ------------------------------------------------------------
add_library(glad STATIC
    ${PROJECT_SOURCE_DIR}/external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    ${PROJECT_SOURCE_DIR}/external/glad/include
)

# ------------------------------------------------------------
# orbit_core (physics library)
# ------------------------------------------------------------
add_library(orbit_core STATIC
    src/core/simulation.cpp
    src/core/conservations.cpp
    src/core/barycenter.cpp
    src/core/eclipse.cpp
    src/core/json_loader.cpp
)

target_include_directories(orbit_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(orbit_core PUBLIC cxx_std_17)

# ------------------------------------------------------------
# orbit-viewer (OpenGL renderer) — OPTIONAL
# ------------------------------------------------------------
if (BUILD_VIEWER)

add_executable(orbit-viewer
    src/viewer/orbit_viewer.cpp
    src/viewer/csv_loader.cpp
    src/viewer/sphere_mesh.cpp
)

target_include_directories(orbit-viewer PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/glad/include
)

if (UNIX AND NOT APPLE)
    target_link_libraries(orbit-viewer PRIVATE
        orbit_core
        glad
        glfw
        OpenGL::GL
        glm::glm
        dl
        pthread
    )
else()
    target_link_libraries(orbit-viewer PRIVATE
        orbit_core
        glad
        glfw
        OpenGL::GL
        glm::glm
    )
endif()

endif()  # BUILD_VIEWER

# ------------------------------------------------------------
# orbit-sim (CLI executable)
# ------------------------------------------------------------
add_executable(orbit-sim
    src/cli/main.cpp
    src/cli/cli.cpp
    src/io/validate.cpp
    src/io/horizons.cpp
)

# Compiler warnings
if (MSVC)
    target_compile_options(orbit-sim PRIVATE /W4)
else()
    target_compile_options(orbit-sim PRIVATE -Wall -Wextra -pedantic)
endif()

target_link_libraries(orbit-sim PRIVATE
    orbit_core
    CURL::libcurl
)