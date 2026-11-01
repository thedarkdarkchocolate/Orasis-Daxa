#include <daxa/daxa.inl>

#include <shared.inl>

// Enables the push constant MyPushConstant 
DAXA_DECL_PUSH_CONSTANT(lighting_ptrs, push)

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


float ambientStrenght = 0.1;

void main()
{
    
    FrameInfo fr = deref(push.f_info);

    vec3 FragPos = texture(daxa_sampler2D(deref(push.gBuffer).attachments[0], push.sampler_ID), FragUV).xyz;
    vec3 FragNorm = texture(daxa_sampler2D(deref(push.gBuffer).attachments[1], push.sampler_ID), FragUV).xyz;
    vec3 FragAlbido = texture(daxa_sampler2D(deref(push.gBuffer).attachments[2], push.sampler_ID), FragUV).xyz;
    float AmbientOcclusion = texture(daxa_sampler2D(push.ssao_view_ID, push.sampler_ID), FragUV).r;
    
    vec3 lighting_result = vec3(0);


    for (int i = 0; i < fr.lightCount; i++)
    {
        vec3 lightPos = vec3(deref(push.proj_view_matrices).view * vec4(fr.lights[i].position, 1.0));
        
        float dist = distance(lightPos, FragPos);
        float attenuation = 1.0/max((dist * dist), 0.01); 


        vec3 norm = normalize(FragNorm);
        vec3 lightDir = normalize(lightPos - FragPos);
        vec3 diffuse = max(dot(lightDir, norm), 0.0) * fr.lights[i].color;
        // The reflect excpect the vector to be from the lights source to the fragment so we have to recverse the lightdir
        float spec = pow(max(dot(reflect(-lightDir, norm), normalize(-FragPos)), 0.0), 64);

        lighting_result += (diffuse + spec * fr.lights[i].color) * attenuation * fr.lights[i].intensity;

    }

    lighting_result += ambientStrenght;
    lighting_result *= AmbientOcclusion;

    outColor = vec4(FragAlbido, 1.0) * vec4(lighting_result, 1.0);

}






// ---------------- FRAGMENT END ----------------

#endif