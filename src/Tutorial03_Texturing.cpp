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
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"

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
        CreateUniformBuffer(m_pDevice, sizeof(float4x4), "VS constants CB", &m_VSConstants);
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
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False}
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

void Tutorial03_Texturing::CreateVertexBuffer()
{
    struct Vertex
    {
        float3 pos;
        float2 uv;
    };

    constexpr Vertex ButterflyVerts[] =
        {
            // Wing left upper
            {float3{6.45f, 3.6f, 0}, float2{0, 1}},     // Vertex 0 centro
            {float3{6.45f, 5.4f, 0}, float2{0, 1}},     // Vertex 1
            {float3{9.2f, 5.35f, 0}, float2{0, 1}},     // Vertex 2
            {float3{8.8f, 4.3f, 0}, float2{0, 1}},      // Vertex 3
            {float3{8.35f, 3.4f, 0}, float2{0, 1}},     // Vertex 4
            {float3{7.7f, 2.58f, 0}, float2{0, 1}},     // Vertex 5
            {float3{7.13f, 2.0f, 0}, float2{0, 1}},     // Vertex 6
            {float3{5.95f, 1.12f, 0}, float2{0, 1}},    // Vertex 7
            {float3{4.45f, 0.9f, 0}, float2{0, 1}},     // Vertex 8
            {float3{4.3f, 1.8f, 0}, float2{0, 1}},      // Vertex 9 
            {float3{4.31f, 2.55f, 0}, float2{0, 1}},    // Vertex 10
            {float3{4.20f, 3.3f, 0}, float2{0, 1}},     // Vertex 11
            {float3{4.07f, 3.65f, 0}, float2{0, 1}},    // Vertex 12
            {float3{4.15f, 4.05f, 0}, float2{0, 1}},    // Vertex 13
            {float3{4.2f, 4.6f, 0}, float2{0, 1}},      // Vertex 14
            {float3{4.55f, 5.1f, 0}, float2{0, 1}},     // Vertex 15
            {float3{6.0f, 5.4f, 0}, float2{0, 1}},      // Vertex 16

            // Wing left down
            {float3{7.1f, 7.0f, 0}, float2{0, 1}},      // Vertex 17 centro
            {float3{6.65f, 5.38f, 0}, float2{0, 1}},    // Vertex 18
            {float3{9.25f, 5.4f, 0}, float2{0, 1}},     // Vertex 19
            {float3{9.24f, 6.12f, 0}, float2{0, 1}},   // Vertex 20
            {float3{9.3f, 6.8f, 0}, float2{0, 1}},      // Vertex 21
            {float3{9.15f, 7.2f, 0}, float2{0, 1}},     // Vertex 22
            {float3{8.86f, 7.75f, 0}, float2{0, 1}},    // Vertex 23
            {float3{8.17f, 8.4f, 0}, float2{0, 1}},     // Vertex 24
            {float3{7.88f, 8.88f, 0}, float2{0, 1}},   // Vertex 25
            {float3{7.7f, 9.15f, 0}, float2{0, 1}},     // Vertex 26
            {float3{6.62f, 9.86f, 0}, float2{0, 1}},   // Vertex 27
            {float3{5.9f, 9.61f, 0}, float2{0, 1}},     // Vertex 28
            {float3{5.72f, 9.2f, 0}, float2{0, 1}},     // Vertex 29
            {float3{5.16f, 8.73f, 0}, float2{0, 1}},   // Vertex 30
            {float3{4.95f, 8.3f, 0}, float2{0, 1}},   // Vertex 31
            {float3{4.72f, 8.1f, 0}, float2{0, 1}},   // Vertex 32
            {float3{4.68f, 7.7f, 0}, float2{0, 1}},   // Vertex 33
            {float3{4.45f, 7.3f, 0}, float2{0, 1}},   // Vertex 34
            {float3{4.5f, 6.4f, 0}, float2{0, 1}},   // Vertex 35
            {float3{4.65f, 6.5f, 0}, float2{0, 1}},   // Vertex 36

            // Antler Left
            {float3{8.72f, 3.27f, 0}, float2{0, 1}}, // Vertex 37 centro
            {float3{8.65f, 3.3f, 0}, float2{0, 1}},   // Vertex 38
            {float3{9.45f, 4.7f, 0}, float2{0, 1}},   // Vertex 39

            // Antler Right
            {float3{10.23f, 3.19f, 0}, float2{0, 1}}, // Vertex 40 centro
            {float3{10.3f, 3.28f, 0}, float2{0, 1}},   // Vertex 41
            {float3{9.57f, 4.7f, 0}, float2{0, 1}},   // Vertex 42

            // head
            {float3{9.47f, 4.9f, 0}, float2{0, 1}}, // Vertex 43 centro
            {float3{9.37f, 5.08f, 0}, float2{0, 1}}, // Vertex 44
            {float3{9.62f, 5.1f, 0}, float2{0, 1}},   // Vertex 45
            {float3{9.7f, 4.9f, 0}, float2{0, 1}},   // Vertex 46
            {float3{9.65f, 4.72f, 0}, float2{0, 1}},   // Vertex 47
            {float3{9.45f, 4.7f, 0}, float2{0, 1}},   // Vertex 48
            {float3{9.25f, 4.78f, 0}, float2{0, 1}},   // Vertex 49
            {float3{9.24f, 4.9f, 0}, float2{0, 1}},   // Vertex 50

            // Body
            {float3{9.55f, 7.12f, 0}, float2{0, 1}}, // Vertex 51
            {float3{9.39f, 7.04f, 0}, float2{0, 1}},   // Vertex 52
            {float3{9.65f, 7.0f, 0}, float2{0, 1}},   // Vertex 53
            {float3{9.72f, 6.8f, 0}, float2{0, 1}},   // Vertex 54
            {float3{9.3f, 6.71f, 0}, float2{0, 1}},  // Vertex 55
            {float3{9.74f, 6.4f, 0}, float2{0, 1}},   // Vertex 56
            {float3{9.23f, 5.92f, 0}, float2{0, 1}},   // Vertex 57
            {float3{9.69f, 5.74f, 0}, float2{0, 1}},   // Vertex 58
            {float3{9.25f, 5.4f, 0}, float2{0, 1}},  // Vertex 59
            {float3{9.7f, 5.25f, 0}, float2{0, 1}},   // Vertex 60
            {float3{9.37f, 5.08f, 0}, float2{0, 1}},  // Vertex 61
            {float3{9.63f, 5.1f, 0}, float2{0, 1}},  // Vertex 62

            // Wing right upper
            {float3{12.65f, 3.8f, 0}, float2{0, 1}}, // Vertex 63 centro
            {float3{9.7f, 5.4f, 0}, float2{0, 1}}, // Vertex 64
            {float3{13.19f, 5.58f, 0}, float2{0, 1}},   // Vertex 65
            {float3{14.08f, 5.6f, 0}, float2{0, 1}}, // Vertex 66
            {float3{14.65f, 5.25f, 0}, float2{0, 1}},   // Vertex 67
            {float3{14.88f, 5.1f, 0}, float2{0, 1}},   // Vertex 68
            {float3{14.92f, 4.47f, 0}, float2{0, 1}},   // Vertex 69
            {float3{15.03f, 4.29f, 0}, float2{0, 1}},   // Vertex 70
            {float3{14.95f, 3.8f, 0}, float2{0, 1}},   // Vertex 71
            {float3{14.96f, 3.15f, 0}, float2{0, 1}},   // Vertex 72
            {float3{15.03f, 2.23f, 0}, float2{0, 1}},   // Vertex 73
            {float3{14.97f, 1.27f, 0}, float2{0, 1}},   // Vertex 74
            {float3{14.52f, 1.15f, 0}, float2{0, 1}},   // Vertex 75
            {float3{13.95f, 1.28f, 0}, float2{0, 1}},   // Vertex 76
            {float3{13.07f, 1.65f, 0}, float2{0, 1}},   // Vertex 77
            {float3{12.3f, 2.05f, 0}, float2{0, 1}},   // Vertex 78
            {float3{11.45f, 2.8f, 0}, float2{0, 1}},   // Vertex 79
            {float3{10.77f, 3.56f, 0}, float2{0, 1}},   // Vertex 80
            {float3{10.17f, 4.28f, 0}, float2{0, 1}},   // Vertex 81

            // Wing right down
            {float3{12.02f, 7.05f, 0}, float2{0, 1}}, // Vertex 82 centro
            {float3{9.68f, 5.4f, 0}, float2{0, 1}},  // Vertex 83
            {float3{13.16f, 5.67f, 0}, float2{0, 1}},  // Vertex 84
            {float3{14.25f, 6.3f, 0}, float2{0, 1}},  // Vertex 85
            {float3{14.48f, 6.75f, 0}, float2{0, 1}},  // Vertex 86
            {float3{14.48f, 7.28f, 0}, float2{0, 1}},   // Vertex 87
            {float3{14.26f, 7.75f, 0}, float2{0, 1}},   // Vertex 88
            {float3{14.18f, 8.18f, 0}, float2{0, 1}},  // Vertex 89
            {float3{13.95f, 8.32f, 0}, float2{0, 1}},  // Vertex 90
            {float3{13.85f, 8.7f, 0}, float2{0, 1}},   // Vertex 91
            {float3{13.5f, 8.88f, 0}, float2{0, 1}},  // Vertex 92
            {float3{13.25f, 9.15f, 0}, float2{0, 1}},  // Vertex 93
            {float3{13.15f, 9.61f, 0}, float2{0, 1}},   // Vertex 94
            {float3{12.82f, 9.62f, 0}, float2{0, 1}},  // Vertex 95
            {float3{12.47f, 9.85f, 0}, float2{0, 1}},   // Vertex 96
            {float3{12.1f, 9.8f, 0}, float2{0, 1}},  // Vertex 97
            {float3{11.58f, 9.52f, 0}, float2{0, 1}},   // Vertex 98
            {float3{11.17f, 8.86f, 0}, float2{0, 1}},  // Vertex 99
            {float3{10.78f, 8.25f, 0}, float2{0, 1}}, // Vertex 100
            {float3{10.0f, 7.45f, 0}, float2{0, 1}},   // Vertex 101
            {float3{9.82f, 6.9f, 0}, float2{0, 1}},  // Vertex 102
            {float3{9.74f, 6.4f, 0}, float2{0, 1}},   // Vertex 103

        };

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Butterfly VB";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size      = sizeof(ButterflyVerts);
    BufferData VBData;
    VBData.pData    = ButterflyVerts;
    VBData.DataSize = sizeof(ButterflyVerts);
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_ButterflyVertexBuffer);
}

