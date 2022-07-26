#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "core/container.hpp"
#include "graphics/resource.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class BufferVulkan final : public Buffer {
public:
    BufferVulkan(Ref<class DeviceVulkan> device, const BufferDesc &desc);
    ~BufferVulkan() override;

    void *Map() override;

    void Unmap() override;

    uint64_t Size() const { return size_; }

    VkBuffer Raw() const { return buffer_; }

private:
    Ref<DeviceVulkan> device_;
    VkBuffer buffer_;
    VmaAllocation allocation_;
    uint64_t size_;

    void *mapped_ptr_ = nullptr;
    bool persistently_mapped_ = false;
};

struct TextureViewVulkanDesc {
    VkImageViewType type;
    VkFormat format;
    uint32_t base_layer = 0;
    uint32_t layers = 0;
    uint32_t base_level = 0;
    uint32_t levels = 0;

    bool operator==(const TextureViewVulkanDesc &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template<>
struct std::hash<bismuth::gfx::TextureViewVulkanDesc> {
    size_t operator()(const bismuth::gfx::TextureViewVulkanDesc &v) const noexcept {
        return bismuth::Hash(v.type, v.format, v.base_layer, v.layers, v.base_level, v.levels);
    }
};

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class TextureVulkan final : public Texture {
public:
    TextureVulkan(Ref<class DeviceVulkan> device, const TextureDesc &desc);
    // external image
    TextureVulkan(Ref<class DeviceVulkan> device, VkImage raw_image, const TextureDesc &desc);
    ~TextureVulkan() override;

    VkImage Raw() const { return image_; }

    VkImageView GetView(const TextureViewVulkanDesc &view_desc) const;

    VkFormat RawFormat() const;

    VkImageAspectFlags GetAspect() const;

    void GetDepthAndLayer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers, uint32_t another = 1) const;

private:
    Ref<DeviceVulkan> device_;
    VkImage image_;
    VmaAllocation allocation_;

    mutable HashMap<TextureViewVulkanDesc, VkImageView> cached_views_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
