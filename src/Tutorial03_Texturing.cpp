/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#include "Tutorial03_Texturing.hpp"
#include "butterfly_verts.hpp"
#include "MapHelper.hpp"
#include "FirstPersonCamera.hpp"
#include "StringTools.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "BasicMath.hpp"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial03_Texturing();
}

void Tutorial03_Texturing::CreatePipelineState()
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Cube PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Pack matrices in row-major order
    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    // Presentation engine always expects input in gamma space. Normally, pixel shader output is
    // converted from linear to gamma space by the GPU. However, some platforms (e.g. Android in GLES mode,
    // or Emscripten in WebGL mode) do not support gamma-correction. In this case the application
    // has to do the conversion manually.
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        BufferDesc CBDesc;
        CBDesc.Name           = "VS constants";
        CBDesc.Size           = sizeof(VSConstants);
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        {0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        {1, 0, 2, VT_FLOAT32, False},
        // Attribute 2 - Wing flag (-1/0/+1)
        {2, 0, 1, VT_FLOAT32, False}
    };
    // clang-format on

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly through the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

// call this from Initialize()  **after** SampleBase::Initialize()
void Tutorial03_Texturing::CreateSkySphere()
{
    // ------------------ constant buffer (inverse VP) ---------------------
    BufferDesc cbd;
    cbd.Name           = "SkySphere CB";
    cbd.Size           = sizeof(float4x4);
    cbd.BindFlags      = BIND_UNIFORM_BUFFER;
    cbd.Usage          = USAGE_DYNAMIC;
    cbd.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer(cbd, nullptr, &m_SkyCB); // new member!

    // ------------------ load equirectangular PNG -------------------------
    TextureLoadInfo tli{};
    tli.IsSRGB = true;
    RefCntAutoPtr<ITexture> SkyTex;
    CreateTextureFromFile("hdrHigh.png", tli,
                          m_pDevice, &SkyTex);
    m_SkySRV = SkyTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // ------------------------- PSO ---------------------------------------
    GraphicsPipelineStateCreateInfo ci;
    ci.PSODesc.Name         = "SkySphere PSO";
    ci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    const auto& sc                                   = m_pSwapChain->GetDesc();
    ci.GraphicsPipeline.NumRenderTargets             = 1;
    ci.GraphicsPipeline.RTVFormats[0]                = sc.ColorBufferFormat;
    ci.GraphicsPipeline.DSVFormat                    = sc.DepthBufferFormat;
    ci.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ci.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    // compile shaders
    ShaderCreateInfo si;
    si.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr,
                                                             &si.pShaderSourceStreamFactory);

    RefCntAutoPtr<IShader> vs, ps;
    si.Desc       = {"Sky VS", SHADER_TYPE_VERTEX, true};
    si.EntryPoint = "VSMain";
    si.FilePath   = "DepthGrid.hlsl";
    m_pDevice->CreateShader(si, &vs);

    si.Desc       = {"Sky PS", SHADER_TYPE_PIXEL, true};
    si.EntryPoint = "PSMain";
    m_pDevice->CreateShader(si, &ps);

    ci.pVS = vs;
    ci.pPS = ps;

    // pipeline resources
    ShaderResourceVariableDesc vars[] =
        {
            {SHADER_TYPE_PIXEL, "g_SkyTex", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    ci.PSODesc.ResourceLayout.Variables    = vars;
    ci.PSODesc.ResourceLayout.NumVariables = _countof(vars);

    SamplerDesc          samp{FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                     TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};
    ImmutableSamplerDesc ims[] =
        {
            {SHADER_TYPE_PIXEL, "g_SkyTex", samp}};
    ci.PSODesc.ResourceLayout.ImmutableSamplers    = ims;
    ci.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ims);

    m_pDevice->CreateGraphicsPipelineState(ci, &m_SkyPSO);
    m_SkyPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CB")->Set(m_SkyCB);
    m_SkyPSO->CreateShaderResourceBinding(&m_SkySRB, true);
    m_SkySRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SkyTex")->Set(m_SkySRV);
}




void Tutorial03_Texturing::CreateVertexBuffer()
{
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Butterfly VB";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size      = sizeof(Butterfly::ButterflyVerts);
    BufferData VBData;
    VBData.pData    = Butterfly::ButterflyVerts;
    VBData.DataSize = sizeof(Butterfly::ButterflyVerts);
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_ButterflyVertexBuffer);
}

void Tutorial03_Texturing::CreateIndexBuffer()
{
    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Butterfly VI";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size      = sizeof(Butterfly::ButterflyIndices);
    BufferData IBData;
    IBData.pData    = Butterfly::ButterflyIndices;
    IBData.DataSize = sizeof(Butterfly::ButterflyIndices);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_ButterflyIndexBuffer);
}

