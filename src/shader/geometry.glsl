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
layout(location = 4) out daxa_f32vec3 out_tang;
layout(location = 5) out daxa_f32vec3 out_bitang;



void main()
{

    Vertex vert = deref_i(push.vertices, gl_VertexIndex);

    mat4 proj = deref(push.proj_view_matrices).proj;
    mat4 view = deref(push.proj_view_matrices).view;
    mat4 model = deref(push.model_M).model;




    gl_Position = proj * view * model * vec4(vert.position, 1);

    out_pos = vec3(view * model * vec4(vert.position, 1.0));
    out_col = vert.color;
    out_norm = mat3(transpose(inverse(view * model))) * vert.normal;
    // out_norm = normalize(vert.normal);
    out_uv = vert.uv;

    out_tang = mat3(view * model) * vert.tangent.xyz;
    out_bitang = cross(out_tang, out_norm) * vert.tangent.w;


}






// ---------------- VERTEX END ----------------



#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT


// ---------------- FRAGMENT START ----------------



layout(location = 0) in daxa_f32vec3 FragPos;
layout(location = 1) in daxa_f32vec3 FragNorm;
layout(location = 2) in daxa_f32vec3 FragColor;
layout(location = 3) in daxa_f32vec2 FragUV;
layout(location = 4) in daxa_f32vec3 FragTang;
layout(location = 5) in daxa_f32vec3 FragBitang;

layout(location = 0) out daxa_f32vec4 outPos;
layout(location = 1) out daxa_f32vec4 outNormal;
layout(location = 2) out daxa_f32vec4 outColor;




void main()
{

    outPos = vec4(FragPos, 1.0);
    if(push.hasNormalMap)
    {
        vec3 normal_T = texture(daxa_sampler2D(push.normal_map_view_ID, push.sampler_ID),
                 FragUV * push.uv_scale).xyz;

        normal_T = (normal_T - vec3(0.5)) * 2.0;
        outNormal = vec4(normalize(mat3(normalize(FragTang), normalize(FragBitang), normalize(FragNorm)) * 
            normal_T), push.emmisive);
    }
    else
        outNormal = vec4(normalize(FragNorm), push.emmisive);


    if (push.hasTexture)
    {
        outColor = texture(daxa_sampler2D(push.image_view_ID, push.sampler_ID), FragUV * push.uv_scale)
            * push.mat_color;   
    

    }
    else
        outColor = push.mat_color;

    if (outColor.a < 0.5)
        discard;

}






// ---------------- FRAGMENT END ----------------

#endif