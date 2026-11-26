#include <daxa/daxa.inl>

#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(ssao_ptrs, push)

#define sampleProjPos sampleUVPos
#define sampleNDCPos sampleUVPos
#define sampleWorldPos sampleUVPos

layout(local_size_x = 8, local_size_y = 8) in;

const float bias = 0.025;
const float radius = 0.5;
const int kernelSize = 64;

void main()
{

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    
    ivec2 pMod = pixel % 4;
    int index = pMod.x + pMod.y * 4;
    vec3 randomVec = deref(push.SSAO).noise[index];

    mat4 proj = deref(push.proj_view_matrices).proj;

    vec4 FragPosition = texelFetch(daxa_sampler2D(deref(push.gBuffer).attachments[0], push.sampler_ID), pixel, 0);
    
    // w is 0 for pixels with no geometry(empty space) so we skip
    if (FragPosition.w == 0.0)
    {
        imageStore(daxa_image2D(push.output_view_ID), pixel, vec4(1.0));
        return;
    }

    vec3 FragPos = FragPosition.xyz;
    vec3 FragNorm = texelFetch(daxa_sampler2D(deref(push.gBuffer).attachments[1], push.sampler_ID), pixel, 0).xyz;
    

    vec3 tangent = normalize(randomVec - FragNorm * dot(randomVec, FragNorm));
    vec3 bitangent = cross(FragNorm, tangent);
    mat3 TBN = mat3(tangent, bitangent, FragNorm);

    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; i++)
    {
        // This is the sample in the correct oriantation in perspective with the normal and it's oriantation 
        vec3 samplePosTangent = TBN * deref(push.SSAO).samples[i];
        samplePosTangent = FragPos + samplePosTangent * radius;

        vec4 sampleWorldPos = vec4(samplePosTangent, 1.0);
        sampleProjPos = proj *  sampleWorldPos;
        sampleNDCPos.xyz /= sampleProjPos.w; // w compomnent is written through the proj matrix and dividing the coords with w gives the x,y,z ranges [-1, 1]
        sampleUVPos.xyz  = sampleNDCPos.xyz * 0.5 + 0.5; // and here we are normilizing them between 0, 1 (uv) coords

        vec4 samplePosition = textureLod(daxa_sampler2D(deref(push.gBuffer).attachments[0], push.sampler_ID), sampleUVPos.xy, 0);
        // w is 1 where geometry was drawn and 0 where the G-buffer is still cleared, so the background can't occlude
        if (samplePosition.w == 0.0)
            continue;

        float actualDepthAtSample = samplePosition.z;
        
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(FragPos.z - actualDepthAtSample));
        occlusion += (actualDepthAtSample <= sampleUVPos.z - bias ? 1.0 : 0.0) * rangeCheck;   
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    imageStore(daxa_image2D(push.output_view_ID), pixel, vec4(occlusion));

}