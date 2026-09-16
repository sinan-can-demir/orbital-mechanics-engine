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

# CSVLoader has no OpenGL/GLFW dependency, so it can be tested standalone
# without spinning up a display.
add_executable(test_csv_loader tests/test_csv_loader.cpp src/viewer/csv_loader.cpp)
target_include_directories(test_csv_loader PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_csv_loader PRIVATE glm::glm)

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
    ${ORBIT_VK_SHADER_DIR}/orbit.vert
    ${ORBIT_VK_SHADER_DIR}/orbit.frag
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
add_executable(test_conservation tests/test_conservation.cpp)

target_link_libraries(test_conservation PRIVATE orbit_core)

target_include_directories(test_conservation PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(test_horizons_parser tests/test_horizons_parser.cpp)

target_link_libraries(test_horizons_parser PRIVATE orbit_core)

target_include_directories(test_horizons_parser PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(test_system_writer tests/test_system_writer.cpp)

target_link_libraries(test_system_writer PRIVATE orbit_core)

target_include_directories(test_system_writer PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

enable_testing()

# ── Register existing tests ──────────────────────────────────────────────────
add_test(NAME conservation
    COMMAND test_conservation
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME horizons_parser
    COMMAND test_horizons_parser
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME system_writer
    COMMAND test_system_writer
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ── New test executables ─────────────────────────────────────────────────────
add_executable(test_two_body tests/test_two_body.cpp)
target_link_libraries(test_two_body PRIVATE orbit_core)
target_include_directories(test_two_body PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME two_body
    COMMAND test_two_body
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_barycenter tests/test_barycenter.cpp)
target_link_libraries(test_barycenter PRIVATE orbit_core)
target_include_directories(test_barycenter PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME barycenter
    COMMAND test_barycenter
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_leapfrog tests/test_leapfrog.cpp)
target_link_libraries(test_leapfrog PRIVATE orbit_core)
target_include_directories(test_leapfrog PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME leapfrog
    COMMAND test_leapfrog
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_yoshida4 tests/test_yoshida4.cpp)
target_link_libraries(test_yoshida4 PRIVATE orbit_core)
target_include_directories(test_yoshida4 PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME yoshida4
    COMMAND test_yoshida4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_hermite tests/test_hermite.cpp)
target_link_libraries(test_hermite PRIVATE orbit_core)
target_include_directories(test_hermite PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME hermite
    COMMAND test_hermite
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_cli tests/test_cli.cpp)
target_link_libraries(test_cli PRIVATE orbit_core)
target_include_directories(test_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME cli_smoke
    COMMAND test_cli
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (BUILD_VIEWER)
add_test(NAME csv_loader
    COMMAND test_csv_loader
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
endif()

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