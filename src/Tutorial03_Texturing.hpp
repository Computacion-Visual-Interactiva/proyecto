/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp"

namespace Diligent
{

class Tutorial03_Texturing final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial03: Texturing"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override;

    virtual float4x4 MakeWorld(const float3& Pos, const float3& Forward, const float3& Up);

private:
    void CreatePipelineState();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void LoadTexture();
    void CreateSkySphere();
    void GenerateInstanceData(float Time);
    void DrawButterflies();
    void InitInstanceData();

    RefCntAutoPtr<IPipelineState>         m_pPSO;
    RefCntAutoPtr<IBuffer>                m_ButterflyVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_ButterflyIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_VSConstants;
    RefCntAutoPtr<ITextureView>           m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;

    // --- PNG + depth grid -----------------------------------------------
    RefCntAutoPtr<IPipelineState>         m_SkyPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_SkySRB;
    RefCntAutoPtr<IBuffer>                m_SkyCB;
    RefCntAutoPtr<ITextureView>           m_SkySRV;

    FirstPersonCamera m_Camera;
    float4x4          m_WorldViewProj;

    float                  m_PathTime  = 0.0f;  // accumulated time
    static constexpr float kRadius     = 6.0f;  // circle radius
    static constexpr float kSpeed      = 0.75f;  // radians·s-¹
    static constexpr float kBaseHeight = 0.0f;  // centre of vertical motion
    static constexpr float kBobAmp     = 0.25f; // vertical amplitude
    static constexpr float kBobFreq    = 0.80f; // Hz

    static constexpr float kWingFactor = 6.0f;  // flaps per bob
    static constexpr float kWingAmp    = 0.60f; // radians

    std::vector<float4x4> m_InstanceWorlds;
    Uint32                m_InstanceCount = 50;
    std::vector<float3>   m_InstanceCenters;
    std::vector<float>    m_InstancePhases;


    struct VSConstants
    {
        float4x4 WorldViewProj;
        float    WingAngle;
        float3   _Padding;
    };
    static_assert(sizeof(VSConstants) % 16 == 0, "CB size must be 16-byte aligned");
};

} // namespace Diligent
