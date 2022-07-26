#include "pipeline.hpp"

#include "device.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

D3D12_DESCRIPTOR_RANGE_TYPE ToDxDescriptorRangeType(DescriptorType type) {
    switch (type) {
        case DescriptorType::eNone:
            Unreachable();
        case DescriptorType::eSampler:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case DescriptorType::eUniformBuffer:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case DescriptorType::eStorageBuffer:
        case DescriptorType::eSampledTexture:
        case DescriptorType::eStorageTexture:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case DescriptorType::eRWStorageBuffer:
        case DescriptorType::eRWStorageTexture:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    }
    Unreachable();
}

void CreateSignature(const PipelineLayout &rhi_layout, ID3D12Device2 *device, ComPtr<ID3D12RootSignature> &signature,
    bool allow_input_assembler) {
    Vec<D3D12_ROOT_PARAMETER1> root_params(rhi_layout.sets_layout.size()
        + (rhi_layout.push_constants_size > 0 ? 1 : 0));
    Vec<D3D12_STATIC_SAMPLER_DESC> static_samplers;

    size_t num_bindings = 0;
    for (const auto &rhi_bindings : rhi_layout.sets_layout) {
        num_bindings += std::count_if(rhi_bindings.bindings.begin(), rhi_bindings.bindings.end(),
            [](const gfx::DescriptorSetLayoutBinding &rhi_binding) {
                return rhi_binding.immutable_samplers.Empty() && rhi_binding.type != DescriptorType::eNone;
            });
    }
    Vec<D3D12_DESCRIPTOR_RANGE1> bindings_info(num_bindings);
    D3D12_DESCRIPTOR_RANGE1 *p_binding_info = bindings_info.data();

    for (size_t set = 0; set < rhi_layout.sets_layout.size(); set++) {
        const auto &rhi_bindings = rhi_layout.sets_layout[set];
        const auto p_binding_info_start = p_binding_info;
        UINT num_bindings = 0;
        for (size_t binding = 0; binding < rhi_bindings.bindings.size(); binding++) {
            const auto &rhi_binding = rhi_bindings.bindings[binding];
            if (rhi_binding.type == DescriptorType::eNone) {
                continue;
            }
            if (rhi_binding.immutable_samplers.Empty()) {
                *p_binding_info = D3D12_DESCRIPTOR_RANGE1 {
                    .RangeType = ToDxDescriptorRangeType(rhi_binding.type),
                    .NumDescriptors = rhi_binding.count,
                    .BaseShaderRegister = static_cast<UINT>(binding),
                    .RegisterSpace = static_cast<UINT>(set),
                    .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                    .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
                };
                ++p_binding_info;
                ++num_bindings;
            } else {
                for (const auto &sampler : rhi_binding.immutable_samplers) {
                    static_samplers.push_back(sampler.CastTo<SamplerD3D12>()->GetStaticSamplerDesc(binding, set));
                }
            }
        }

        root_params[set] = D3D12_ROOT_PARAMETER1 {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE1 {
                .NumDescriptorRanges = num_bindings,
                .pDescriptorRanges = p_binding_info_start,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };
    }

    if (rhi_layout.push_constants_size > 0) {
        root_params[root_params.size() - 1] = D3D12_ROOT_PARAMETER1 {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = D3D12_ROOT_CONSTANTS {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .Num32BitValues = (rhi_layout.push_constants_size + 3) / 4,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };
    }
    
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC signature_desc {
        .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
        .Desc_1_1 = D3D12_ROOT_SIGNATURE_DESC1 {
            .NumParameters = static_cast<UINT>(root_params.size()),
            .pParameters = root_params.data(),
            .NumStaticSamplers = static_cast<UINT>(static_samplers.size()),
            .pStaticSamplers = static_samplers.data(),
            .Flags = allow_input_assembler
                ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE,
        },
    };
    ComPtr<ID3DBlob> serialized_signature;
    ComPtr<ID3DBlob> error;
    D3D12SerializeVersionedRootSignature(&signature_desc, &serialized_signature, &error);
    device->CreateRootSignature(0, serialized_signature->GetBufferPointer(), serialized_signature->GetBufferSize(),
        IID_PPV_ARGS(&signature));
}

D3D12_BLEND_OP ToDxBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::eAdd: return D3D12_BLEND_OP_ADD;
        case BlendOp::eSubtract: return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::eReserveSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::eMin: return D3D12_BLEND_OP_MIN;
        case BlendOp::eMax: return D3D12_BLEND_OP_MAX;
    }
    Unreachable();
}

