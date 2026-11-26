#include <daxa/daxa.inl>

#include <shared.inl>

// Enables the push constant MyPushConstant 
DAXA_DECL_PUSH_CONSTANT(shadow_ptrs, push)

// ---------------- VERTEX START ----------------
#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

#define fr (deref(push.f_info))


void main()
{

    Vertex vert = deref_i(push.vertices, gl_VertexIndex);

    mat4 model = deref(push.model_M).model;

    gl_Position = fr.dlights[0].lightSpace * model * vec4(vert.position, 1);

}





// ---------------- VERTEX END ----------------



#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT


// ---------------- FRAGMENT START ----------------


void main()
{}






// ---------------- FRAGMENT END ----------------

#endif