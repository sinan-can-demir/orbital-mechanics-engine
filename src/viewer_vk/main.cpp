// orbit-viewer-vk — Vulkan #70 "Boot" + #71 "First pipeline + triangle" +
// #72 "Sphere mesh via VMA" + #73 "Lighting + orbit trails".
//
// #70 opened a window and cleared it to a solid color every frame. #71 added
// the graphics pipeline object and a hardcoded triangle. #72 replaced that
// triangle with a real UV-sphere mesh uploaded via VMA. #73 reaches visual
// parity with the OpenGL viewer's core look for a small hardcoded 3-body
// scene (Sun/Earth/Moon, procedurally animated — not loaded from a CSV, a
// deliberate scope choice to keep this milestone about Vulkan concepts, not
// file parsing): lit spheres per body (Blinn-Phong, ported from
// src/viewer/orbit_viewer.cpp), growing orbit-trail line strips, and the two
// biggest remaining OpenGL-vs-Vulkan conceptual gaps — a second,
// topology-specific pipeline, and descriptor sets for per-body uniforms
// (color, light position, view position) instead of ad-hoc glUniform calls.
//
// This milestone also adds a depth buffer, which #71/#72 didn't need (a
// single convex sphere with back-face culling never overlaps itself in
// depth) but three separate bodies at different distances absolutely can —
// without one, overlapping bodies would render in draw-call order instead
// of correct front-to-back order.
//
// Compared to the OpenGL viewer, nothing here is implicit. OpenGL hides a
// global state machine behind you; every object below (instance, device,
// swapchain, render pass, pipelines, descriptor sets, buffers, framebuffers,
// command buffers, sync objects) is something *you* create, configure, and
// destroy by hand, in a specific order. That's the whole point of the
// exercise.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

constexpr uint32_t kInitialWidth = 1280;
constexpr uint32_t kInitialHeight = 720;
constexpr int kMaxFramesInFlight = 2;

constexpr float kSphereRadius = 1.0f; // unit sphere; per-body radius applied via push constants
constexpr int kSphereSegments = 64;
constexpr int kSphereRings = 32;

// Hardcoded scene constants (see the file header for why this isn't loaded
// from a CSV). Orbit radii/periods are arbitrary "looks reasonable on
// screen" values, not physically scaled.
// Body radii are wildly exaggerated relative to orbit distances — real
// solar-system proportions render as invisible dots, so every orbit
// visualization (including this project's OpenGL viewer) fakes the scale.
// These specific values were tuned so each body subtends enough angular
// size from the fixed camera below for Blinn-Phong shading to actually show
// visible curvature — too small and a sphere looks like a flat disc
// regardless of how correct the lighting math is (the normal barely varies
// across a few degrees of angular size).
constexpr float kSunRadius = 1.4f;
constexpr float kEarthRadius = 0.55f;
constexpr float kMoonRadius = 0.22f;
constexpr float kEarthOrbitRadius = 3.5f;
constexpr float kEarthOrbitPeriod = 12.0f; // seconds per revolution
constexpr float kMoonOrbitRadius = 1.1f;
constexpr float kMoonOrbitPeriod = 2.0f;
constexpr int kTrailSamples = 512;
constexpr float kTrailDuration = kEarthOrbitPeriod; // time to fully reveal a trail

// Position + normal, read from a real vertex buffer (see #72) rather than
// hardcoded inside the shader (see #71's triangle).
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset = offsetof(Vertex, pos);

        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset = offsetof(Vertex, normal);
        return attrs;
    }
};