D3D12_BLEND ToDxBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::eZero: return D3D12_BLEND_ZERO;
        case BlendFactor::eOne: return D3D12_BLEND_ONE;
        case BlendFactor::eSrc: return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::eOneMinusSrc: return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::eSrcAlpha: return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::eOneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::eDst: return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::eOneMinusDst: return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::eDstAlpha: return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::eOneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::eSrcAlphaSaturated: return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::eConstant: return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::eOneMinusConstant: return D3D12_BLEND_INV_BLEND_FACTOR;
    }
    Unreachable();
}

D3D12_FILL_MODE ToDxFillMode(PolygonMode poly) {
    switch (poly) {
        case PolygonMode::eFill: return D3D12_FILL_MODE_SOLID;
        case PolygonMode::eLine: return D3D12_FILL_MODE_WIREFRAME;
        case PolygonMode::ePoint: return D3D12_FILL_MODE_WIREFRAME;
    }
    Unreachable();
}

D3D12_CULL_MODE ToDxCullMode(CullMode cull) {
    switch (cull) {
        case CullMode::eNone: return D3D12_CULL_MODE_NONE;
        case CullMode::eBackFace: return D3D12_CULL_MODE_BACK;
        case CullMode::eFrontFace: return D3D12_CULL_MODE_FRONT;
    }
    Unreachable();
}

D3D12_STENCIL_OP ToDxStencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::eKeep: return D3D12_STENCIL_OP_KEEP;
        case StencilOp::eZero: return D3D12_STENCIL_OP_ZERO;
        case StencilOp::eReplace: return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::eIncrementClamp: return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::eDecrementClamp: return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::eInvert: return D3D12_STENCIL_OP_INVERT;
        case StencilOp::eIncrementWrap: return D3D12_STENCIL_OP_INCR;
        case StencilOp::eDecrementWrap: return D3D12_STENCIL_OP_DECR;
    }
    Unreachable();
}

LPCSTR ToDxSemanticName(VertexSemantics semantic) {
    switch (semantic) {
        case VertexSemantics::ePosition: return "POSITION";
        case VertexSemantics::eColor: return "COLOR";
        case VertexSemantics::eNormal: return "NORMAL";
        case VertexSemantics::eTangent: return "TANGENT";
        case VertexSemantics::eBitangent: return "BINORMAL";
        case VertexSemantics::eTexcoord0:
        case VertexSemantics::eTexcoord1:
        case VertexSemantics::eTexcoord2:
        case VertexSemantics::eTexcoord3:
        case VertexSemantics::eTexcoord4:
        case VertexSemantics::eTexcoord5:
        case VertexSemantics::eTexcoord6:
        case VertexSemantics::eTexcoord7:
            return "TEXCOORD";
    }
    Unreachable();
}
UINT ToDxSemanticIndex(VertexSemantics semantic) {
    int semantic_raw = static_cast<int>(semantic);
    return std::max(0, semantic_raw - static_cast<int>(VertexSemantics::eTexcoord0));
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDxPrimitiveTopologyType(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::ePointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveTopology::eLineList:
        case PrimitiveTopology::eLineStrip:
        case PrimitiveTopology::eLineListAdjacency:
        case PrimitiveTopology::eLineStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopology::eTriangleList:
        case PrimitiveTopology::eTriangleStrip:
        case PrimitiveTopology::eTriangleListAdjacency:
        case PrimitiveTopology::eTriangleStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case PrimitiveTopology::ePatchList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    }
    Unreachable();
}

D3D12_PRIMITIVE_TOPOLOGY ToDxPrimitiveTopology(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::ePointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::eLineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::eLineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::eLineListAdjacency: return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
        case PrimitiveTopology::eLineStripAdjacency: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
        case PrimitiveTopology::eTriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::eTriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveTopology::eTriangleListAdjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case PrimitiveTopology::eTriangleStripAdjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
        // TODO - support tessellation
        case PrimitiveTopology::ePatchList: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
    }
    Unreachable();
}

}

