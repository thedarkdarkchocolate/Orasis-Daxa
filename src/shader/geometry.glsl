#include <daxa/daxa.inl>

#include <shared.inl>

// Enables the push constant MyPushConstant 
DAXA_DECL_PUSH_CONSTANT(buffer_ptrs, push)

// ---------------- VERTEX START ----------------
#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX


layout(location = 0) out daxa_f32vec3 out_pos;
layout(location = 1) out daxa_f32vec3 out_norm;
layout(location = 2) out daxa_f32vec3 out_col;
layout(location = 3) out daxa_f32vec2 out_uv;



void main()
{

    Vertex vert = deref_i(push.vertices, gl_VertexIndex);

    mat4 proj_view = deref(push.proj_view_matrices).proj_view;
    mat4 model = deref(push.model_M).model;




    gl_Position = proj_view * model * vec4(vert.position, 1);

    out_pos = vec3(model * vec4(vert.position, 1.0));
    out_col = vert.color;
    out_norm = mat3(transpose(inverse(model))) * vert.normal;
    // out_norm = vert.normal;
    out_uv = vert.uv;

}






// ---------------- VERTEX END ----------------



#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT


// ---------------- FRAGMENT START ----------------



layout(location = 0) in daxa_f32vec3 FragPos;
layout(location = 1) in daxa_f32vec3 FragNorm;
layout(location = 2) in daxa_f32vec3 FragColor;
layout(location = 3) in daxa_f32vec2 FragUV;

layout(location = 0) out daxa_f32vec4 outPos;
layout(location = 1) out daxa_f32vec4 outNormal;
layout(location = 2) out daxa_f32vec4 outColor;




void main()
{

    outPos = vec4(FragPos, 1.0);
    outNormal = vec4(normalize(FragNorm), 1.0);

    if (push.hasTexture)
    {
        outColor = texture(daxa_sampler2D(push.image_view_ID, push.sampler_ID), FragUV * push.uv_scale)
            * push.mat_color;   
    }
    else
        outColor = push.mat_color;

}






// ---------------- FRAGMENT END ----------------

#endif