// A single position, for orbit-trail line strips — no normal needed, trails
// aren't lit.
struct LineVertex
{
    glm::vec3 pos;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(LineVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static VkVertexInputAttributeDescription getAttributeDescription()
    {
        VkVertexInputAttributeDescription attr{};
        attr.binding = 0;
        attr.location = 0;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = 0;
        return attr;
    }
};

struct SpherePushConstants
{
    glm::mat4 mvp;
    glm::vec4 worldOffsetAndRadius; // xyz = world position, w = radius
};

struct OrbitPushConstants
{
    glm::mat4 vp;
    glm::vec4 color; // xyz used; w unused, keeps the block free of packing ambiguity
};

// Mirrors sphere.frag's BodyUBO block field-for-field. Each glm::vec3 is
// explicitly padded to 16 bytes (alignas(16)) to match std140's rule that a
// vec3's *base alignment* is always 16, same as vec4 — get this wrong and
// the GPU reads the wrong bytes for each field, a classic and easy-to-miss
// Vulkan uniform-buffer bug.
struct BodyUBO
{
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 lightPos;
    // xyz = camera position, w = 1.0 for the emissive body (the Sun), 0.0
    // otherwise — mirrors sphere.frag's BodyUBO block field-for-field.
    alignas(16) glm::vec4 viewPosAndEmissive;
};

// Backend-agnostic UV-sphere geometry generation, lifted out of the
// OpenGL-only SphereMesh::build() (src/viewer/sphere_mesh.cpp), which is
// tied to GLuint VAO/VBO/EBO handles and can't be reused directly from
// Vulkan. See GitHub issue #78 for the follow-up to de-duplicate this
// against the OpenGL viewer properly.
void generateSphereMesh(float radius, int segments, int rings, std::vector<Vertex>& vertices,
                        std::vector<uint32_t>& indices)
{
    for (int y = 0; y <= rings; ++y)
    {
        float v = static_cast<float>(y) / static_cast<float>(rings);
        float phi = v * static_cast<float>(M_PI);

        for (int x = 0; x <= segments; ++x)
        {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float theta = u * 2.0f * static_cast<float>(M_PI);

            glm::vec3 pos(radius * std::sin(phi) * std::cos(theta), radius * std::cos(phi),
                          radius * std::sin(phi) * std::sin(theta));
            glm::vec3 normal = pos / radius;
            vertices.push_back({pos, normal});
        }
    }

    for (int y = 0; y < rings; ++y)
    {
        for (int x = 0; x < segments; ++x)
        {
            uint32_t i0 = static_cast<uint32_t>(y * (segments + 1) + x);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + static_cast<uint32_t>(segments + 1);
            uint32_t i3 = i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
}

// Pure functions of time, used both to animate each body live every frame
// and to precompute its full orbit trail upfront (see createBodies()) —
// using the same formula for both means the trail is guaranteed to actually
// pass through the sphere's rendered position at every past moment.
glm::vec3 sunPositionAt(float /*t*/) { return glm::vec3(0.0f); }

glm::vec3 earthPositionAt(float t)
{
    float angle = (t / kEarthOrbitPeriod) * 2.0f * static_cast<float>(M_PI);
    return glm::vec3(kEarthOrbitRadius * std::cos(angle), 0.0f,
                     kEarthOrbitRadius * std::sin(angle));
}

glm::vec3 moonPositionAt(float t)
{
    float angle = (t / kMoonOrbitPeriod) * 2.0f * static_cast<float>(M_PI);
    return earthPositionAt(t) +
           glm::vec3(kMoonOrbitRadius * std::cos(angle), 0.0f, kMoonOrbitRadius * std::sin(angle));
}

struct RenderBody
{
    std::string name;
    glm::vec3 color;
    float radius;
    glm::vec3 (*positionFn)(float);

    VkBuffer trailBuffer = VK_NULL_HANDLE;
    VmaAllocation trailBufferAllocation = VK_NULL_HANDLE;
    uint32_t trailPointCount = 0; // 0 for the Sun: it doesn't move, no trail to show
};

#ifdef ORBIT_VK_VALIDATION_LAYERS
constexpr bool kEnableValidationLayers = true;
#else
constexpr bool kEnableValidationLayers = false;
#endif

// Reads a compiled SPIR-V binary. std::ios::ate seeks to the end on open, so
// tellg() immediately below gives the file size without a separate stat call.
std::vector<char> readSpirvFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open shader file: " + path);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

} // namespace

class VulkanViewerApp
{
  public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

  private:
    // ---- GLFW window ---------------------------------------------------

    void initWindow()
    {
        glfwInit();
        // GLFW defaults to creating an OpenGL context; tell it not to — Vulkan
        // manages its own surface/swapchain instead of a GL context.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window_ =
            glfwCreateWindow(kInitialWidth, kInitialHeight, "orbit-viewer-vk", nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int, int)
    {
        auto* app = reinterpret_cast<VulkanViewerApp*>(glfwGetWindowUserPointer(window));
        app->framebufferResized_ = true;
    }

    // ---- Vulkan bring-up -------------------------------------------------
    //
    // Order matters and mirrors the dependency chain: instance -> surface ->
    // physical device -> logical device + queues -> allocator -> swapchain
    // -> depth image (needs allocator + swapchain extent) -> descriptor set
    // layout (needed by pipeline layouts) -> render pass (needs the depth
    // format) -> pipelines -> framebuffers (need the depth image view) ->
    // command pool/buffers -> mesh + body/trail buffers (need the command
    // pool for staging copies) -> descriptor pool/UBOs/sets (need the body
    // count from createBodies()) -> sync objects.

    void initVulkan()
    {
        createInstance();
        createSurface();
        pickPhysicalDeviceAndCreateDevice();
        createAllocator();
        createSwapchain();
        createDepthResources();
        createColorResources();
        createDescriptorSetLayout();
        createRenderPass();
        createSpherePipeline();
        createOrbitPipeline();
        createFramebuffers();
        createCommandPoolAndBuffers();
        createMeshBuffers();
        createBodies();
        createDescriptorPool();
        createBodyUniformBuffers();
        createDescriptorSets();
        createSyncObjects();
    }

    void createInstance()
    {
        vkb::InstanceBuilder builder;
        auto instRet = builder.set_app_name("orbit-viewer-vk")
                           .request_validation_layers(kEnableValidationLayers)
                           .use_default_debug_messenger()
                           .require_api_version(1, 2, 0)
                           .build();
        if (!instRet)
        {
            throw std::runtime_error("failed to create Vulkan instance: " +
                                     instRet.error().message());
        }
        vkbInstance_ = instRet.value();
        instance_ = vkbInstance_.instance;
        debugMessenger_ = vkbInstance_.debug_messenger;
    }

    void createSurface()
    {
        // The surface is the one part of this whole app that's platform-specific
        // (Vulkan itself knows nothing about windows). GLFW hides the
        // Win32/Xlib/Wayland/Cocoa details behind this single call.
        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface");
        }
    }

    void pickPhysicalDeviceAndCreateDevice()
    {
        // Physical device = an actual GPU in the machine. Logical device (below)
        // = your application's private handle to it, through which every other
        // Vulkan call is dispatched.
        // wideLines lifts VkPipelineRasterizationStateCreateInfo::lineWidth
        // above 1.0 — without requesting it, a driver may reject any
        // non-default line width outright. Requesting it here restricts
        // physical-device selection to GPUs that actually support it, and
        // vk-bootstrap enables it automatically on the logical device below.
        VkPhysicalDeviceFeatures requiredFeatures{};
        requiredFeatures.wideLines = VK_TRUE;

        vkb::PhysicalDeviceSelector selector{vkbInstance_};
        auto physRet = selector.set_surface(surface_)
                           .set_minimum_version(1, 2)
                           .set_required_features(requiredFeatures)
                           .select();
        if (!physRet)
        {
            throw std::runtime_error("failed to select Vulkan physical device: " +
                                     physRet.error().message());
        }
        vkb::PhysicalDevice vkbPhysicalDevice = physRet.value();

        vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
        auto devRet = deviceBuilder.build();
        if (!devRet)
        {
            throw std::runtime_error("failed to create Vulkan logical device: " +
                                     devRet.error().message());
        }
        vkbDevice_ = devRet.value();
        physicalDevice_ = vkbPhysicalDevice.physical_device;
        device_ = vkbDevice_.device;

        // Most consumer GPUs expose one queue family that supports both
        // graphics and present, but the spec doesn't guarantee it — vk-bootstrap
        // picks whatever's actually available, which may be two different
        // queues (or even the same queue under two names).
        auto graphicsQueueRet = vkbDevice_.get_queue(vkb::QueueType::graphics);
        auto presentQueueRet = vkbDevice_.get_queue(vkb::QueueType::present);
        if (!graphicsQueueRet || !presentQueueRet)
        {
            throw std::runtime_error("failed to get graphics/present queues");
        }
        graphicsQueue_ = graphicsQueueRet.value();
        presentQueue_ = presentQueueRet.value();
        graphicsQueueFamily_ = vkbDevice_.get_queue_index(vkb::QueueType::graphics).value();

        msaaSamples_ = getMaxUsableSampleCount();
    }

    // MSAA (multisample anti-aliasing) smooths the jagged, stair-stepped
    // edges you get from rendering at 1 sample/pixel — every silhouette
    // edge (sphere outlines especially) picks up intermediate blended
    // colors instead of a hard binary in/out decision. Not every sample
    // count a GPU *could* support is worth using; cap at 8 rather than
    // whatever the hardware maximum is (sometimes higher) for a sane
    // perf/quality tradeoff on a scene this simple.
    VkSampleCountFlagBits getMaxUsableSampleCount()
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice_, &props);
        VkSampleCountFlags counts =
            props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;

        for (VkSampleCountFlagBits bit :
             {VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT})
        {
            if (counts & bit)
            {
                return bit;
            }
        }
        return VK_SAMPLE_COUNT_1_BIT;
    }

