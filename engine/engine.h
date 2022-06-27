#pragma once

#include <vector>
#include <array>
#include <functional>

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

class Window {
public:
    void Create();
    void Destroy();

    glm::ivec2 GetSize() const;
    glm::ivec2 GetDrawSize() const;

    std::vector<const char*> GetExtensions() const;
    VkSurfaceKHR GenSurface(const VkInstance& inst) const;

private:
    SDL_Window* window_{ nullptr };
};

enum QueueType : uint32_t {
    eGraphics,
    ePresent,
    eMaxQueue,
};

class Engine {
public:
    
    void Create();
    void Destroy();

private:
    VkInstance instance_{};
    VkDebugUtilsMessengerEXT debug_messenger_{};
    VkPhysicalDevice gpu_{};
    VkSurfaceKHR surface_{};
    VkDevice device_{};
    std::array<uint32_t, QueueType::eMaxQueue> queue_indices;
    std::array<VkQueue, QueueType::eMaxQueue> queues{};
    VkSwapchainKHR swapchain_{};
    union {
        uint32_t framebuffer_count_{ 0 };
        uint32_t swapchain_image_count_;
    };
    uint32_t current_buffer_{ 0 };
    std::unique_ptr<VkImage[]> color_images_{};
    std::unique_ptr<VkImageView[]> color_imageviews_{};
    VkImage depth_image_{};
    VkDeviceMemory depth_memory_{};
    VkImageView depth_imageview_{};
    VkSemaphore image_available_semaphore_{};
    VkSemaphore render_finished_semaphore_{};
    
public:
    uint32_t queue_family_count{};
    std::unique_ptr<VkQueueFamilyProperties[]> queue_family_properties{};
    VkPhysicalDeviceProperties gpu_properties{};
    VkPhysicalDeviceMemoryProperties memory_properties{};
    VkFormat surface_format{ VK_FORMAT_UNDEFINED };
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    std::vector<VkPresentModeKHR> present_modes{};
    VkFormat depth_format{ VK_FORMAT_UNDEFINED };

private:
    bool GetMemoryType(uint32_t typeBits, VkFlags mask, uint32_t &typeIndex);

    void CreateInstance();
    void DestroyInstance();

    void SetupDebugMessenger();
    void UninstallDebugMessenger();

    void GetGpuInfo();

    void CreateSurface();
    void DestroySurface();

    void CreateDevice();
    void DestroyDevice();

    void GetQueue();

    void CreateSwapchain();
    void DestroySwapchain();

    void CreateDepthImage();
    void DestroyDepthImage();

    void CreateSemaphore();
    void DestroySemaphore();
};

class Buffer {
public:


private:
    VkDevice device_;
    uint32_t size_{ 0 };
    VkBufferUsageFlags usage_{ 0 };
    VkMemoryPropertyFlags memprop_{ 0 };
};

Window& GetWindow();
Engine& GetEngine();
