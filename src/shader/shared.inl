#pragma once
#include <daxa/daxa.inl>

#define MAX_POINT_LIGHTS 16
#define MAX_SSAO_SAMPLES 64

// Vertex and Vertex_HOST on Model.hpp must have the same declarations
struct Vertex
{
    daxa_f32vec3 position;
    daxa_f32vec3 color;
    daxa_f32vec3 normal;
    daxa_f32vec2 uv;
};

struct Projection_View_Matrix {
    daxa_f32mat4x4 proj;
    daxa_f32mat4x4 view;
};

struct Model_Matrix {
    daxa_f32mat4x4 model;
};

struct PointLight {
    daxa_f32vec3 position;
    daxa_f32vec3 color;
    daxa_f32 intensity;
};

struct FrameInfo 
{
    daxa_f32vec3 cameraPos;

    daxa_u32 lightCount;
    PointLight lights[MAX_POINT_LIGHTS];
};

struct G_Buffer
{
    daxa_ImageViewId attachments[3];
};

struct SSAO_Kernel 
{
    daxa_f32vec3 samples[MAX_SSAO_SAMPLES];
    daxa_f32vec3 noise[16];
};

// Allows the shader to use pointers to Vertex
DAXA_DECL_BUFFER_PTR(Vertex)
DAXA_DECL_BUFFER_PTR(Projection_View_Matrix)
DAXA_DECL_BUFFER_PTR(Model_Matrix)
DAXA_DECL_BUFFER_PTR(FrameInfo)
DAXA_DECL_BUFFER_PTR(G_Buffer)
DAXA_DECL_BUFFER_PTR(SSAO_Kernel)

struct buffer_ptrs
{
    daxa_BufferPtr(Vertex) vertices;
    daxa_BufferPtr(Projection_View_Matrix) proj_view_matrices;
    daxa_BufferPtr(Model_Matrix) model_M;
    daxa_BufferPtr(FrameInfo) f_info;
    
    daxa_ImageViewId image_view_ID;
    daxa_SamplerId sampler_ID;
    daxa_b32 hasTexture;
    daxa_f32vec2 uv_scale;
    daxa_f32vec4 mat_color;
    
};

struct ssao_ptrs
{
    daxa_BufferPtr(G_Buffer) gBuffer;
    daxa_BufferPtr(Projection_View_Matrix) proj_view_matrices;
    daxa_BufferPtr(SSAO_Kernel) SSAO;
    
    daxa_SamplerId sampler_ID;
    daxa_ImageViewId output_view_ID;
};

struct ssao_blur_ptrs
{
    daxa_ImageViewId input_view_ID;
    daxa_ImageViewId output_view_ID;
};

struct lighting_ptrs
{
    daxa_BufferPtr(G_Buffer) gBuffer;
    daxa_BufferPtr(FrameInfo) f_info;
    daxa_BufferPtr(Projection_View_Matrix) proj_view_matrices;
    
    daxa_SamplerId sampler_ID;
    daxa_ImageViewId ssao_view_ID;
};