    void createSwapchain()
    {
        // The swapchain is a ring of images the GPU renders into and the
        // presentation engine (compositor/display) shows on screen. You never
        // render "directly to the screen" in Vulkan — you render to one of
        // these images, then hand it back for presentation.
        vkb::SwapchainBuilder swapchainBuilder{vkbDevice_};
        auto swapRet = swapchainBuilder.set_old_swapchain(vkbSwapchain_)
                           .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                           .build();
        if (!swapRet)
        {
            throw std::runtime_error("failed to create swapchain: " + swapRet.error().message());
        }

        // vk-bootstrap keeps the old swapchain's handle inside vkbSwapchain_
        // until we explicitly destroy it — do that now that the new one exists.
        vkb::destroy_swapchain(vkbSwapchain_);

        vkbSwapchain_ = swapRet.value();
        swapchain_ = vkbSwapchain_.swapchain;
        swapchainImageFormat_ = vkbSwapchain_.image_format;
        swapchainExtent_ = vkbSwapchain_.extent;
        swapchainImageViews_ = vkbSwapchain_.get_image_views().value();
    }

    // ---- Depth buffer ---------------------------------------------------
    //
    // #71/#72 never needed one: a single convex sphere with back-face
    // culling can't occlude itself incorrectly. Three separate bodies at
    // different distances can and do overlap on screen, so without a depth
    // buffer they'd render in draw-call order rather than correct
    // front-to-back order.

