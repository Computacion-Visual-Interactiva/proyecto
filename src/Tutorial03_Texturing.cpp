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
#include <random> 

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial03_Texturing();
}

void Tutorial03_Texturing::CreatePipelineState()
{
    // 1) Prepare PSO descriptor
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name         = "Butterfly PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // 2) Configure render target and depth formats
    const auto& SCDesc                              = m_pSwapChain->GetDesc();
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]    = SCDesc.ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat        = SCDesc.DepthBufferFormat;

    // 3) Rasterizer & Depth‐Stencil settings
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE; // no back‐face culling
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;           // enable depth test

    // 4) Common shader compile settings
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    // Macro to optionally convert PS output to gamma
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    // Shader source loader
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    // 5) Create vertex shader + its constant buffer
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Butterfly VS";
        ShaderCI.FilePath        = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);

        // Create dynamic uniform buffer for VSConstants
        BufferDesc CBDesc;
        CBDesc.Name           = "VS constants";
        CBDesc.Size           = sizeof(VSConstants);
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
    }

    // 6) Create pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Butterfly PS";
        ShaderCI.FilePath        = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // 7) Define vertex input layout: Position, UV, WingFlag
    LayoutElement LayoutElems[] =
        {
            {0, 0, 3, VT_FLOAT32, False}, // ATTRIB0: float3 Pos
            {1, 0, 2, VT_FLOAT32, False}, // ATTRIB1: float2 UV
            {2, 0, 1, VT_FLOAT32, False}  // ATTRIB2: float  WingFlag
        };
    PSOCreateInfo.pVS                                         = pVS;
    PSOCreateInfo.pPS                                         = pPS;
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    // 8) Resource layout: one mutable texture SRV, one immutable sampler
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};
    ImmutableSamplerDesc ImtblSamplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    // 9) Create the PSO
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    // 10) Bind the static VS constant buffer and create SRB
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial03_Texturing::CreateSkySphere()
{
    //----------------------------------------------------------------------------------------------
    // 1) Create dynamic uniform buffer for the inverse View×Proj matrix (used by sky‐sphere VS)
    //----------------------------------------------------------------------------------------------
    BufferDesc cbd;
    cbd.Name           = "SkySphere CB";
    cbd.Size           = sizeof(float4x4);
    cbd.BindFlags      = BIND_UNIFORM_BUFFER;
    cbd.Usage          = USAGE_DYNAMIC; // we'll update it every frame
    cbd.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer(cbd, nullptr, &m_SkyCB);

    //----------------------------------------------------------------------------------------------
    // 2) Load the equirectangular HDR sky texture from file and create SRV
    //----------------------------------------------------------------------------------------------
    TextureLoadInfo tli{};
    tli.IsSRGB = true; // treat as sRGB for correct gamma
    RefCntAutoPtr<ITexture> SkyTex;
    CreateTextureFromFile("hdrHigh.png", tli, m_pDevice, &SkyTex);
    m_SkySRV = SkyTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    //----------------------------------------------------------------------------------------------
    // 3) Configure graphics pipeline state for rendering the sky sphere
    //----------------------------------------------------------------------------------------------
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name         = "SkySphere PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // Render target & depth formats match the swap chain
    const auto& SCDesc                              = m_pSwapChain->GetDesc();
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]    = SCDesc.ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat        = SCDesc.DepthBufferFormat;

    // We only draw a full‐screen triangle, no depth test needed
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    //----------------------------------------------------------------------------------------------
    // 4) Compile sky‐sphere shaders (HLSL entry points VSMain/PSMain in DepthGrid.hlsl)
    //----------------------------------------------------------------------------------------------
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &ShaderCI.pShaderSourceStreamFactory);

    RefCntAutoPtr<IShader> vs, ps;
    // Vertex shader
    ShaderCI.Desc       = {"Sky VS", SHADER_TYPE_VERTEX, true};
    ShaderCI.EntryPoint = "VSMain";
    ShaderCI.FilePath   = "DepthGrid.hlsl";
    m_pDevice->CreateShader(ShaderCI, &vs);

    // Pixel shader
    ShaderCI.Desc       = {"Sky PS", SHADER_TYPE_PIXEL, true};
    ShaderCI.EntryPoint = "PSMain";
    m_pDevice->CreateShader(ShaderCI, &ps);

    PSOCreateInfo.pVS = vs;
    PSOCreateInfo.pPS = ps;

    //----------------------------------------------------------------------------------------------
    // 5) Resource layout: mutable sky texture + immutable linear sampler
    //----------------------------------------------------------------------------------------------
    ShaderResourceVariableDesc Vars[] =
        {
            {SHADER_TYPE_PIXEL, "g_SkyTex", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    SamplerDesc          sampDesc{FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                         TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};
    ImmutableSamplerDesc immutableSamp[]                      = {{SHADER_TYPE_PIXEL, "g_SkyTex", sampDesc}};
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = immutableSamp;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutableSamp);

    //----------------------------------------------------------------------------------------------
    // 6) Create the pipeline state and bind static resources
    //----------------------------------------------------------------------------------------------
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_SkyPSO);

    // Bind the sky‐sphere CB as a static VS variable named "CB"
    m_SkyPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CB")->Set(m_SkyCB);

    // Create SRB and set the sky texture SRV
    m_SkyPSO->CreateShaderResourceBinding(&m_SkySRB, true);
    m_SkySRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SkyTex")->Set(m_SkySRV);
}