void Tutorial03_Texturing::CreateIndexBuffer()
{
    constexpr Uint32 Indices[] =
    {
        // Wing left upper
        1,0,2, 2,0,3,
        3,0,4, 4,0,5,
        5,0,6, 6,0,7,
        7,0,8, 8,0,9,
        9,0,10, 10,0,11,
        11,0,12, 12,0,13,
        13,0,14, 14,0,15,
        15,0,16, 16,0,1,

        // Wing left down
        18,17,19, 19,17,20,
        20,17,21, 21,17,22,
        22,17,23, 23,17,24,
        24,17,25, 25,17,26,
        26,17,27, 27,17,28,
        28,17,29, 29,17,30,
        30,17,31, 31,17,32,
        32,17,33, 33,17,34,
        34,17,35, 35,17,36,
        36,17,18, 18,17,1,

        // Antlers
        38,37,39, 41,40,42,

        // head
        44,43,45, 46,43,47,
        48,43,49, 50,43,44,
        44,43,45, 46,43,47,
        47,43,48, 49,43,50,
        50,43,44, 44,43,45,
        45,43,46, 47,43,48,

        // Body
        51,52,53, 53,52,54,
        54,52,55, 55,54,56,
        56,55,57, 57,56,58,
        58,57,59, 59,58,60,
        60,59,61, 61,60,62,

        // Wing right upper
        64,63,65, 65,63,66,
        66,63,67, 67,63,68,
        68,63,69, 69,63,70,
        70,63,71, 71,63,72,
        72,63,73, 73,63,74,
        74,63,75, 75,63,76,
        76,63,77, 77,63,78,
        78,63,79, 79,63,80,
        80,63,81, 81,63,64,

        // Wing right down
        83,82,84, 84,82,85,
        85,82,86, 86,82,87,
        87,82,88, 88,82,89,
        89,82,90, 90,82,91,
        91,82,92, 92,82,93,
        93,82,94, 94,82,95,
        95,82,96, 96,82,97,
        97,82,98, 98,82,99,
        99,82,100, 100,82,101,
        101,82,102, 102,82,83,
        
    };

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Butterfly VI";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size      = sizeof(Indices);
    BufferData IBData;
    IBData.pData    = Indices;
    IBData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_ButterflyIndexBuffer);
}

void Tutorial03_Texturing::LoadTexture()
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> Tex;
    CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &Tex);
    // Get shader resource view from the texture
    m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Set texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}


void Tutorial03_Texturing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();
    LoadTexture();
}

// Render a frame
void Tutorial03_Texturing::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = m_WorldViewProjMatrix;
    }

    // Bind vertex and index buffers
    const Uint64 offset   = 0;
    IBuffer*     pBuffs[] = {m_ButterflyVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_ButterflyIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
    DrawAttrs.IndexType  = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 10000;
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void Tutorial03_Texturing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    // Apply rotation
    float4x4 CubeModelTransform = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-PI_F * 0.1f);

    // Camera is at (0, 0, -5) looking along the Z axis
    float4x4 View = float4x4::RotationZ(PI_F) * float4x4::Translation(10.f, 6.0f, 20.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute world-view-projection matrix
    m_WorldViewProjMatrix =  View * SrfPreTransform * Proj;
}

} // namespace Diligent
