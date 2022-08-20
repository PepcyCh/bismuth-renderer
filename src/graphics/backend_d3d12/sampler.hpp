#pragma once

#include "graphics/sampler.hpp"
#include "descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class SamplerD3D12 : public Sampler {
public:
    SamplerD3D12(Ref<class DeviceD3D12> device, const SamplerDesc &desc);
    ~SamplerD3D12() override;

    DescriptorHandle GetDescriptorHandle() const { return descriptor_handle_; }

private:
    Ref<DeviceD3D12> device_;
    DescriptorHandle descriptor_handle_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