RenderPipelineD3D12::RenderPipelineD3D12(Ref<DeviceD3D12> device, const RenderPipelineDesc &desc)
    : device_(device), desc_(desc) {
    CreateSignature(desc.layout, device->Raw(), root_signature_, true);
}

void RenderPipelineD3D12::SetTargetFormats(Span<ResourceFormat> color_formats, ResourceFormat depth_stencil_format) {
    bool need_to_rebuild = pipeline_ == nullptr;

    for (size_t i = 0; i < std::min(color_formats.Size(), desc_.color_target_state.attachments.size()); i++) {
        need_to_rebuild |= desc_.color_target_state.attachments[i].format != color_formats[i];
        desc_.color_target_state.attachments[i].format = color_formats[i];
    }
    need_to_rebuild |= desc_.depth_stencil_state.format != depth_stencil_format;
    desc_.depth_stencil_state.format = depth_stencil_format;

    if (need_to_rebuild) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc {};

        {
            auto shader_vk = desc_.shaders.vertex.CastTo<ShaderModuleD3D12>();
            pipeline_desc.VS = shader_vk->RawBytecode();
        }
        if (desc_.shaders.tessellation_control) {
            auto shader_vk = static_cast<ShaderModuleD3D12 *>(desc_.shaders.tessellation_control);
            pipeline_desc.DS = shader_vk->RawBytecode();
        }
        if (desc_.shaders.tessellation_evaluation) {
            auto shader_vk = static_cast<ShaderModuleD3D12 *>(desc_.shaders.tessellation_evaluation);
            pipeline_desc.HS = shader_vk->RawBytecode();
        }
        if (desc_.shaders.geometry) {
            auto shader_vk = static_cast<ShaderModuleD3D12 *>(desc_.shaders.geometry);
            pipeline_desc.GS = shader_vk->RawBytecode();
        }
        {
            auto shader_vk = desc_.shaders.fragment.CastTo<ShaderModuleD3D12>();
            pipeline_desc.PS = shader_vk->RawBytecode();
        }

        pipeline_desc.pRootSignature = root_signature_.Get();

        pipeline_desc.NumRenderTargets = desc_.color_target_state.attachments.size();
        for (size_t i = 0; i < desc_.color_target_state.attachments.size(); i++) {
            const auto &attachment = desc_.color_target_state.attachments[i];
            pipeline_desc.RTVFormats[i] = ToDxFormat(attachment.format);
            pipeline_desc.BlendState.RenderTarget[i] = D3D12_RENDER_TARGET_BLEND_DESC {
                .BlendEnable = attachment.blend_enable,
                .LogicOpEnable = false,
                .SrcBlend = ToDxBlendFactor(attachment.src_blend_factor),
                .DestBlend = ToDxBlendFactor(attachment.dst_blend_factor),
                .BlendOp = ToDxBlendOp(attachment.blend_op),
                .SrcBlendAlpha = ToDxBlendFactor(attachment.src_alpha_blend_factor),
                .DestBlendAlpha = ToDxBlendFactor(attachment.dst_alpha_blend_factor),
                .BlendOpAlpha = ToDxBlendOp(attachment.alpha_blend_op),
                .LogicOp = D3D12_LOGIC_OP_NOOP,
                .RenderTargetWriteMask = attachment.color_write_mask.RawValue(),
            };
        }

        pipeline_desc.SampleMask = ~0u;

        pipeline_desc.RasterizerState = D3D12_RASTERIZER_DESC {
            .FillMode = ToDxFillMode(desc_.primitive_state.polygon_mode),
            .CullMode = ToDxCullMode(desc_.primitive_state.cull_mode),
            .FrontCounterClockwise = desc_.primitive_state.front_face == FrontFace::eCcw,
            .DepthBias = 0,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 0.0f,
            .DepthClipEnable = false,
            .MultisampleEnable = false,
            .AntialiasedLineEnable = false,
            .ForcedSampleCount = 0,
            .ConservativeRaster = desc_.primitive_state.conservative ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON
                : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
        };

        pipeline_desc.DepthStencilState = D3D12_DEPTH_STENCIL_DESC {
            .DepthEnable = desc_.depth_stencil_state.format == ResourceFormat::eUndefined ? false
                : desc_.depth_stencil_state.depth_test,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = ToDxCompareFunc(desc_.depth_stencil_state.depth_compare_op),
            .StencilEnable = desc_.depth_stencil_state.stencil_test,
            .StencilReadMask = desc_.depth_stencil_state.stencil_compare_mask,
            .StencilWriteMask = desc_.depth_stencil_state.stencil_write_mask,
            .FrontFace = D3D12_DEPTH_STENCILOP_DESC {
                .StencilFailOp = ToDxStencilOp(desc_.depth_stencil_state.stencil_front_face.fail_op),
                .StencilDepthFailOp = ToDxStencilOp(desc_.depth_stencil_state.stencil_front_face.depth_fail_op),
                .StencilPassOp = ToDxStencilOp(desc_.depth_stencil_state.stencil_front_face.pass_op),
                .StencilFunc = ToDxCompareFunc(desc_.depth_stencil_state.stencil_front_face.compare_op),
            },
            .BackFace = D3D12_DEPTH_STENCILOP_DESC {
                .StencilFailOp = ToDxStencilOp(desc_.depth_stencil_state.stencil_back_face.fail_op),
                .StencilDepthFailOp = ToDxStencilOp(desc_.depth_stencil_state.stencil_back_face.depth_fail_op),
                .StencilPassOp = ToDxStencilOp(desc_.depth_stencil_state.stencil_back_face.pass_op),
                .StencilFunc = ToDxCompareFunc(desc_.depth_stencil_state.stencil_back_face.compare_op),
            },
        };
        pipeline_desc.DSVFormat = ToDxFormat(desc_.depth_stencil_state.format);

        Vec<D3D12_INPUT_ELEMENT_DESC> input_elements;
        for (size_t i = 0; i < desc_.vertex_input_buffers.size(); i++) {
            const auto &input_buffer = desc_.vertex_input_buffers[i];
            for (const auto &attribute : input_buffer.attributes) {
                uint32_t location = static_cast<uint32_t>(attribute.semantics);
                input_elements.push_back(D3D12_INPUT_ELEMENT_DESC {
                    .SemanticName = ToDxSemanticName(attribute.semantics),
                    .SemanticIndex = ToDxSemanticIndex(attribute.semantics),
                    .Format = ToDxFormat(attribute.format),
                    .InputSlot = static_cast<UINT>(i),
                    .AlignedByteOffset = attribute.offset,
                    .InputSlotClass = input_buffer.per_instance
                        ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    .InstanceDataStepRate = input_buffer.per_instance ? 1u : 0u,
                });
            }
        }
        pipeline_desc.InputLayout = D3D12_INPUT_LAYOUT_DESC {
            .pInputElementDescs = input_elements.data(),
            .NumElements = static_cast<UINT>(input_elements.size()),
        };

        pipeline_desc.PrimitiveTopologyType = ToDxPrimitiveTopologyType(desc_.primitive_state.topology);
        topology_ = ToDxPrimitiveTopology(desc_.primitive_state.topology);

        pipeline_desc.SampleDesc = DXGI_SAMPLE_DESC { .Count = 1, .Quality = 0 };

        pipeline_desc.NodeMask = 0;

        pipeline_desc.CachedPSO = {};

        pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        device_->Raw()->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));

        if (!desc_.name.empty()) {
            pipeline_->SetPrivateData(WKPDID_D3DDebugObjectName, desc_.name.size(), desc_.name.data());
        }
    }
}

RenderPipelineD3D12::~RenderPipelineD3D12() {}

ComputePipelineD3D12::ComputePipelineD3D12(Ref<DeviceD3D12> device, const ComputePipelineDesc &desc)
    : device_(device), desc_(desc) {
    auto shader_dx = desc.compute.CastTo<ShaderModuleD3D12>();

    CreateSignature(desc.layout, device->Raw(), root_signature_, false);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc {
        .pRootSignature = root_signature_.Get(),
        .CS = shader_dx->RawBytecode(),
        .NodeMask = 0,
        .CachedPSO = {},
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };
    device->Raw()->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));

    if (!desc.name.empty()) {
        pipeline_->SetPrivateData(WKPDID_D3DDebugObjectName, desc.name.size(), desc.name.data());
    }
}

ComputePipelineD3D12::~ComputePipelineD3D12() {}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
