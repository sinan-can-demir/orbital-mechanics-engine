// orbit-viewer-vk — Vulkan #70 "Boot" milestone.
//
// Goal: open a window and clear it to a solid color every frame, with a
// swapchain that survives resize, and clean shutdown. No geometry yet —
// that starts at milestone #71 (first pipeline + triangle).
//
// Compared to the OpenGL viewer, nothing here is implicit. OpenGL hides a
// global state machine behind you; every object below (instance, device,
// swapchain, render pass, framebuffers, command buffers, sync objects) is
// something *you* create, configure, and destroy by hand, in a specific
// order. That's the whole point of the exercise.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

constexpr uint32_t kInitialWidth = 1280;
constexpr uint32_t kInitialHeight = 720;
constexpr int kMaxFramesInFlight = 2;

#ifdef ORBIT_VK_VALIDATION_LAYERS
constexpr bool kEnableValidationLayers = true;
#else
constexpr bool kEnableValidationLayers = false;
#endif

}  // namespace

class VulkanViewerApp {
 public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

 private:
  // ---- GLFW window ---------------------------------------------------

  void initWindow() {
    glfwInit();
    // GLFW defaults to creating an OpenGL context; tell it not to — Vulkan
    // manages its own surface/swapchain instead of a GL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(kInitialWidth, kInitialHeight,
                                "orbit-viewer-vk", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
  }

