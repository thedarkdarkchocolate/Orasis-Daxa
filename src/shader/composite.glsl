#include <daxa/daxa.inl>

#include <shared.inl>

// Enables the push constant MyPushConstant 
DAXA_DECL_PUSH_CONSTANT(composite_ptrs, push)

// ---------------- VERTEX START ----------------
#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX


const vec2 wholeScreenTriangle[3] = {{-1, 3}, {-1, -1}, {3, -1}};

layout(location = 0) out vec2 out_uv;

void main()
{
    vec2 pos = wholeScreenTriangle[gl_VertexIndex];

    gl_Position = daxa_f32vec4(pos, 0.0, 1.0);
    out_uv = pos * 0.5 + 0.5; 
}




// ---------------- VERTEX END ----------------



#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT


// ---------------- FRAGMENT START ----------------

layout(location = 0) in vec2 FragUV;

layout(location = 0) out daxa_f32vec4 outColor;

void main()
{
    const float gamma = 2.2;
    vec3 hdrColor = texture(daxa_sampler2D(push.hdr_view_ID, push.sampler_ID), FragUV).rgb;      
    vec3 bloomColor = texture(daxa_sampler2D(push.bloom_view_ID, push.sampler_ID), FragUV).rgb;

    hdrColor += bloomColor; // additive blending
    // tone mapping
    vec3 result = vec3(1.0) - exp(-hdrColor * push.exposure);
    // also gamma correct while we're at it       
    result = pow(result, vec3(1.0 / gamma));
    outColor = vec4(result, 1.0);


}






// ---------------- FRAGMENT END ----------------

#endif