void Tutorial03_Texturing::InitInstanceData()
{
    // Prepare containers
    m_InstanceCenters.clear();
    m_InstancePhases.clear();
    m_InstanceCenters.reserve(m_InstanceCount);
    m_InstancePhases.reserve(m_InstanceCount);

    // Set up random generators
    std::mt19937                          rng{std::random_device{}()};
    std::uniform_real_distribution<float> distX(-20.f, 20.f);       // X range
    std::uniform_real_distribution<float> distY(-10.f, 10.f);       // Y range
    std::uniform_real_distribution<float> distZ(-30.f, 30.f);       // Z range
    std::uniform_real_distribution<float> distPhase(0.f, 2 * PI_F); // start angle

    // Populate per-instance data
    for (Uint32 i = 0; i < m_InstanceCount; ++i)
    {
        // Random 3D spawn point
        float cx = distX(rng);
        float cy = distY(rng);
        float cz = distZ(rng);
        m_InstanceCenters.emplace_back(cx, cy, cz);

        // Random initial phase around orbit
        m_InstancePhases.push_back(distPhase(rng));
    }
}

void Tutorial03_Texturing::CreateVertexBuffer()
{
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Butterfly VB";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE; // static mesh
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size      = sizeof(Butterfly::ButterflyVerts);

    BufferData VBData;
    VBData.pData    = Butterfly::ButterflyVerts; // pointer to raw verts
    VBData.DataSize = sizeof(Butterfly::ButterflyVerts);

    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_ButterflyVertexBuffer);
}

void Tutorial03_Texturing::CreateIndexBuffer()
{
    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Butterfly IB";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE; // static mesh
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size      = sizeof(Butterfly::ButterflyIndices);

    BufferData IBData;
    IBData.pData    = Butterfly::ButterflyIndices; // pointer to index array
    IBData.DataSize = sizeof(Butterfly::ButterflyIndices);

    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_ButterflyIndexBuffer);
}

void Tutorial03_Texturing::GenerateInstanceData(float Time)
{
    // Prepare output container
    m_InstanceWorlds.clear();
    m_InstanceWorlds.reserve(m_InstanceCount);

    // Compute vertical bob offset once per frame
    float bobPhase  = Time * kBobFreq * 2.0f * PI_F;
    float bobOffset = kBobAmp * (0.6f * std::sin(bobPhase) + 0.4f * std::sin(bobPhase * 2.3f));

    // Build world matrix for each butterfly
    for (Uint32 i = 0; i < m_InstanceCount; ++i)
    {
        // 1) Compute orbit angle: startPhase + global speed*time
        float theta = m_InstancePhases[i] + Time * kSpeed;

        // 2) Position on horizontal circle + vertical bob
        const auto& C = m_InstanceCenters[i];
        float       x = C.x + kRadius * std::cos(theta);
        float       y = C.y + bobOffset;
        float       z = C.z + kRadius * std::sin(theta);

        // 3) Compute forward vector tangent to the circle
        float3 forward = normalize(float3{
            -kRadius * std::sin(theta), // dx/dθ
            0.0f,
            kRadius * std::cos(theta) // dz/dθ
        });

        // 4) Assemble world matrix and store
        m_InstanceWorlds.push_back(
            MakeWorld({x, y, z}, forward, float3{0, 1, 0}));
    }
}

