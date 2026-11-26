#include <daxa/daxa.inl>

#include <shared.inl>

// Enables the push constant MyPushConstant 
DAXA_DECL_PUSH_CONSTANT(lighting_ptrs, push)

// ---------------- VERTEX START ----------------
#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

#define fr (deref(push.f_info))

const vec2 wholeScreenTriangle[3] = {{-1, 3}, {-1, -1}, {3, -1}};

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_Frag_world_pos;

void main()
{
    vec2 pos = wholeScreenTriangle[gl_VertexIndex];

    gl_Position = daxa_f32vec4(pos, 0.0, 1.0);
    out_uv = pos * 0.5 + 0.5; 

    // Here I calculate the the world space position for 3 vertices(All fragments) from NDC coords 
    // It is used to find the direction for the skybox 
    out_Frag_world_pos = fr.viewToWorld * fr.projToView * vec4(pos.x , pos.y, 1, 1);
}






// ---------------- VERTEX END ----------------



#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT


// ---------------- FRAGMENT START ----------------

#define fr (deref(push.f_info))

layout(location = 0) in vec2 FragUV;
layout(location = 1) in vec4 in_Frag_world_pos;

layout(location = 0) out daxa_f32vec4 outColor;
layout(location = 1) out daxa_f32vec4 outBloomColor;


float ambientStrenght = 0.001;


float ShadowCalculation(vec4 fragPosLSpc, float bias)
{
    // NDC [-1, 1]
    vec3 projectedCoords = fragPosLSpc.xyz / fragPosLSpc.w;
    // [0, 1]
    projectedCoords.xy = projectedCoords.xy * 0.5 + 0.5; 
    float closestDepth = texture(daxa_sampler2D(push.shadow_map_view_ID, push.sampler_clamp_to_edge_ID), projectedCoords.xy).r;
    
    float shadow = projectedCoords.z - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}

void main()
{
    vec3 FragPos = texture(daxa_sampler2D(deref(push.gBuffer).attachments[0], push.sampler_ID), FragUV).xyz;

    if (FragPos == vec3(0))
    {
        outColor = texture(daxa_samplerCube(push.skybox_view_ID, push.sampler_ID), in_Frag_world_pos.xyz/in_Frag_world_pos.w - fr.cameraPos);
        outBloomColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 tmpNorm = texture(daxa_sampler2D(deref(push.gBuffer).attachments[1], push.sampler_ID), FragUV);
    vec3 FragAlbido = texture(daxa_sampler2D(deref(push.gBuffer).attachments[2], push.sampler_ID), FragUV).xyz;

    // The w comp stores if the fragment is owned by a light if yes its value is > 0
    if (tmpNorm.w > 0.0)
    {
        outColor = vec4(FragAlbido * tmpNorm.w, 1.0);
        outBloomColor = outColor;
        return;
    }

    vec3 FragNorm = tmpNorm.xyz;
    float AmbientOcclusion = texture(daxa_sampler2D(push.ssao_view_ID, push.sampler_ID), FragUV).r;
    
    vec3 lighting_result = vec3(0);


    for (int i = 0; i < fr.plightCount; i++)
    {
        vec3 lightPos = vec3(deref(push.proj_view_matrices).view * vec4(fr.plights[i].position, 1.0));
        
        float dist = distance(lightPos, FragPos);
        float attenuation = 1.0/max((dist * dist), 0.01); 


        vec3 norm = normalize(FragNorm);
        vec3 lightDir = normalize(lightPos - FragPos);
        vec3 diffuse = max(dot(lightDir, norm), 0.0) * fr.plights[i].color;
        // The reflect excpect the vector to be from the lights source to the fragment so we have to recverse the lightdir
        float spec = pow(max(dot(reflect(-lightDir, norm), normalize(-FragPos)), 0.0), 64);

        lighting_result += (diffuse + spec * fr.plights[i].color) * attenuation * fr.plights[i].intensity;

    }

    for (int i = 0; i < fr.dlightCount; i++)
    {
        vec3 norm = normalize(FragNorm);
        vec3 l_dir = normalize(mat3(deref(push.proj_view_matrices).view) * -fr.dlights[i].dir);

        vec3 diffuse = max(dot(l_dir, norm), 0.0) * fr.dlights[i].color;
        // The reflect excpect the vector to be from the lights source to the fragment so we have to recverse the lightdir
        float spec = pow(max(dot(reflect(-l_dir, norm), normalize(-FragPos)), 0.0), 64);

        // Its a bias that solves acne. It help neighboor fragment to have the correct depth value relative to texel that can be spread across many fragments
        // in the case a fragment is a little to right of the center of texel the depth value changes by a bit
        // so it might be a bit behind the relative to texel and it will be drawn as a shadow 
        float shadow_texel_bias = max(0.05 * (1.0 - dot(norm, l_dir)), 0.005);
        lighting_result += (diffuse + spec * fr.dlights[i].color) * (1 - ShadowCalculation(fr.dlights[i].viewToLightSpace * vec4(FragPos, 1.0), shadow_texel_bias)) * fr.dlights[i].intensity;

    }


    lighting_result += ambientStrenght;
    lighting_result *= AmbientOcclusion;

    outColor = vec4(FragAlbido, 1.0) * vec4(lighting_result, 1.0);


    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outBloomColor = vec4(outColor.rgb, 1.0);
    else
        outBloomColor = vec4(0.0, 0.0, 0.0, 1.0);
}






// ---------------- FRAGMENT END ----------------

#endif