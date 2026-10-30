cmake_minimum_required(VERSION 3.14)
project(ORBITAL-MECHANICS-ENGINE VERSION 2.0 LANGUAGES C CXX)

# ------------------------------------------------------------
# Global Settings
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable / disable viewer (ON locally, OFF in CI)
option(BUILD_VIEWER "Build OpenGL viewer (requires OpenGL, GLFW, GLM)" ON)
option(BUILD_VULKAN_VIEWER "Build Vulkan viewer (requires Vulkan SDK, GLFW, GLM)" ON)

# Put all executables in build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Project include directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# ------------------------------------------------------------
# External Dependencies (core always needed)
# ------------------------------------------------------------
find_package(CURL REQUIRED)

# ------------------------------------------------------------
# pybind11
# ------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
)
FetchContent_MakeAvailable(pybind11)

# Viewer-only dependencies (only searched if viewer enabled)
if (BUILD_VIEWER)
    find_package(OpenGL)
    find_package(glfw3)
    find_package(glm)

    if (NOT OpenGL_FOUND OR NOT glfw3_FOUND OR NOT glm_FOUND)
        message(WARNING "OpenGL, GLFW3, or GLM not found. Disabling viewer.")
        set(BUILD_VIEWER OFF CACHE BOOL "Build OpenGL viewer" FORCE)
    endif()
endif()

# Vulkan viewer dependencies (independent of the OpenGL viewer — GLFW/GLM are
# backend-agnostic and shared, but this does its own find_package() calls so
# BUILD_VIEWER=OFF doesn't drag BUILD_VULKAN_VIEWER down with it).
if (BUILD_VULKAN_VIEWER)
    find_package(Vulkan)
    find_package(glfw3)
    find_package(glm)

    if (NOT Vulkan_FOUND OR NOT glfw3_FOUND OR NOT glm_FOUND)
        message(WARNING "Vulkan SDK, GLFW3, or GLM not found. Disabling Vulkan viewer.")
        set(BUILD_VULKAN_VIEWER OFF CACHE BOOL "Build Vulkan viewer" FORCE)
    endif()
endif()

if (BUILD_VULKAN_VIEWER)
    FetchContent_Declare(
        vk-bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
        GIT_TAG        v1.3.302
    )
    FetchContent_MakeAvailable(vk-bootstrap)

    # glslc compiles GLSL source to SPIR-V bytecode, the format Vulkan
    # actually consumes at runtime. Ships with the Vulkan SDK / distro
    # vulkan-validation-layers packages, so a plain find_program() is enough
    # rather than a whole FetchContent.
    find_program(GLSLC_EXECUTABLE glslc REQUIRED)

    # VMA (Vulkan Memory Allocator) — handles GPU memory-type selection and
    # suballocation so we don't hand-roll a bump allocator on top of raw
    # vkAllocateMemory. It's a header-only library; the actual implementation
    # is compiled into one .cpp (src/viewer_vk/vma_impl.cpp) via the
    # VMA_IMPLEMENTATION macro, same pattern as stb_image.h-style libraries.
    FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.1.0
    )
    FetchContent_MakeAvailable(VulkanMemoryAllocator)
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
    src/io/horizons_parser.cpp
    src/io/system_writer.cpp
    src/io/horizons_builder.cpp
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
# orbit-viewer-vk (Vulkan renderer) — OPTIONAL, learning-exercise rewrite
# ------------------------------------------------------------
if (BUILD_VULKAN_VIEWER)

# Compile each GLSL shader to SPIR-V at build time. Each add_custom_command()
# declares a real dependency edge (glslc only reruns when the .vert/.frag
# source is newer than the .spv it produced last time) — this is what
# "wired into the build" means, versus a README note saying "remember to run
# glslc by hand."
set(ORBIT_VK_SHADER_DIR ${PROJECT_SOURCE_DIR}/src/viewer_vk/shaders)
set(ORBIT_VK_SPV_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${ORBIT_VK_SPV_DIR})

set(ORBIT_VK_SHADER_SOURCES
    ${ORBIT_VK_SHADER_DIR}/sphere.vert
    ${ORBIT_VK_SHADER_DIR}/sphere.frag
)
set(ORBIT_VK_SHADER_SPVS "")
foreach(shader_src ${ORBIT_VK_SHADER_SOURCES})
    get_filename_component(shader_name ${shader_src} NAME)
    set(shader_spv ${ORBIT_VK_SPV_DIR}/${shader_name}.spv)
    add_custom_command(
        OUTPUT ${shader_spv}
        COMMAND ${GLSLC_EXECUTABLE} ${shader_src} -o ${shader_spv}
        DEPENDS ${shader_src}
        COMMENT "Compiling shader ${shader_name} -> SPIR-V"
        VERBATIM
    )
    list(APPEND ORBIT_VK_SHADER_SPVS ${shader_spv})
endforeach()

add_custom_target(orbit-viewer-vk-shaders DEPENDS ${ORBIT_VK_SHADER_SPVS})

add_executable(orbit-viewer-vk
    src/viewer_vk/main.cpp
    src/viewer_vk/vma_impl.cpp
)
add_dependencies(orbit-viewer-vk orbit-viewer-vk-shaders)

target_include_directories(orbit-viewer-vk PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(orbit-viewer-vk PRIVATE
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    glfw
    glm::glm
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    target_compile_definitions(orbit-viewer-vk PRIVATE ORBIT_VK_VALIDATION_LAYERS=1)
endif()

target_compile_definitions(orbit-viewer-vk PRIVATE
    ORBIT_VK_SHADER_DIR="${ORBIT_VK_SPV_DIR}"
)

endif()  # BUILD_VULKAN_VIEWER

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

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

# add_orbit_test(<ctest name> <source stem> [WORKDIR <dir>])
#   Builds test_<stem> from tests/test_<stem>.cpp, links orbit_core, and registers it
#   with ctest under <ctest name>. Tests run from the source dir unless WORKDIR is given.
function(add_orbit_test name stem)
    cmake_parse_arguments(ARG "" "WORKDIR" "" ${ARGN})
    if (NOT ARG_WORKDIR)
        set(ARG_WORKDIR ${CMAKE_SOURCE_DIR})
    endif()
    add_executable(test_${stem} tests/test_${stem}.cpp)
    target_link_libraries(test_${stem} PRIVATE orbit_core)
    target_include_directories(test_${stem} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    add_test(NAME ${name} COMMAND test_${stem} WORKING_DIRECTORY ${ARG_WORKDIR})
endfunction()

add_orbit_test(conservation    conservation)
add_orbit_test(horizons_parser horizons_parser)
add_orbit_test(system_writer   system_writer)
add_orbit_test(two_body        two_body)
add_orbit_test(barycenter      barycenter)
add_orbit_test(leapfrog        leapfrog)
add_orbit_test(yoshida4        yoshida4)
add_orbit_test(hermite         hermite)
add_orbit_test(golden          golden)
add_orbit_test(cli_smoke       cli WORKDIR ${CMAKE_BINARY_DIR})

# ------------------------------------------------------------
# orbit (Python module)
# ------------------------------------------------------------
pybind11_add_module(orbit orbit_py/bindings.cpp)

install(TARGETS orbit COMPONENT python DESTINATION .)

target_link_libraries(orbit PRIVATE orbit_core)

set_target_properties(orbit_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(orbit PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)