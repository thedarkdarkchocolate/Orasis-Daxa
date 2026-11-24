#include <daxa/daxa.inl>

#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(bloom_ptrs, push)

layout(local_size_x = 8, local_size_y = 8) in;

float weight[5] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};

void main()
{

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(textureSize(daxa_texture2D(push.input_view_ID), 0));

    vec3 result = texelFetch(daxa_texture2D(push.input_view_ID), pixel, 0).rgb * weight[0];

    if(push.horizontal)
    {
        for(int offset = 1; offset < 5; offset++)
        {
            result += texelFetch(daxa_texture2D(push.input_view_ID), clamp(pixel + ivec2(offset, 0), ivec2(0), size - 1), 0).rgb * weight[offset];
            result += texelFetch(daxa_texture2D(push.input_view_ID), clamp(pixel - ivec2(offset, 0), ivec2(0), size - 1), 0).rgb * weight[offset];
        }
    }
    else
        for(int offset = 1; offset < 5; offset++)
        {
            result += texelFetch(daxa_texture2D(push.input_view_ID), clamp(pixel + ivec2(0, offset), ivec2(0), size - 1), 0).rgb * weight[offset];
            result += texelFetch(daxa_texture2D(push.input_view_ID), clamp(pixel - ivec2(0, offset), ivec2(0), size - 1), 0).rgb * weight[offset];
        }



    imageStore(daxa_image2D(push.output_view_ID), pixel, vec4(result, 1.0));

}