  static void framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto* app =
        reinterpret_cast<VulkanViewerApp*>(glfwGetWindowUserPointer(window));
    app->framebufferResized_ = true;
  }

  // ---- Vulkan bring-up -------------------------------------------------
  //
  // Order matters and mirrors the dependency chain:
  //   instance -> surface -> physical device -> logical device + queues
  //   -> swapchain -> render pass -> framebuffers -> command pool/buffers
  //   -> sync objects.
  // Each stage needs the one before it to exist.

  void initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDeviceAndCreateDevice();
    createSwapchain();
    createRenderPass();
    createFramebuffers();
    createCommandPoolAndBuffers();
    createSyncObjects();
  }

  void createInstance() {
    vkb::InstanceBuilder builder;
    auto instRet = builder.set_app_name("orbit-viewer-vk")
                       .request_validation_layers(kEnableValidationLayers)
                       .use_default_debug_messenger()
                       .require_api_version(1, 2, 0)
                       .build();
    if (!instRet) {
      throw std::runtime_error("failed to create Vulkan instance: " +
                                instRet.error().message());
    }
    vkbInstance_ = instRet.value();
    instance_ = vkbInstance_.instance;
    debugMessenger_ = vkbInstance_.debug_messenger;
  }

  void createSurface() {
    // The surface is the one part of this whole app that's platform-specific
    // (Vulkan itself knows nothing about windows). GLFW hides the
    // Win32/Xlib/Wayland/Cocoa details behind this single call.
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface");
    }
  }

  void pickPhysicalDeviceAndCreateDevice() {
    // Physical device = an actual GPU in the machine. Logical device (below)
    // = your application's private handle to it, through which every other
    // Vulkan call is dispatched.
    vkb::PhysicalDeviceSelector selector{vkbInstance_};
    auto physRet = selector.set_surface(surface_)
                       .set_minimum_version(1, 2)
                       .select();
    if (!physRet) {
      throw std::runtime_error("failed to select Vulkan physical device: " +
                                physRet.error().message());
    }
    vkb::PhysicalDevice vkbPhysicalDevice = physRet.value();

    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
    auto devRet = deviceBuilder.build();
    if (!devRet) {
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
    if (!graphicsQueueRet || !presentQueueRet) {
      throw std::runtime_error("failed to get graphics/present queues");
    }
    graphicsQueue_ = graphicsQueueRet.value();
    presentQueue_ = presentQueueRet.value();
    graphicsQueueFamily_ =
        vkbDevice_.get_queue_index(vkb::QueueType::graphics).value();
  }

  void createSwapchain() {
    // The swapchain is a ring of images the GPU renders into and the
    // presentation engine (compositor/display) shows on screen. You never
    // render "directly to the screen" in Vulkan — you render to one of
    // these images, then hand it back for presentation.
    vkb::SwapchainBuilder swapchainBuilder{vkbDevice_};
    auto swapRet = swapchainBuilder.set_old_swapchain(vkbSwapchain_)
                        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                        .build();
    if (!swapRet) {
      throw std::runtime_error("failed to create swapchain: " +
                                swapRet.error().message());
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

  void createRenderPass() {
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

    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create render pass");
    }
  }

  void createFramebuffers() {
    // A framebuffer binds concrete image views to a render pass's attachment
    // slots. We need one per swapchain image, since each is a distinct
    // image the GPU might be rendering into at any given time.
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
      VkImageView attachments[] = {swapchainImageViews_[i]};

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderPass_;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = swapchainExtent_.width;
      framebufferInfo.height = swapchainExtent_.height;
      framebufferInfo.layers = 1;

      if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr,
                               &framebuffers_[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer");
      }
    }
  }

  void createCommandPoolAndBuffers() {
    // A command pool allocates command buffers from a specific queue
    // family. VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT lets us
    // vkResetCommandBuffer() individual buffers each frame instead of
    // resetting the whole pool.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily_;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create command pool");
    }

    commandBuffers_.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;

    if (vkAllocateCommandBuffers(device_, &allocInfo,
                                  commandBuffers_.data()) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate command buffers");
    }
  }

  void createSyncObjects() {
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

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
      if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                             &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
          vkCreateFence(device_, &fenceInfo, nullptr,
                        &inFlightFences_[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create sync objects for a frame");
      }
    }

    createRenderFinishedSemaphores();
  }

  void createRenderFinishedSemaphores() {
    // One per swapchain image, not per frame-in-flight — see the note in
    // createSyncObjects(). Recreated whenever the swapchain is (the image
    // count can change across recreation).
    renderFinishedSemaphores_.resize(swapchainImageViews_.size());
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& semaphore : renderFinishedSemaphores_) {
      if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore) !=
          VK_SUCCESS) {
        throw std::runtime_error("failed to create render-finished semaphore");
      }
    }
  }

  void destroyRenderFinishedSemaphores() {
    for (auto semaphore : renderFinishedSemaphores_) {
      vkDestroySemaphore(device_, semaphore, nullptr);
    }
    renderFinishedSemaphores_.clear();
  }

  // ---- Swapchain recreation ---------------------------------------------

  void cleanupSwapchain() {
    destroyRenderFinishedSemaphores();
    for (auto framebuffer : framebuffers_) {
      vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();
    vkbSwapchain_.destroy_image_views(swapchainImageViews_);
    swapchainImageViews_.clear();
  }

  void recreateSwapchain() {
    // Minimizing the window gives a 0x0 framebuffer size, which Vulkan
    // rejects — just block until the window has real dimensions again
    // rather than spin.
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
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

  void mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      drawFrame();
    }
    vkDeviceWaitIdle(device_);
  }

  void recordCommandBuffer(VkCommandBuffer commandBuffer,
                            uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
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

    // Everything between vkCmdBeginRenderPass and vkCmdEndRenderPass is
    // where actual draw calls will go, starting at milestone #71. For now
    // the clear (LOAD_OP_CLEAR above) is the entire frame's content.
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                          VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to record command buffer");
    }
  }

  void drawFrame() {
    // Wait for this frame-in-flight slot's previous submission to finish
    // before reusing its command buffer.
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE,
                     UINT64_MAX);

    uint32_t imageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapchain();
      return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("failed to acquire swapchain image");
    }

    // Only reset the fence once we know we're actually submitting work this
    // call — otherwise an early return above would leave it permanently
    // unsignaled.
    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
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

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo,
                       inFlightFences_[currentFrame_]) != VK_SUCCESS) {
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

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR || framebufferResized_) {
      framebufferResized_ = false;
      recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
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

  void cleanup() {
    cleanupSwapchain();
    vkb::destroy_swapchain(vkbSwapchain_);

    // renderFinishedSemaphores_ is already destroyed above, inside
    // cleanupSwapchain() -> destroyRenderFinishedSemaphores().
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
      vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
      vkDestroyFence(device_, inFlightFences_[i], nullptr);
    }
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    vkDestroyRenderPass(device_, renderPass_, nullptr);

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
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers_;

  std::vector<VkSemaphore> imageAvailableSemaphores_;
  std::vector<VkSemaphore> renderFinishedSemaphores_;
  std::vector<VkFence> inFlightFences_;
  size_t currentFrame_ = 0;
};

int main() {
  VulkanViewerApp app;
  try {
    app.run();
  } catch (const std::exception& e) {
    std::cerr << "orbit-viewer-vk error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