void Tutorial03_Texturing::DrawButterflies()
{
    // Compute common wing flap angle for all butterflies this frame
    const float wingAng = std::sin(m_PathTime * 2.f * PI_F * kWingFactor) * kWingAmp;

    // Setup draw parameters (same index buffer for every instance)
    DrawIndexedAttribs Attribs;
    Attribs.IndexType  = VT_UINT32;
    Attribs.NumIndices = Butterfly::ButterflyIndexCount;
    Attribs.Flags      = DRAW_FLAG_VERIFY_ALL;

    // Loop through each instance world matrix
    for (Uint32 i = 0; i < m_InstanceWorlds.size(); ++i)
    {
        // 1) Compute World×ViewProj for this instance
        float4x4 wvp = m_InstanceWorlds[i] * m_WorldViewProj;

        // 2) Map the VS constant buffer (discard old), write new constants
        MapHelper<VSConstants> CB(m_pImmediateContext, m_VSConstants,
                                  MAP_WRITE, MAP_FLAG_DISCARD);
        CB->WorldViewProj = wvp;     // per-instance transform
        CB->WingAngle     = wingAng; // common flap angle

        // 3) Draw the indexed mesh for this butterfly
        m_pImmediateContext->DrawIndexed(Attribs);
    }
}

void Tutorial03_Texturing::LoadTexture()
{
    // Configure texture loading as sRGB
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;

    // Load texture from file
    RefCntAutoPtr<ITexture> Tex;
    CreateTextureFromFile("try.png", loadInfo, m_pDevice, &Tex);

    // Obtain SRV for use in the pixel shader
    m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Bind the texture SRV to the shader variable "g_Texture"
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}

void Tutorial03_Texturing::Initialize(const SampleInitInfo& InitInfo)
{
    // 1) Base class initialization (window, swap chain, input, etc.)
    SampleBase::Initialize(InitInfo);

    // 2) Configure camera position, orientation, and projection
    const auto SCDesc = m_pSwapChain->GetDesc();
    m_Camera.SetPos(float3{0.f, 0.f, -30.f}); // move camera back
    m_Camera.SetRotation(0.f, 0.f);
    m_Camera.SetMoveSpeed(4.f);
    m_Camera.SetRotationSpeed(0.006f);
    m_Camera.SetProjAttribs(
        0.1f, 100.f,
        static_cast<float>(SCDesc.Width) / SCDesc.Height,
        PI_F / 4,
        SCDesc.PreTransform,
        m_pDevice->GetDeviceInfo().IsGLDevice());

    // 3) Create rendering pipeline, mesh buffers, and sky sphere
    CreatePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();
    LoadTexture();
    CreateSkySphere();

    // 4) Set up butterfly instance centers & phases, and generate initial worlds
    InitInstanceData();
    GenerateInstanceData(0.0f);
}