    VkFormat findDepthFormat()
    {
        // Not every format is guaranteed to support
        // VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT on every driver —
        // query instead of assuming, and take the first candidate that does.
        std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                            VK_FORMAT_D24_UNORM_S8_UINT};
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                return format;
            }
        }
        throw std::runtime_error("failed to find a supported depth format");
    }

    void createDepthResources()
    {
        if (depthFormat_ == VK_FORMAT_UNDEFINED)
        {
            depthFormat_ = findDepthFormat();
        }

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat_;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        // Must match the color attachment's sample count — a render pass
        // requires every attachment used by the same subpass to agree on it.
        imageInfo.samples = msaaSamples_;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &depthImage_, &depthImageAllocation_,
                           nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create depth image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create depth image view");
        }
    }

    void cleanupDepthResources()
    {
        vkDestroyImageView(device_, depthImageView_, nullptr);
        vmaDestroyImage(allocator_, depthImage_, depthImageAllocation_);
    }

    // The actual rendering happens into this multisampled color image, not
    // directly into a swapchain image — swapchain images are always 1
    // sample (that's what gets presented), so the render pass's resolve
    // step (see createRenderPass()) downsamples this into the swapchain
    // image at the end of the subpass. TRANSIENT_ATTACHMENT_BIT tells the
    // driver its contents never need to survive outside this one render
    // pass, which on tile-based GPUs means it may never even hit real VRAM.
    void createColorResources()
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapchainImageFormat_;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage =
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.samples = msaaSamples_;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &colorImage_, &colorImageAllocation_,
                           nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create MSAA color image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = colorImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &viewInfo, nullptr, &colorImageView_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create MSAA color image view");
        }
    }

    void cleanupColorResources()
    {
        vkDestroyImageView(device_, colorImageView_, nullptr);
        vmaDestroyImage(allocator_, colorImage_, colorImageAllocation_);
    }

    // ---- Descriptor set layout ---------------------------------------------
    //
    // The layout is just a schema — "binding 0 is one uniform buffer,
    // visible to the fragment stage" — created once and shared by every
    // body's actual descriptor set (created later in createDescriptorSets(),
    // once we know how many bodies there are).

    void createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout");
        }
    }

    void createRenderPass()
    {
        // A render pass describes *what kind* of attachments a frame uses and
        // how their contents transition across the frame (load, store, and the
        // image layout before/after) — not the actual pixels, just the plan.
        // Rendered into at msaaSamples_ samples/pixel, never presented
        // directly — resolved down to 1 sample by the resolve attachment
        // below at the end of the subpass.
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainImageFormat_;
        colorAttachment.samples = msaaSamples_;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // The resolve step reads it, not anything outside this render pass —
        // DONT_CARE lets a tile-based GPU skip writing it to memory at all.
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat_;
        depthAttachment.samples = msaaSamples_;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // We never read the depth buffer back after the frame — DONT_CARE
        // lets the driver skip writing it out, unlike the color attachment.
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // The actual swapchain image (always 1 sample) — the subpass writes
        // the final resolved (downsampled) pixels here via pResolveAttachments
        // below, entirely as a side effect of the driver's fixed-function
        // multisample resolve; no shader/draw call targets this directly.
        VkAttachmentDescription colorResolveAttachment{};
        colorResolveAttachment.format = swapchainImageFormat_;
        colorResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorResolveRef{};
        colorResolveRef.attachment = 2;
        colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorResolveRef;

        // The implicit "external" subpass boundary needs an explicit
        // dependency telling the GPU not to start color/depth writes until
        // the swapchain image is actually available (signaled by the
        // image-available semaphore in drawFrame()) — extended this
        // milestone to also cover the depth-testing pipeline stage.
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment,
                                                              colorResolveAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass");
        }
    }

    VkShaderModule createShaderModule(const std::vector<char>& code)
    {
        // A VkShaderModule is just the SPIR-V bytecode wrapped in a Vulkan
        // handle — no compilation to GPU machine code happens yet. That
        // happens when the module is referenced inside pipeline creation
        // below, where the driver can see the *whole* pipeline state at once
        // and optimize accordingly (part of why pipelines are baked upfront).
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module");
        }
        return shaderModule;
    }

    // Depth-stencil state is identical for both pipelines this milestone
    // (test + write enabled, standard "closer wins" compare op) — factored
    // out so createSpherePipeline() and createOrbitPipeline() don't repeat it.
    VkPipelineDepthStencilStateCreateInfo makeDepthStencilState()
    {
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        return depthStencil;
    }

    void createSpherePipeline()
    {
        // ---- Programmable stages: load the two shader modules ----
        auto vertCode = readSpirvFile(std::string(ORBIT_VK_SHADER_DIR) + "/sphere.vert.spv");
        auto fragCode = readSpirvFile(std::string(ORBIT_VK_SHADER_DIR) + "/sphere.frag.spv");
        VkShaderModule vertModule = createShaderModule(vertCode);
        VkShaderModule fragModule = createShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertModule;
        vertStageInfo.pName = "main"; // entry point function name inside the GLSL

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragModule;
        fragStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

        // ---- Vertex input: describes the layout of the real vertex buffer
        // created in createMeshBuffers() — the fixed-function vertex-fetch
        // stage uses this to know how to read Vertex structs out of GPU
        // memory and feed them to the vertex shader's `in` attributes.
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // ---- Input assembly: how to group the vertices Vulkan feeds the
        // vertex shader. TRIANGLE_LIST = every 3 vertices forms one
        // independent triangle.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // ---- Viewport/scissor: made *dynamic* (set per-frame via
        // vkCmdSetViewport/vkCmdSetScissor in recordCommandBuffer) rather than
        // baked into the pipeline, since the window — and therefore the
        // swapchain extent — can resize. Baking a fixed size in would mean
        // rebuilding the whole pipeline on every resize.
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // ---- Rasterizer: turns the triangle into fragments (candidate
        // pixels). FILL = solid triangles (vs. LINE for wireframe, POINT for
        // vertices only).
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        // CCW here (vs. #71's flat hardcoded triangle, which needed
        // CLOCKWISE) because the projection matrix flips clip-space Y to
        // correct for GLM assuming OpenGL's Y-up convention — that flip
        // mirrors the apparent winding of every triangle, so the "front
        // face" definition has to flip along with it.
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // ---- Multisampling: must match the render pass's attachment sample
        // count (msaaSamples_) — the pipeline and the attachments it draws
        // into have to agree on this.
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = msaaSamples_;

        VkPipelineDepthStencilStateCreateInfo depthStencil = makeDepthStencilState();

        // ---- Color blending: no blending, just overwrite the framebuffer
        // pixel with whatever the fragment shader outputs (opaque sphere).
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // ---- Pipeline layout: one push-constant range for the MVP +
        // world-offset/radius (vertex stage only), plus the per-body
        // descriptor set layout for color/light/view (fragment stage,
        // bound to an actual buffer per body in recordCommandBuffer).
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SpherePushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &spherePipelineLayout_) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create sphere pipeline layout");
        }

        // ---- Tie it all together into one immutable VkPipeline. renderPass_
        // + subpass = 0 tells the driver which framebuffer attachment layout
        // this pipeline is compatible with.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = spherePipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &spherePipeline_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create sphere graphics pipeline");
        }

        // Shader modules are only needed during pipeline creation above — the
        // driver has already consumed and compiled the bytecode by this
        // point, so these can be destroyed immediately rather than kept
        // around for the app's lifetime.
        vkDestroyShaderModule(device_, fragModule, nullptr);
        vkDestroyShaderModule(device_, vertModule, nullptr);
    }

    // A second, topology-specific pipeline for orbit trails. Vulkan bakes
    // primitive topology into the pipeline object — unlike OpenGL, where
    // glDrawArrays(GL_LINE_STRIP, ...) vs glDrawElements(GL_TRIANGLES, ...)
    // is just a different argument to the same draw call, switching
    // topology in Vulkan means switching to a whole different VkPipeline.
    void createOrbitPipeline()
    {
        auto vertCode = readSpirvFile(std::string(ORBIT_VK_SHADER_DIR) + "/orbit.vert.spv");
        auto fragCode = readSpirvFile(std::string(ORBIT_VK_SHADER_DIR) + "/orbit.frag.spv");
        VkShaderModule vertModule = createShaderModule(vertCode);
        VkShaderModule fragModule = createShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertModule;
        vertStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragModule;
        fragStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

        auto bindingDescription = LineVertex::getBindingDescription();
        auto attributeDescription = LineVertex::getAttributeDescription();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = 1;
        vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 2.0f; // matches the OpenGL viewer's glLineWidth(2.0f); needs
                                     // wideLines (requested above)
        rasterizer.cullMode = VK_CULL_MODE_NONE; // lines have no "back face"
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = msaaSamples_;

        VkPipelineDepthStencilStateCreateInfo depthStencil = makeDepthStencilState();

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // No descriptor sets — a trail only needs a view-projection matrix
        // and a flat color, both small enough for push constants, visible to
        // both stages since orbit.frag reads pc.color.
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(OrbitPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &orbitPipelineLayout_) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create orbit pipeline layout");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = orbitPipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &orbitPipeline_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create orbit graphics pipeline");
        }

        vkDestroyShaderModule(device_, fragModule, nullptr);
        vkDestroyShaderModule(device_, vertModule, nullptr);
    }

    void createFramebuffers()
    {
        // A framebuffer binds concrete image views to a render pass's attachment
        // slots, in the same order the render pass declared them (color-msaa,
        // depth-msaa, color-resolve). We need one per swapchain image, since
        // each is a distinct resolve target the GPU might write into at any
        // given time — but the MSAA color/depth views are shared across all
        // of them, since only one frame is ever actually mid-render at once.
        framebuffers_.resize(swapchainImageViews_.size());
        for (size_t i = 0; i < swapchainImageViews_.size(); ++i)
        {
            std::array<VkImageView, 3> attachments = {colorImageView_, depthImageView_,
                                                      swapchainImageViews_[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass_;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapchainExtent_.width;
            framebufferInfo.height = swapchainExtent_.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffers_[i]) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to create framebuffer");
            }
        }
    }

    void createCommandPoolAndBuffers()
    {
        // A command pool allocates command buffers from a specific queue
        // family. VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT lets us
        // vkResetCommandBuffer() individual buffers each frame instead of
        // resetting the whole pool.
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily_;

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool");
        }

        commandBuffers_.resize(kMaxFramesInFlight);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = kMaxFramesInFlight;

        if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    void createAllocator()
    {
        // VMA needs to know which Vulkan entry points to bind against; the
        // default (VMA_STATIC_VULKAN_FUNCTIONS, implied when vulkan.h is
        // visible, which it is via GLFW_INCLUDE_VULKAN above) links directly
        // against the loader we already link via Vulkan::Vulkan.
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = physicalDevice_;
        allocatorInfo.device = device_;
        allocatorInfo.instance = instance_;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

        if (vmaCreateAllocator(&allocatorInfo, &allocator_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create VMA allocator");
        }
    }

    // A short-lived command buffer for one-off GPU work (here, the
    // staging->device-local buffer copy) that isn't part of the per-frame
    // render loop. Submitted and waited on synchronously — fine for
    // one-time setup work, not something you'd do every frame.
    VkCommandBuffer beginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool_;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // vkQueueWaitIdle rather than a fence: simplest correct option for
        // one-time setup work that isn't on the hot per-frame path.
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
    }

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
        endSingleTimeCommands(commandBuffer);
    }

    // The staging-buffer pattern: data is only ever memcpy'd into
    // host-visible memory (the staging buffer), never directly into the
    // device-local buffer the GPU actually reads from during rendering.
    // Used for anything uploaded once and never touched by the CPU again:
    // the sphere mesh, and each body's precomputed orbit trail below.
    template <typename T>
    void uploadViaStagingBuffer(const std::vector<T>& data, VkBufferUsageFlags usage,
                                VkBuffer& outBuffer, VmaAllocation& outAllocation)
    {
        VkDeviceSize bufferSize = sizeof(T) * data.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;
        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = bufferSize;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo stagingInfoOut{};
        if (vmaCreateBuffer(allocator_, &stagingInfo, &stagingAllocInfo, &stagingBuffer,
                            &stagingAllocation, &stagingInfoOut) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create staging buffer");
        }
        // VMA_ALLOCATION_CREATE_MAPPED_BIT means the allocation is already
        // mapped and pMappedData is valid immediately — no separate
        // vmaMapMemory() call needed for a host-visible allocation like this.
        std::memcpy(stagingInfoOut.pMappedData, data.data(), static_cast<size_t>(bufferSize));

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &outBuffer, &outAllocation,
                            nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create device-local buffer");
        }

        copyBuffer(stagingBuffer, outBuffer, bufferSize);

        vmaDestroyBuffer(allocator_, stagingBuffer, stagingAllocation);
    }

    void createMeshBuffers()
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        generateSphereMesh(kSphereRadius, kSphereSegments, kSphereRings, vertices, indices);
        sphereIndexCount_ = static_cast<uint32_t>(indices.size());

        uploadViaStagingBuffer(vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_,
                               vertexBufferAllocation_);
        uploadViaStagingBuffer(indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_,
                               indexBufferAllocation_);
    }

    // The hardcoded 3-body scene (see the file header for why this isn't
    // loaded from a CSV). Each body's orbit trail is precomputed here by
    // sampling its own live position function across one full
    // kTrailDuration — the same function recordCommandBuffer() calls every
    // frame to place the sphere — and uploaded once via the staging-buffer
    // pattern, exactly like the sphere mesh. "Growing" a trail during
    // playback then just means drawing an increasing *prefix* of this
    // already-uploaded buffer (see recordCommandBuffer()), not re-uploading
    // anything — the actual technique the OpenGL viewer uses too
    // (src/viewer/orbit_viewer.cpp draws glDrawArrays(GL_LINE_STRIP, 0,
    // count) with a growing count against a buffer it also only uploads
    // once).
    void createBodies()
    {
        bodies_.push_back({"Sun", glm::vec3(1.0f, 0.85f, 0.3f), kSunRadius, sunPositionAt});
        bodies_.push_back({"Earth", glm::vec3(0.25f, 0.55f, 1.0f), kEarthRadius, earthPositionAt});
        bodies_.push_back({"Moon", glm::vec3(0.65f, 0.65f, 0.65f), kMoonRadius, moonPositionAt});

        // The Sun doesn't move in this scene, so it has no meaningful trail
        // (index 0, skipped below).
        for (size_t i = 1; i < bodies_.size(); ++i)
        {
            RenderBody& body = bodies_[i];
            std::vector<LineVertex> trail;
            trail.reserve(kTrailSamples);
            for (int s = 0; s < kTrailSamples; ++s)
            {
                float t =
                    kTrailDuration * static_cast<float>(s) / static_cast<float>(kTrailSamples - 1);
                trail.push_back({body.positionFn(t)});
            }
            body.trailPointCount = static_cast<uint32_t>(trail.size());
            uploadViaStagingBuffer(trail, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, body.trailBuffer,
                                   body.trailBufferAllocation);
        }
    }

    // ---- Descriptor pool, per-body uniform buffers, and descriptor sets ---
    //
    // One VkBuffer + one VkDescriptorSet per (body, frame-in-flight) pair —
    // frame-in-flight, because up to kMaxFramesInFlight command buffers can
    // be in the GPU's queue at once (see createSyncObjects()), and
    // overwriting a UBO that an earlier frame's command buffer might still
    // be reading from would be a race condition.

    void createDescriptorPool()
    {
        uint32_t setCount = static_cast<uint32_t>(bodies_.size() * kMaxFramesInFlight);

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = setCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = setCount;

        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool");
        }
    }

    void createBodyUniformBuffers()
    {
        size_t total = bodies_.size() * kMaxFramesInFlight;
        bodyUboBuffers_.resize(total);
        bodyUboAllocations_.resize(total);
        bodyUboMapped_.resize(total);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(BodyUBO);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        // Uniform buffers are rewritten every frame, so a persistently-
        // mapped host-visible allocation (no staging buffer) is the right
        // call here — unlike the sphere mesh/trails above, which are
        // uploaded once and never touched by the CPU again.
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

        for (size_t i = 0; i < total; ++i)
        {
            VmaAllocationInfo outInfo{};
            if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &bodyUboBuffers_[i],
                                &bodyUboAllocations_[i], &outInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create body uniform buffer");
            }
            bodyUboMapped_[i] = outInfo.pMappedData;
        }
    }

    void createDescriptorSets()
    {
        size_t total = bodies_.size() * kMaxFramesInFlight;
        std::vector<VkDescriptorSetLayout> layouts(total, descriptorSetLayout_);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(total);
        allocInfo.pSetLayouts = layouts.data();

        bodyDescriptorSets_.resize(total);
        if (vkAllocateDescriptorSets(device_, &allocInfo, bodyDescriptorSets_.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        for (size_t i = 0; i < total; ++i)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = bodyUboBuffers_[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(BodyUBO);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = bodyDescriptorSets_[i];
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        }
    }

    void createSyncObjects()
    {
        // Two kinds of sync here, for two different audiences:
        //  - Semaphores sync GPU-to-GPU: they order work *within* the GPU
        //    timeline (don't start rendering until the image is acquired;
        //    don't present until rendering is done).
        //  - Fences (inFlight) sync CPU-to-GPU: they let the CPU know a given
        //    frame's command buffer is safe to re-record, so we don't overwrite
        //    a command buffer the GPU is still executing.
        //
        // imageAvailable and inFlight are indexed by *frame-in-flight slot*
        // (0..kMaxFramesInFlight-1) — that's fine, since each is entirely
        // consumed within the same drawFrame() call that signals it.
        // renderFinished is different: it's still "in use" by the present
        // engine after our fence says the frame is done, so it must be indexed
        // by *swapchain image* instead (see createRenderFinishedSemaphores()) —
        // otherwise, with more swapchain images than frames-in-flight, we can
        // re-signal a semaphore the present engine hasn't finished consuming
        // yet, which validation layers correctly reject.
        imageAvailableSemaphores_.resize(kMaxFramesInFlight);
        inFlightFences_.resize(kMaxFramesInFlight);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // Start signaled so the very first drawFrame() doesn't wait forever on
        // a fence that nothing has ever submitted.
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < kMaxFramesInFlight; ++i)
        {
            if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                                  &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create sync objects for a frame");
            }
        }

        createRenderFinishedSemaphores();
    }

    void createRenderFinishedSemaphores()
    {
        // One per swapchain image, not per frame-in-flight — see the note in
        // createSyncObjects(). Recreated whenever the swapchain is (the image
        // count can change across recreation).
        renderFinishedSemaphores_.resize(swapchainImageViews_.size());
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (auto& semaphore : renderFinishedSemaphores_)
        {
            if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create render-finished semaphore");
            }
        }
    }

    void destroyRenderFinishedSemaphores()
    {
        for (auto semaphore : renderFinishedSemaphores_)
        {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }
        renderFinishedSemaphores_.clear();
    }

    // ---- Swapchain recreation ---------------------------------------------

    void cleanupSwapchain()
    {
        destroyRenderFinishedSemaphores();
        for (auto framebuffer : framebuffers_)
        {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
        framebuffers_.clear();
        // Framebuffers reference the depth/color-msaa views, so both must be
        // destroyed after them, not before.
        cleanupDepthResources();
        cleanupColorResources();
        vkbSwapchain_.destroy_image_views(swapchainImageViews_);
        swapchainImageViews_.clear();
    }

    void recreateSwapchain()
    {
        // Minimizing the window gives a 0x0 framebuffer size, which Vulkan
        // rejects — just block until the window has real dimensions again
        // rather than spin.
        int width = 0, height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }

        // Don't touch anything the GPU might still be reading from.
        vkDeviceWaitIdle(device_);

        cleanupSwapchain();
        createSwapchain();
        // The depth and MSAA color images are sized to the swapchain
        // extent, so both need recreating alongside it. Render
        // pass/pipelines are unaffected since neither format nor sample
        // count changes across resize on this hardware/platform.
        createDepthResources();
        createColorResources();
        createFramebuffers();
        createRenderFinishedSemaphores();
    }

    // ---- Frame loop ---------------------------------------------------

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window_))
        {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device_);
    }

    // Fixed camera + projection, shared by every body and trail drawn this
    // frame — real camera control (mouse orbit, zoom) is #74's job. Y-flip
    // explained where it's applied: GLM assumes OpenGL's Y-up clip space;
    // Vulkan's is Y-down.
    void computeViewProj(glm::mat4& outView, glm::mat4& outProj, glm::vec3& outCameraPos)
    {
        // Off-axis (not straight down the Z axis) so orbit depth/curvature
        // and sphere shading actually read as 3D rather than a flat,
        // symmetric top-down view.
        outCameraPos = glm::vec3(6.0f, 5.5f, 9.0f);
        outView = glm::lookAt(outCameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        float aspect = static_cast<float>(swapchainExtent_.width) /
                       static_cast<float>(swapchainExtent_.height);
        // Wider than a typical 45° default — lets the fixed camera sit
        // close enough to give bodies a visible angular size (see the radii
        // comment above) while still framing the whole orbit.
        outProj = glm::perspective(glm::radians(55.0f), aspect, 0.1f, 100.0f);
        outProj[1][1] *= -1.0f;
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        // Index order must match the render pass's attachment order (color-
        // msaa, depth-msaa, color-resolve). clearValues[2] is never actually
        // used — the resolve attachment's loadOp is DONT_CARE, since the
        // resolve step overwrites it completely — but Vulkan still requires
        // clearValueCount to match attachmentCount, so a slot must exist.
        std::array<VkClearValue, 3> clearValues{};
        clearValues[0].color = {{0.05f, 0.07f, 0.12f, 1.0f}};
        // 1.0 = the far plane in Vulkan's default [0, 1] depth range — start
        // every pixel as "as far away as possible" so the first real
        // fragment drawn at that pixel always passes the LESS depth test.
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = framebuffers_[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent_;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Viewport/scissor were declared dynamic in both pipelines, so they
        // must be set here, every frame, using the current swapchain extent
        // (which changes across recreateSwapchain() on resize).
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchainExtent_.width);
        viewport.height = static_cast<float>(swapchainExtent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchainExtent_;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        float t = static_cast<float>(glfwGetTime());
        glm::mat4 view, proj;
        glm::vec3 cameraPos;
        computeViewProj(view, proj, cameraPos);
        glm::mat4 vp = proj * view;
        glm::vec3 sunWorldPos = bodies_[0].positionFn(t);

        // ---- Pass 1: orbit trails (line-strip pipeline) ----
        //
        // Drawing trails before spheres or after makes no visual difference
        // here — both pipelines test *and write* depth, so final visibility
        // is resolved correctly regardless of draw order (only overdraw
        // performance would differ, irrelevant at this scale).
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, orbitPipeline_);
        for (const auto& body : bodies_)
        {
            if (body.trailPointCount < 2)
            {
                continue;
            }

            // Reveal a growing prefix of the precomputed trail as t advances,
            // capping at the full length once t passes kTrailDuration —
            // this is the entire "growth" mechanism; the buffer itself was
            // uploaded once, in createBodies().
            float progress = std::min(t / kTrailDuration, 1.0f);
            uint32_t visibleCount =
                1 + static_cast<uint32_t>(progress * static_cast<float>(body.trailPointCount - 1));
            if (visibleCount < 2)
            {
                continue;
            }

            OrbitPushConstants orbitPc{vp, glm::vec4(body.color, 0.0f)};
            vkCmdPushConstants(commandBuffer, orbitPipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(OrbitPushConstants), &orbitPc);

            VkBuffer trailBuffers[] = {body.trailBuffer};
            VkDeviceSize trailOffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, trailBuffers, trailOffsets);
            vkCmdDraw(commandBuffer, visibleCount, 1, 0, 0);
        }

        // ---- Pass 2: lit spheres (triangle-list pipeline) ----
        //
        // The sphere mesh is identical for every body (only push
        // constants/descriptor set differ per body), so bind it once outside
        // the loop rather than redundantly per body.
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline_);
        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

        for (size_t i = 0; i < bodies_.size(); ++i)
        {
            const RenderBody& body = bodies_[i];
            glm::vec3 worldPos = body.positionFn(t);

            glm::mat4 model =
                glm::scale(glm::translate(glm::mat4(1.0f), worldPos), glm::vec3(body.radius));
            glm::mat4 mvp = vp * model;

            // Rewrite this body's UBO for *this* frame-in-flight slot before
            // binding its descriptor set — safe because the fence wait at
            // the top of drawFrame() guarantees the GPU is done with
            // whatever this slot's descriptor set pointed at last time this
            // same slot was used.
            size_t uboIndex = i * kMaxFramesInFlight + currentFrame_;
            float isEmissive =
                (i == 0) ? 1.0f : 0.0f; // body 0 is always the Sun (see createBodies())
            BodyUBO ubo{body.color, sunWorldPos, glm::vec4(cameraPos, isEmissive)};
            std::memcpy(bodyUboMapped_[uboIndex], &ubo, sizeof(BodyUBO));

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    spherePipelineLayout_, 0, 1, &bodyDescriptorSets_[uboIndex], 0,
                                    nullptr);

            SpherePushConstants spherePc{mvp, glm::vec4(worldPos, body.radius)};
            vkCmdPushConstants(commandBuffer, spherePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(SpherePushConstants), &spherePc);

            vkCmdDrawIndexed(commandBuffer, sphereIndexCount_, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer");
        }
    }

    void drawFrame()
    {
        // Wait for this frame-in-flight slot's previous submission to finish
        // before reusing its command buffer (and, this milestone, its
        // per-body UBOs/descriptor sets).
        vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult acquireResult = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                       imageAvailableSemaphores_[currentFrame_],
                                                       VK_NULL_HANDLE, &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain();
            return;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swapchain image");
        }

        // Only reset the fence once we know we're actually submitting work this
        // call — otherwise an early return above would leave it permanently
        // unsignaled.
        vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

        vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
        recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        // Indexed by imageIndex, not currentFrame_ — see createSyncObjects().
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[imageIndex]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapchains[] = {swapchain_};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
            framebufferResized_)
        {
            framebufferResized_ = false;
            recreateSwapchain();
        }
        else if (presentResult != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swapchain image");
        }

        currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
    }

    // ---- Teardown ---------------------------------------------------
    //
    // Destruction order is the exact reverse of creation — Vulkan won't stop
    // you from getting this wrong, it'll just crash or leak, which is why
    // validation layers (enabled above) matter: they catch use-after-destroy
    // and leaked-handle mistakes that would otherwise be silent.

    void cleanup()
    {
        cleanupSwapchain();
        vkb::destroy_swapchain(vkbSwapchain_);

        // renderFinishedSemaphores_/depth resources are already destroyed
        // above, inside cleanupSwapchain().
        for (int i = 0; i < kMaxFramesInFlight; ++i)
        {
            vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
            vkDestroyFence(device_, inFlightFences_[i], nullptr);
        }
        vkDestroyCommandPool(device_, commandPool_, nullptr);

        vkDestroyPipeline(device_, orbitPipeline_, nullptr);
        vkDestroyPipelineLayout(device_, orbitPipelineLayout_, nullptr);
        vkDestroyPipeline(device_, spherePipeline_, nullptr);
        vkDestroyPipelineLayout(device_, spherePipelineLayout_, nullptr);
        vkDestroyRenderPass(device_, renderPass_, nullptr);

        // Descriptor pool destruction implicitly frees every descriptor set
        // allocated from it — no need to free bodyDescriptorSets_ one by one.
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);

        for (size_t i = 0; i < bodyUboBuffers_.size(); ++i)
        {
            vmaDestroyBuffer(allocator_, bodyUboBuffers_[i], bodyUboAllocations_[i]);
        }

        for (auto& body : bodies_)
        {
            if (body.trailBuffer != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(allocator_, body.trailBuffer, body.trailBufferAllocation);
            }
        }

        // Buffers must be destroyed before the allocator that owns their
        // underlying memory.
        vmaDestroyBuffer(allocator_, vertexBuffer_, vertexBufferAllocation_);
        vmaDestroyBuffer(allocator_, indexBuffer_, indexBufferAllocation_);
        vmaDestroyAllocator(allocator_);

        vkb::destroy_device(vkbDevice_);
        vkb::destroy_surface(vkbInstance_, surface_);
        vkb::destroy_instance(vkbInstance_);

        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    // ---- State ---------------------------------------------------

    GLFWwindow* window_ = nullptr;
    bool framebufferResized_ = false;

    vkb::Instance vkbInstance_;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    vkb::Device vkbDevice_;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = 0;
    VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;

    vkb::Swapchain vkbSwapchain_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthImageAllocation_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    VkImage colorImage_ = VK_NULL_HANDLE;
    VmaAllocation colorImageAllocation_ = VK_NULL_HANDLE;
    VkImageView colorImageView_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;

    VkPipelineLayout spherePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline spherePipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout orbitPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline orbitPipeline_ = VK_NULL_HANDLE;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation vertexBufferAllocation_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation indexBufferAllocation_ = VK_NULL_HANDLE;
    uint32_t sphereIndexCount_ = 0;

    std::vector<RenderBody> bodies_;

    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkBuffer> bodyUboBuffers_;
    std::vector<VmaAllocation> bodyUboAllocations_;
    std::vector<void*> bodyUboMapped_;
    std::vector<VkDescriptorSet> bodyDescriptorSets_;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    size_t currentFrame_ = 0;
};

int main()
{
    VulkanViewerApp app;
    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "orbit-viewer-vk error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
