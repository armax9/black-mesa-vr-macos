#pragma once

#include "d3d9_device_child.h"
#include "d3d9_device.h"
#include "d3d9_state.h"

#include "../util/util_bit.h"
#include "../util/util_env.h"
#include "../util/log/log.h"

#include <atomic>

namespace dxvk {

  enum class D3D9CapturedStateFlag : uint32_t {
    VertexDecl,
    Indices,
    RenderStates,
    SamplerStates,
    VertexBuffers,
    Textures,
    VertexShader,
    PixelShader,
    Viewport,
    ScissorRect,
    ClipPlanes,
    VsConstants,
    PsConstants,
    StreamFreq,
    Transforms,
    TextureStages,
    Material,
    Lights
  };

  using D3D9CapturedStateFlags = Flags<D3D9CapturedStateFlag>;

  struct D3D9StateCaptures {
    D3D9CapturedStateFlags flags;

    bit::bitset<RenderStateCount>                       renderStates;

    bit::bitset<SamplerCount>                           samplers;
    std::array<
      bit::bitset<SamplerStateCount>,
      SamplerCount>                                     samplerStates;

    bit::bitset<caps::MaxStreams>                       vertexBuffers;
    bit::bitset<SamplerCount>                           textures;
    bit::bitset<caps::MaxClipPlanes>                    clipPlanes;
    bit::bitset<caps::MaxStreams>                       streamFreq;
    bit::bitset<caps::MaxTransforms>                    transforms;
    bit::bitset<caps::TextureStageCount>                textureStages;
    std::array<
      bit::bitset<TextureStageStateCount>,
      caps::TextureStageCount>                          textureStageStates;

    struct {
      bit::bitset<caps::MaxFloatConstantsSoftware>      fConsts;
      bit::bitset<caps::MaxOtherConstantsSoftware>      iConsts;
      bit::bitset<caps::MaxOtherConstantsSoftware>      bConsts;
    } vsConsts;

    struct {
      bit::bitset<caps::MaxSM3FloatConstantsPS>         fConsts;
      bit::bitset<caps::MaxOtherConstants>              iConsts;
      bit::bitset<caps::MaxOtherConstants>              bConsts;
    } psConsts;

    bit::bitvector                                      lightEnabledChanges;
  };

  enum class D3D9StateBlockType : uint8_t {
    None,
    All,
    PixelState,
    VertexState,
    Unknown
  };

  inline D3D9StateBlockType ConvertStateBlockType(D3DSTATEBLOCKTYPE type) {
    switch (type) {
      case D3DSBT_ALL:         return D3D9StateBlockType::All;
      case D3DSBT_PIXELSTATE:  return D3D9StateBlockType::PixelState;
      case D3DSBT_VERTEXSTATE: return D3D9StateBlockType::VertexState;
      default:                 return D3D9StateBlockType::Unknown;
    }
  }

  using D3D9StateBlockBase = D3D9DeviceChild<IDirect3DStateBlock9>;
  class D3D9StateBlock : public D3D9StateBlockBase {

  public:

    D3D9StateBlock(D3D9DeviceEx* pDevice, D3D9StateBlockType Type);

    ~D3D9StateBlock();

    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID  riid,
        void** ppvObject) final;

    HRESULT STDMETHODCALLTYPE Capture() final;
    HRESULT STDMETHODCALLTYPE Apply() final;

    HRESULT SetVertexDeclaration(D3D9VertexDecl* pDecl);

    HRESULT SetIndices(D3D9IndexBuffer* pIndexData);

    HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);

    HRESULT SetStateSamplerState(
            DWORD               StateSampler,
            D3DSAMPLERSTATETYPE Type,
            DWORD               Value);

    HRESULT SetStreamSource(
            UINT               StreamNumber,
            D3D9VertexBuffer*  pStreamData,
            UINT               OffsetInBytes,
            UINT               Stride);

    HRESULT SetStreamSourceWithoutOffset(
            UINT               StreamNumber,
            D3D9VertexBuffer*  pStreamData,
            UINT               Stride);

    HRESULT SetStreamSourceFreq(UINT StreamNumber, UINT Setting);

    HRESULT SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture);

    HRESULT SetVertexShader(D3D9VertexShader* pShader);

    HRESULT SetPixelShader(D3D9PixelShader* pShader);

    HRESULT SetMaterial(const D3DMATERIAL9* pMaterial);

    HRESULT SetLight(DWORD Index, const D3DLIGHT9* pLight);

    HRESULT LightEnable(DWORD Index, BOOL Enable);

    HRESULT SetStateTransform(uint32_t idx, const D3DMATRIX* pMatrix);

    HRESULT SetStateTextureStageState(
            DWORD                      Stage,
            D3D9TextureStageStateTypes Type,
            DWORD                      Value);

    HRESULT SetViewport(const D3DVIEWPORT9* pViewport);

    HRESULT SetScissorRect(const RECT* pRect);

    HRESULT SetClipPlane(DWORD Index, const float* pPlane);


    HRESULT SetVertexShaderConstantF(
            UINT   StartRegister,
      const float* pConstantData,
            UINT   Vector4fCount);

    HRESULT SetVertexShaderConstantI(
            UINT StartRegister,
      const int* pConstantData,
            UINT Vector4iCount);

    HRESULT SetVertexShaderConstantB(
            UINT  StartRegister,
      const BOOL* pConstantData,
            UINT  BoolCount);


    HRESULT SetPixelShaderConstantF(
            UINT   StartRegister,
      const float* pConstantData,
            UINT   Vector4fCount);

    HRESULT SetPixelShaderConstantI(
            UINT StartRegister,
      const int* pConstantData,
            UINT Vector4iCount);

    HRESULT SetPixelShaderConstantB(
            UINT  StartRegister,
      const BOOL* pConstantData,
            UINT  BoolCount);

    enum class D3D9StateFunction {
      Apply,
      Capture
    };

    static bool CpuConstantBatchEnabled() {
      static const bool enabled = env::getEnvVar("BMVR_CPU_BATCH_DISABLE") != "1";
      return enabled;
    }

    struct ConstantBatchMetrics {
      uint64_t referenceCalls;
      uint64_t actualCalls;
      uint64_t actualRegisters;
    };

    static bool CpuConstantDiagnosticsEnabled() {
      static const bool enabled = env::getEnvVar("BMVR_STATEBLOCK_DIAG") == "1";
      return enabled;
    }

    template <typename Dst>
    static void LogConstantBatchMetrics(Dst* dst, const ConstantBatchMetrics& metrics) {
      // Count once per block, after its normal work. Reference and optimized
      // runs pay the same opt-in aggregation overhead. No counters in normal
      // gameplay; reporting is sparse and contains no per-register clocks.
      unsigned group = 0;
      if constexpr (std::is_same_v<Dst, D3D9DeviceEx>)
        group = dst->ShouldRecord() ? 3 : (dst->CanSWVP() ? 2 : 1);
      struct Totals {
        std::atomic<uint64_t> blocks{0};
        std::atomic<uint64_t> referenceCalls{0};
        std::atomic<uint64_t> actualCalls{0};
        std::atomic<uint64_t> actualRegisters{0};
      };
      static Totals groups[4];
      auto& totals = groups[group];
      totals.referenceCalls.fetch_add(metrics.referenceCalls, std::memory_order_relaxed);
      totals.actualCalls.fetch_add(metrics.actualCalls, std::memory_order_relaxed);
      totals.actualRegisters.fetch_add(metrics.actualRegisters, std::memory_order_relaxed);
      const uint64_t blocks = totals.blocks.fetch_add(1, std::memory_order_relaxed) + 1;
      if (blocks == 1 || (blocks & 255u) == 0) {
        static const char* const names[] = { "capture", "apply-hw", "apply-swvp", "record" };
        Logger::info(str::format("[BMVR stateblock] path=", names[group],
          " batching=", CpuConstantBatchEnabled() ? 1 : 0, " blocks=", blocks,
          " referenceSingleSetters=", totals.referenceCalls.load(std::memory_order_relaxed),
          " actualSetters=", totals.actualCalls.load(std::memory_order_relaxed),
          " actualRequestedRegisters=", totals.actualRegisters.load(std::memory_order_relaxed)));
      }
    }

    // A state block commonly captures hundreds of adjacent constants. Walking
    // them as Count=1 setters repeated validation, device locking and tiny
    // copies per register. Preserve sparse masks and register order, but issue
    // one setter for each contiguous included run. The disabled path is the
    // original per-register path for controlled CPU comparisons.
    template <bool Diagnostics, bool SingleRegister = false,
              typename Mask, typename Include, typename Set>
    static void ForEachConstantRangeImpl(Mask& mask, Include&& include, Set&& set,
                                        ConstantBatchMetrics* metrics) {
      const bool batch = !SingleRegister && CpuConstantBatchEnabled();
      uint32_t start = 0;
      uint32_t count = 0;
      const auto emit = [&](uint32_t idx, uint32_t n) {
        if constexpr (Diagnostics) {
          ++metrics->actualCalls;
          metrics->actualRegisters += n;
        }
        set(idx, n);
      };
      for (uint32_t word = 0; word < mask.dwordCount(); word++) {
        for (uint32_t bit : bit::BitMask(mask.dword(word))) {
          const uint32_t idx = word * 32 + bit;
          if constexpr (Diagnostics)
            ++metrics->referenceCalls;
          if (!batch) {
            emit(idx, 1);
            continue;
          }
          if (!include(idx)) {
            if (count)
              emit(start, count);
            count = 0;
            continue;
          }
          if (count && idx == start + count) {
            ++count;
          } else {
            if (count)
              emit(start, count);
            start = idx;
            count = 1;
          }
        }
      }
      if (count)
        emit(start, count);
    }

    template <typename Mask, typename Include, typename Set>
    static void ForEachConstantRange(Mask& mask, Include&& include, Set&& set,
                                    ConstantBatchMetrics* metrics = nullptr) {
      // Compile the per-register bookkeeping out of the ordinary path.
      if (metrics)
        ForEachConstantRangeImpl<true>(mask, include, set, metrics);
      else
        ForEachConstantRangeImpl<false>(mask, include, set, nullptr);
    }

    template <typename Dst, typename Mask, typename Include, typename Set>
    static void ForEachFloatConstantRange(Dst*, Mask& mask, Include&& include, Set&& set,
                                         ConstantBatchMetrics* metrics = nullptr) {
      if constexpr (std::is_same_v<Dst, D3D9DeviceEx>) {
        // The VR shim hooks the public Float setters. Its projection/size
        // rewrites depend on StartRegister and Count, so neither batching nor
        // comparing raw state here preserves that call-boundary behavior.
        // Keep Float Apply exactly as Count=1, including identical values.
        if (metrics)
          ForEachConstantRangeImpl<true, true>(mask, include, set, metrics);
        else
          ForEachConstantRangeImpl<false, true>(mask, include, set, nullptr);
      } else {
        // Capture writes to a StateBlock, not the hooked device setter.
        ForEachConstantRange(mask, include, set, metrics);
      }
    }

    template <DxsoProgramType ProgramType, D3D9ConstantType ConstantType,
              typename Dst, typename Src>
    static bool IncludeConstant(Dst* dst, const Src* src, uint32_t idx) {
      if constexpr (std::is_same_v<Dst, D3D9DeviceEx>
                    && ConstantType == D3D9ConstantType::Int) {
        // Apply() holds LockDevice and rejects state-block recording, but keep
        // that recording behavior even if this template is used elsewhere.
        // SWVP/mixed devices retain their original non-deduplicated semantics.
        if (dst->ShouldRecord() || dst->CanSWVP())
          return true;

        const auto& layout = ProgramType == DxsoProgramTypes::VertexShader
          ? dst->GetVertexConstantLayout() : dst->GetPixelConstantLayout();
        const uint32_t limit = layout.intCount;
        // The original device setter clips these to zero before accessing data.
        if (idx >= limit)
          return false;

        const auto different = [&](const auto& current, const auto& incoming) {
          return std::memcmp(current->iConsts[idx].data, incoming->iConsts[idx].data, sizeof(Vector4i)) != 0;
        };
        auto* current = dst->GetRawState();
        return ProgramType == DxsoProgramTypes::VertexShader
          ? different(current->vsConsts, src->vsConsts)
          : different(current->psConsts, src->psConsts);
      }
      return true;
    }

    template <typename Dst, typename Src, bool IgnoreStreamOffset>
    void ApplyOrCapture(Dst* dst, const Src* src) {
      ConstantBatchMetrics metrics;
      ConstantBatchMetrics* metricPtr = nullptr;
      if (CpuConstantDiagnosticsEnabled()) {
        metrics = {};
        metricPtr = &metrics;
      }
      if (m_captures.flags.test(D3D9CapturedStateFlag::StreamFreq)) {
        for (uint32_t idx : bit::BitMask(m_captures.streamFreq.dword(0)))
          dst->SetStreamSourceFreq(idx, src->streamFreq[idx]);
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Indices))
        dst->SetIndices(src->indices.ptr());

      if (m_captures.flags.test(D3D9CapturedStateFlag::RenderStates)) {
        for (uint32_t i = 0; i < m_captures.renderStates.dwordCount(); i++) {
          for (uint32_t rs : bit::BitMask(m_captures.renderStates.dword(i))) {
            uint32_t idx = i * 32 + rs;

            dst->SetRenderState(D3DRENDERSTATETYPE(idx), src->renderStates[idx]);
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::SamplerStates)) {
        for (uint32_t samplerIdx : bit::BitMask(m_captures.samplers.dword(0))) {
          for (uint32_t stateIdx : bit::BitMask(m_captures.samplerStates[samplerIdx].dword(0)))
            dst->SetStateSamplerState(samplerIdx, D3DSAMPLERSTATETYPE(stateIdx), src->samplerStates[samplerIdx][stateIdx]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VertexBuffers)) {
        for (uint32_t idx : bit::BitMask(m_captures.vertexBuffers.dword(0))) {
          const auto& vbo = src->vertexBuffers[idx];
          if constexpr (!IgnoreStreamOffset) {
            dst->SetStreamSource(
              idx,
              vbo.vertexBuffer.ptr(),
              vbo.offset,
              vbo.stride);
          } else {
            // For whatever reason, D3D9 doesn't capture the stream offset
            dst->SetStreamSourceWithoutOffset(
              idx,
              vbo.vertexBuffer.ptr(),
              vbo.stride);
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Material))
        dst->SetMaterial(&src->material);

      if (m_captures.flags.test(D3D9CapturedStateFlag::Textures)) {
        for (uint32_t idx : bit::BitMask(m_captures.textures.dword(0)))
          dst->SetStateTexture(idx, src->textures[idx]);
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VertexShader))
        dst->SetVertexShader(src->vertexShader.ptr());

      if (m_captures.flags.test(D3D9CapturedStateFlag::PixelShader))
        dst->SetPixelShader(src->pixelShader.ptr());

      if (m_captures.flags.test(D3D9CapturedStateFlag::Transforms)) {
        for (uint32_t i = 0; i < m_captures.transforms.dwordCount(); i++) {
          for (uint32_t trans : bit::BitMask(m_captures.transforms.dword(i))) {
            uint32_t idx = i * 32 + trans;

            dst->SetStateTransform(idx, reinterpret_cast<const D3DMATRIX*>(&src->transforms[idx]));
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::TextureStages)) {
        for (uint32_t stageIdx : bit::BitMask(m_captures.textureStages.dword(0))) {
          for (uint32_t stateIdx : bit::BitMask(m_captures.textureStageStates[stageIdx].dword(0)))
            dst->SetStateTextureStageState(stageIdx, D3D9TextureStageStateTypes(stateIdx), src->textureStages[stageIdx][stateIdx]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Viewport))
        dst->SetViewport(&src->viewport);

      if (m_captures.flags.test(D3D9CapturedStateFlag::ScissorRect))
        dst->SetScissorRect(&src->scissorRect);

      if (m_captures.flags.test(D3D9CapturedStateFlag::ClipPlanes)) {
        for (uint32_t idx : bit::BitMask(m_captures.clipPlanes.dword(0)))
          dst->SetClipPlane(idx, src->clipPlanes[idx].coeff);
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VsConstants)) {
        ForEachFloatConstantRange(dst, m_captures.vsConsts.fConsts,
          [&](uint32_t idx) { return IncludeConstant<DxsoProgramTypes::VertexShader, D3D9ConstantType::Float>(dst, src, idx); },
          [&](uint32_t idx, uint32_t count) {
            dst->SetVertexShaderConstantF(idx, reinterpret_cast<const float*>(&src->vsConsts->fConsts[idx]), count);
          }, metricPtr);

        ForEachConstantRange(m_captures.vsConsts.iConsts,
          [&](uint32_t idx) { return IncludeConstant<DxsoProgramTypes::VertexShader, D3D9ConstantType::Int>(dst, src, idx); },
          [&](uint32_t idx, uint32_t count) {
            dst->SetVertexShaderConstantI(idx, reinterpret_cast<const int*>(&src->vsConsts->iConsts[idx]), count);
          }, metricPtr);

        if (m_captures.vsConsts.bConsts.any()) {
          for (uint32_t i = 0; i < m_captures.vsConsts.bConsts.dwordCount(); i++)
            dst->SetVertexBoolBitfield(i, m_captures.vsConsts.bConsts.dword(i), src->vsConsts->bConsts[i]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::PsConstants)) {
        ForEachFloatConstantRange(dst, m_captures.psConsts.fConsts,
          [&](uint32_t idx) { return IncludeConstant<DxsoProgramTypes::PixelShader, D3D9ConstantType::Float>(dst, src, idx); },
          [&](uint32_t idx, uint32_t count) {
            dst->SetPixelShaderConstantF(idx, reinterpret_cast<const float*>(&src->psConsts->fConsts[idx]), count);
          }, metricPtr);

        ForEachConstantRange(m_captures.psConsts.iConsts,
          [&](uint32_t idx) { return IncludeConstant<DxsoProgramTypes::PixelShader, D3D9ConstantType::Int>(dst, src, idx); },
          [&](uint32_t idx, uint32_t count) {
            dst->SetPixelShaderConstantI(idx, reinterpret_cast<const int*>(&src->psConsts->iConsts[idx]), count);
          }, metricPtr);

        if (m_captures.psConsts.bConsts.any()) {
          for (uint32_t i = 0; i < m_captures.psConsts.bConsts.dwordCount(); i++)
            dst->SetPixelBoolBitfield(i, m_captures.psConsts.bConsts.dword(i), src->psConsts->bConsts[i]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Lights)) {
        for (uint32_t i = 0; i < src->lights.size(); i++) {
          if (!src->lights[i].isValid)
            continue;

          dst->SetLight(i, &src->lights[i].light);
        }

        for (uint32_t i = 0; i < m_captures.lightEnabledChanges.dwordCount(); i++) {
          for (uint32_t consts : bit::BitMask(m_captures.lightEnabledChanges.dword(i))) {
            uint32_t idx = i * 32 + consts;

            if (idx < src->lights.size())
              dst->LightEnable(idx, src->lights[idx].isEnabled);
          }
        }
      }
      if (metricPtr)
        LogConstantBatchMetrics(dst, metrics);
    }

    template <D3D9StateFunction Func, bool IgnoreStreamOffset>
    void ApplyOrCapture() {
      if      constexpr (Func == D3D9StateFunction::Apply)
        ApplyOrCapture<D3D9DeviceEx, D3D9CapturableState, IgnoreStreamOffset>(m_parent, &m_state);
      else if constexpr (Func == D3D9StateFunction::Capture)
        ApplyOrCapture<D3D9StateBlock, D3D9DeviceState, IgnoreStreamOffset>(this, m_deviceState);
    }

    template <
      DxsoProgramType  ProgramType,
      D3D9ConstantType ConstantType,
      typename         T>
    HRESULT SetShaderConstants(
            UINT  StartRegister,
      const T*    pConstantData,
            UINT  Count) {
      auto SetHelper = [&](auto& setCaptures) {
        if constexpr (ProgramType == DxsoProgramTypes::VertexShader)
          m_captures.flags.set(D3D9CapturedStateFlag::VsConstants);
        else
          m_captures.flags.set(D3D9CapturedStateFlag::PsConstants);

        for (uint32_t i = 0; i < Count; i++) {
          uint32_t reg = StartRegister + i;
          if      constexpr (ConstantType == D3D9ConstantType::Float)
            setCaptures.fConsts.set(reg, true);
          else if constexpr (ConstantType == D3D9ConstantType::Int)
            setCaptures.iConsts.set(reg, true);
          else if constexpr (ConstantType == D3D9ConstantType::Bool)
            setCaptures.bConsts.set(reg, true);
        }

        UpdateStateConstants<
          ProgramType,
          ConstantType,
          T>(
            &m_state,
            StartRegister,
            pConstantData,
            Count,
            false);

        return D3D_OK;
      };

      return ProgramType == DxsoProgramTypes::VertexShader
        ? SetHelper(m_captures.vsConsts)
        : SetHelper(m_captures.psConsts);
    }

    HRESULT SetVertexBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits);
    HRESULT SetPixelBoolBitfield (uint32_t idx, uint32_t mask, uint32_t bits);

  private:

    void CapturePixelRenderStates();
    void CapturePixelSamplerStates();
    void CapturePixelShaderStates();

    void CaptureVertexRenderStates();
    void CaptureVertexSamplerStates();
    void CaptureVertexShaderStates();

    void CaptureType(D3D9StateBlockType State);

    D3D9CapturableState  m_state;
    D3D9StateCaptures    m_captures;

    D3D9DeviceState*     m_deviceState;

  };

}