void Tutorial03_Texturing::Render()
{
    // 1) Acquire back buffer and depth-stencil views
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();

    // 2) Clear color & depth. Optionally convert clear color to sRGB
    float4 Clear = {0.35f, 0.35f, 0.35f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // Linear→Gamma since some platforms lack hardware gamma-correct
        Clear = float4{LinearToSRGB(float3{Clear.r, Clear.g, Clear.b}), Clear.a};
    }
    m_pImmediateContext->ClearRenderTarget(
        pRTV, Clear.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        pDSV, CLEAR_DEPTH_FLAG, /* Depth = */ 1.0f, /* Stencil = */ 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // --------------------------------------------------------------------------
    // 3) Draw sky sphere (full-screen triangle) with its own PSO & SRB
    // --------------------------------------------------------------------------
    {
        // Remove translation from view matrix for sky so it always surrounds camera
        float4x4 ViewNoPos = m_Camera.GetViewMatrix();
        ViewNoPos._41 = ViewNoPos._42 = ViewNoPos._43 = 0;

        // Compute inverse of (view × projection) and upload to CB
        float4x4            InvRotProj = (ViewNoPos * m_Camera.GetProjMatrix()).Inverse();
        MapHelper<float4x4> CB(m_pImmediateContext, m_SkyCB, MAP_WRITE, MAP_FLAG_DISCARD);
        *CB = InvRotProj;

        // Set sky pipeline & resources, then draw full-screen tri
        m_pImmediateContext->SetPipelineState(m_SkyPSO);
        m_pImmediateContext->CommitShaderResources(m_SkySRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawAttribs DA;
        DA.NumVertices = 3;
        DA.Flags       = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->Draw(DA);
    }

    // --------------------------------------------------------------------------
    // 4) Draw all butterflies (CPU-instanced): bind mesh VB/IB, PSO, SRB, then loop draw
    // --------------------------------------------------------------------------
    {
        // Bind butterfly mesh vertex buffer (slot 0)
        Uint64   offset = 0;
        IBuffer* VBs[]  = {m_ButterflyVertexBuffer};
        m_pImmediateContext->SetVertexBuffers(
            /*StartSlot=*/0, /*NumBuffers=*/1, VBs, &offset,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);

        // Bind butterfly mesh index buffer
        m_pImmediateContext->SetIndexBuffer(
            m_ButterflyIndexBuffer, /*ByteOffset=*/0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set butterfly pipeline & commit texture SRV
        m_pImmediateContext->SetPipelineState(m_pPSO);
        m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Issue draws for each instance
        DrawButterflies();
    }
}

float4x4 Tutorial03_Texturing::MakeWorld(const float3& Pos,
                                         const float3& Forward,
                                         const float3& Up)
{
    // Compute orthonormal basis
    float3 Z = normalize(-Forward);     // Backward direction
    float3 X = normalize(cross(Up, Z)); // Right direction
    float3 Y = cross(Z, X);             // True up

    // Pack into row-major 4×4 (last row = translation)
    return float4x4(
        X.x, Y.x, Z.x, 0.0f,
        X.y, Y.y, Z.y, 0.0f,
        X.z, Y.z, Z.z, 0.0f,
        Pos.x, Pos.y, Pos.z, 1.0f);
}

void Tutorial03_Texturing::Update(double CurrTime, double ElapsedTime)
{
    // Handle UI and internal timers
    SampleBase::Update(CurrTime, ElapsedTime);

    // Move the camera based on user input
    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

    // Advance global animation time (wing flop, bob, orbits)
    m_PathTime += static_cast<float>(ElapsedTime);

    // Recompute butterfly instance transforms
    GenerateInstanceData(m_PathTime);

    // Compute combined View×Proj once per frame
    // Surface pre-transform handles display rotation/orientation
    const float4x4 SurfT = GetSurfacePretransformMatrix(float3{0, 0, 1});
    m_WorldViewProj      = m_Camera.GetViewMatrix() *
        SurfT *
        m_Camera.GetProjMatrix();
}

void Tutorial03_Texturing::WindowResize(Uint32 W, Uint32 H)
{
    // Resize default swap chain buffers, UI, etc.
    SampleBase::WindowResize(W, H);

    // Update camera's projection parameters to new window dimensions
    m_Camera.SetProjAttribs(
        /* near plane    */ 0.1f,
        /* far plane     */ 100.f,
        /* aspect ratio  */ static_cast<float>(W) / H,
        /* vertical FOV  */ PI_F / 4,
        /* pre-transform */ m_pSwapChain->GetDesc().PreTransform,
        /* isGL device   */ m_pDevice->GetDeviceInfo().IsGLDevice());
}

} // namespace Diligent
