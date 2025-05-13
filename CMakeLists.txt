cmake_minimum_required (VERSION 3.10)

project(Tutorial03_Texturing CXX)

set(SOURCE
    src/Tutorial03_Texturing.cpp
)

set(INCLUDE
    src/Tutorial03_Texturing.hpp
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/DepthGrid.hlsl
)

set(ASSETS
    assets/DGLogo.png
    assets/try.png
    assets/hdrHigh.png
    assets/depth.png
)

add_sample_app("Tutorial03_Texturing" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
