#include <daxa/daxa.inl>

#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(ssao_blur_ptrs, push)

layout(local_size_x = 8, local_size_y = 8) in;


void main() 
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(textureSize(daxa_texture2D(push.input_view_ID), 0));
    float result = 0.0;

    for(int x_off = -2; x_off < 2; x_off++)
        for(int y_off = -2; y_off < 2; y_off++)
        {
            ivec2 offset_pixel = clamp((pixel + ivec2(x_off, y_off)), ivec2(0), size - 1);
            result += texelFetch(daxa_texture2D(push.input_view_ID), offset_pixel, 0).r;
        }    

    result /= 16;
    imageStore(daxa_image2D(push.output_view_ID), pixel, vec4(result));
}