void Tutorial03_Texturing::LoadTexture()
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> Tex;
    CreateTextureFromFile("try.png", loadInfo, m_pDevice, &Tex);
    // Get shader resource view from the texture
    m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Set texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}


void Tutorial03_Texturing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    const auto SCDesc = m_pSwapChain->GetDesc();

    m_Camera.SetPos(float3{0.f, 0.f, -18.f});
    m_Camera.SetRotation(0.f, 0.f);
    m_Camera.SetMoveSpeed(4.f);
    m_Camera.SetRotationSpeed(0.006f);
    m_Camera.SetProjAttribs(0.1f, 100.f,
                            static_cast<float>(SCDesc.Width) / SCDesc.Height,
                            PI_F / 4,
                            SCDesc.PreTransform,
                            m_pDevice->GetDeviceInfo().IsGLDevice());

    CreatePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();
    LoadTexture();
    CreateSkySphere();
}

// Render a frame
void Tutorial03_Texturing::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();

    float4 ClearColor = {0.35f, 0.35f, 0.35f, 1.0f};
    if (m_ConvertPSOutputToGamma)
        ClearColor = LinearToSRGB(ClearColor);

    m_pImmediateContext->ClearRenderTarget(
        pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
     
        float4x4 ViewNoPos = m_Camera.GetViewMatrix();
        ViewNoPos._41 = ViewNoPos._42 = ViewNoPos._43 = 0.0f;

        const float4x4 InvRotProj =
            (ViewNoPos * m_Camera.GetProjMatrix()).Inverse();

        MapHelper<float4x4> CB(m_pImmediateContext, m_SkyCB,
                               MAP_WRITE, MAP_FLAG_DISCARD);
        *CB = InvRotProj; 

        m_pImmediateContext->SetPipelineState(m_SkyPSO);
        m_pImmediateContext->CommitShaderResources(
            m_SkySRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs DA;
        DA.NumVertices = 3;
        DA.Flags       = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->Draw(DA);
    }

    {
        // Update VS constants (row‑major)
        MapHelper<VSConstants> CB(m_pImmediateContext, m_VSConstants,
                               MAP_WRITE, MAP_FLAG_DISCARD);
        CB->WorldViewProj = m_WorldViewProj;
        CB->WingAngle     = std::sin(m_PathTime * 2.0f * PI_F * kWingFactor) * kWingAmp;

        // Bind VB / IB
        const Uint64 offsets[] = {0};
        IBuffer*     vbs[]     = {m_ButterflyVertexBuffer};
        m_pImmediateContext->SetVertexBuffers(
            0, 1, vbs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(
            m_ButterflyIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // PSO & SRB
        m_pImmediateContext->SetPipelineState(m_pPSO);
        m_pImmediateContext->CommitShaderResources(
            m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs Draw;
        Draw.IndexType  = VT_UINT32;
        Draw.NumIndices = Butterfly::ButterflyIndexCount;
        Draw.Flags      = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(Draw);
    }
}

float4x4 Tutorial03_Texturing::MakeWorld(const float3& Pos,
                          const float3& Forward,
                          const float3& Up)
{
    float3 Z = normalize(-Forward);
    float3 X = normalize(cross(Up, Z));
    float3 Y = cross(Z, X);
    return float4x4(X.x, Y.x, Z.x, 0.0f,
                    X.y, Y.y, Z.y, 0.0f,
                    X.z, Y.z, Z.z, 0.0f,
                    Pos.x, Pos.y, Pos.z, 1.0f);
}


void Tutorial03_Texturing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    m_PathTime += static_cast<float>(ElapsedTime);

    float theta = m_PathTime * kSpeed;
    float x     = kRadius * std::cos(theta);
    float z     = kRadius * std::sin(theta);

    float phase = m_PathTime * kBobFreq * 2.0f * PI_F;
    float y     = kBaseHeight +
        kBobAmp * (0.6f * std::sin(phase) + 0.4f * std::sin(phase * 2.3f));

    float3 forward = normalize(float3{
        -kRadius * std::sin(theta),
        0.0f,
        kRadius * std::cos(theta)});

    float4x4 World = MakeWorld(float3{x, y, z},
                               forward,
                               float3{0, 1, 0});

    const auto SurfT = GetSurfacePretransformMatrix(float3{0, 0, 1});
    m_WorldViewProj  = World *
        m_Camera.GetViewMatrix() *
        SurfT *
        m_Camera.GetProjMatrix();
}


void Tutorial03_Texturing::WindowResize(Uint32 W, Uint32 H)
{
    SampleBase::WindowResize(W, H);

    m_Camera.SetProjAttribs(0.1f, 100.f,
                            static_cast<float>(W) / H,
                            PI_F / 4,
                            m_pSwapChain->GetDesc().PreTransform,
                            m_pDevice->GetDeviceInfo().IsGLDevice());
}

} // namespace Diligent
