// orbit-viewer-vk — Vulkan #70 "Boot" + #71 "First pipeline + triangle" +
// #72 "Sphere mesh: vertex/index buffers via VMA, MVP uniforms".
//
// #70 opened a window and cleared it to a solid color every frame, with a
// swapchain that survives resize, and clean shutdown. #71 added the graphics
// pipeline object and rendered a single hardcoded triangle. #72 replaces
// that triangle with a real UV-sphere mesh: vertex/index data uploaded to
// device-local GPU memory via VMA (through a staging-buffer copy), and an
// MVP matrix pushed to the vertex shader every frame via push constants
// (src/viewer_vk/shaders/sphere.{vert,frag}).
//
// Compared to the OpenGL viewer, nothing here is implicit. OpenGL hides a
// global state machine behind you; every object below (instance, device,
// swapchain, render pass, pipeline, buffers, framebuffers, command buffers,
// sync objects) is something *you* create, configure, and destroy by hand,
// in a specific order. That's the whole point of the exercise.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{

constexpr uint32_t kInitialWidth = 1280;
constexpr uint32_t kInitialHeight = 720;
constexpr int kMaxFramesInFlight = 2;

constexpr float kSphereRadius = 1.0f;
constexpr int kSphereSegments = 32;
constexpr int kSphereRings = 16;

// Position + normal, read from a real vertex buffer this milestone instead
// of being hardcoded inside the shader (see #71's triangle for the
// hardcoded version this replaces).
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

struct PushConstants
{
    glm::mat4 mvp;
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
    // Order matters and mirrors the dependency chain:
    //   instance -> surface -> physical device -> logical device + queues
    //   -> allocator -> swapchain -> render pass -> pipeline -> framebuffers
    //   -> command pool/buffers -> mesh buffers -> sync objects.
    // Mesh buffers need the command pool (for the staging-copy command
    // buffer) and the allocator (for the buffers themselves), so they have
    // to come after both.

    void initVulkan()
    {
        createInstance();
        createSurface();
        pickPhysicalDeviceAndCreateDevice();
        createAllocator();
        createSwapchain();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPoolAndBuffers();
        createMeshBuffers();
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
        vkb::PhysicalDeviceSelector selector{vkbInstance_};
        auto physRet = selector.set_surface(surface_).set_minimum_version(1, 2).select();
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

    void createRenderPass()
    {
        // A render pass describes *what kind* of attachments a frame uses and
        // how their contents transition across the frame (load, store, and the
        // image layout before/after) — not the actual pixels, just the plan.
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainImageFormat_;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // The implicit "external" subpass boundary needs an explicit dependency
        // telling the GPU not to start the color-attachment write stage until
        // the swapchain image is actually available (signaled by the
        // image-available semaphore in drawFrame()).
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
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

    void createGraphicsPipeline()
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

        // ---- Input assembly: how to group the 3 vertices Vulkan feeds the
        // vertex shader. TRIANGLE_LIST = every 3 vertices forms one
        // independent triangle (vs. TRIANGLE_STRIP, LINE_LIST, POINT_LIST...).
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
        // vertices only) — useful to know, that's a one-line swap to see the
        // wireframe later if you're curious.
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        // CCW here (vs. #71's flat hardcoded triangle, which needed
        // CLOCKWISE) because computeMvp()'s projection matrix flips clip-
        // space Y to correct for GLM assuming OpenGL's Y-up convention —
        // that flip mirrors the apparent winding of every triangle, so the
        // "front face" definition has to flip along with it. If the sphere
        // renders inside-out (you'd see the far side, culled near side), this
        // is the first thing to check.
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // ---- Multisampling: disabled (1 sample/pixel, no MSAA yet).
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // ---- Color blending: no blending, just overwrite the framebuffer
        // pixel with whatever the fragment shader outputs (opaque triangle).
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // ---- Pipeline layout: describes what external resources (uniform
        // buffers, textures, push constants) the shaders can access. This
        // milestone adds one push-constant range for the MVP matrix, visible
        // only to the vertex stage (matches sphere.vert's `layout(push_constant)`
        // block) — still no descriptor sets, that's for a real uniform
        // buffer / texture later in the epic.
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout");
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
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &graphicsPipeline_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline");
        }

        // Shader modules are only needed during pipeline creation above — the
        // driver has already consumed and compiled the bytecode by this
        // point, so these can be destroyed immediately rather than kept
        // around for the app's lifetime.
        vkDestroyShaderModule(device_, fragModule, nullptr);
        vkDestroyShaderModule(device_, vertModule, nullptr);
    }

    void createFramebuffers()
    {
        // A framebuffer binds concrete image views to a render pass's attachment
        // slots. We need one per swapchain image, since each is a distinct
        // image the GPU might be rendering into at any given time.
        framebuffers_.resize(swapchainImageViews_.size());
        for (size_t i = 0; i < swapchainImageViews_.size(); ++i)
        {
            VkImageView attachments[] = {swapchainImageViews_[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass_;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
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
    // Overkill for a mesh this size, but it's the pattern that stays correct
    // once vertex data is large or updates frequently — the acceptance
    // criteria for #72 asks for it explicitly for that reason.
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
        // Framebuffers reference the old image views; render pass is unaffected
        // since it doesn't depend on the swapchain's chosen format changing
        // (safe assumption on this hardware/platform — a truly format-agnostic
        // recreation would also recreate the render pass).
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

    glm::mat4 computeMvp()
    {
        // Slow rotation stands in for real input/camera control, which is
        // #74's job — this only exists to prove the MVP matrix is actually
        // recomputed and pushed fresh every frame, not just uploaded once.
        float time = static_cast<float>(glfwGetTime());
        glm::mat4 model =
            glm::rotate(glm::mat4(1.0f), time * glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::vec3 cameraPos(0.0f, 1.5f, 4.0f);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        float aspect = static_cast<float>(swapchainExtent_.width) /
                       static_cast<float>(swapchainExtent_.height);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        // GLM was designed for OpenGL's clip space (Y up, matching the
        // triangle-vertex convention from #71 that Vulkan itself does NOT
        // follow — Vulkan's is Y down). This is the standard fix: flip the
        // single matrix entry that controls clip-space Y scale, rather than
        // hand-writing a Vulkan-specific projection matrix from scratch.
        proj[1][1] *= -1.0f;

        return proj * view * model;
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        VkClearValue clearColor{{{0.05f, 0.07f, 0.12f, 1.0f}}};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = framebuffers_[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent_;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

        // Viewport/scissor were declared dynamic in the pipeline, so they
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

        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

        PushConstants pushConstants{computeMvp()};
        vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstants), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, sphereIndexCount_, 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer");
        }
    }

    void drawFrame()
    {
        // Wait for this frame-in-flight slot's previous submission to finish
        // before reusing its command buffer.
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

        // renderFinishedSemaphores_ is already destroyed above, inside
        // cleanupSwapchain() -> destroyRenderFinishedSemaphores().
        for (int i = 0; i < kMaxFramesInFlight; ++i)
        {
            vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
            vkDestroyFence(device_, inFlightFences_[i], nullptr);
        }
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        vkDestroyRenderPass(device_, renderPass_, nullptr);

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

    vkb::Swapchain vkbSwapchain_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation vertexBufferAllocation_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation indexBufferAllocation_ = VK_NULL_HANDLE;
    uint32_t sphereIndexCount_ = 0;

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
