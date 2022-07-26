#include "shader_compiler.hpp"

#include <core/module_manager.hpp>

#include "dxc_helper.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

const wchar_t *ToDxShaderStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::eVertex: return L"vs_6_5";
        case ShaderStage::eTessellationControl: return L"hs_6_5";
        case ShaderStage::eTessellationEvaluation: return L"ds_6_5";
        case ShaderStage::eGeometry: return L"gs_6_5";
        case ShaderStage::eFragment: return L"ps_6_5";
        case ShaderStage::eCompute: return L"cs_6_5";
    }
    Unreachable();
}

}

ShaderCompilerD3D12::ShaderCompilerD3D12() {
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));

    DxcHelper::Instance().Utils()->CreateDefaultIncludeHandler(&include_handler_);
}

ShaderCompilerD3D12::~ShaderCompilerD3D12() {}

Vec<uint8_t> ShaderCompilerD3D12::Compile(const fs::path &src_path, const std::string &entry, ShaderStage stage,
    const HashMap<std::string, std::string> &defines, const Vec<fs::path> &include_dirs) const {
    std::string src_filename = src_path.string();
    if (!fs::exists(src_path)) {
        BI_CRTICAL(ModuleManager::Get<GraphicsModule>()->Lgr(), "Shader file '{}' doesn't exist", src_filename);
    }

    std::wstring src_filename_w = CharsToWString(src_filename.c_str());
    std::wstring entry_w = CharsToWString(entry.c_str());

    Vec<std::wstring> defines_w;
    defines_w.reserve(defines.size());
    for (const auto &[key, value] : defines) {
        if (value.empty()) {
            defines_w.push_back(CharsToWString(key.c_str()));
        } else {
            std::string temp = key + "=" + value;
            defines_w.push_back(CharsToWString(temp.c_str()));
        }
    }
    Vec<std::wstring> includes_w;
    includes_w.reserve(include_dirs.size());
    for (const auto &dir : include_dirs) {
        auto dir_str = dir.string();
        includes_w.push_back(CharsToWString(dir_str.c_str()));
    }

    Vec<const wchar_t *> args = {
        src_filename_w.c_str(),
        L"-E", entry_w.c_str(),
        L"-T", ToDxShaderStage(stage),
#ifndef BI_DEBUG_MODE
        L"-Qstrip_debug",
        L"-Qstrip_reflect",
#endif
    };
    args.reserve(args.size() + defines_w.size() * 2 + includes_w.size() * 2);
    for (const auto &define_w : defines_w) {
        args.push_back(L"-D");
        args.push_back(define_w.c_str());
    }
    for (const auto &include_w : includes_w) {
        args.push_back(L"-I");
        args.push_back(include_w.c_str());
    }

    ComPtr<IDxcBlobEncoding> shader_source = nullptr;
    DxcHelper::Instance().Utils()->LoadFile(src_filename_w.c_str(), nullptr, &shader_source);
    DxcBuffer shader_source_buf {
        .Ptr = shader_source->GetBufferPointer(),
        .Size = shader_source->GetBufferSize(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<IDxcResult> result;
    compiler_->Compile(&shader_source_buf, args.data(), args.size(), include_handler_.Get(), IID_PPV_ARGS(&result));

    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    HRESULT status;
    result->GetStatus(&status);
    if (FAILED(status)) {
        BI_CRTICAL(ModuleManager::Get<GraphicsModule>()->Lgr(),
            "Failed to compile shader '{}' (entry point '{}'), info:\n{}",
            src_filename, entry, errors->GetStringPointer());
    }

    ComPtr<IDxcBlob> dxil_binary;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxil_binary), nullptr);

    Vec<uint8_t> dxil_binary_bytes(dxil_binary->GetBufferSize());
    memcpy(dxil_binary_bytes.data(), dxil_binary->GetBufferPointer(), dxil_binary->GetBufferSize());
    return dxil_binary_bytes;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
