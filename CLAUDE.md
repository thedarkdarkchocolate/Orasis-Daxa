# Orasis-Daxa

A learning renderer in C++20 on top of [Daxa](https://github.com/Ipotrick/Daxa) (a thin wrapper over Vulkan). The owner is
building it to learn graphics programming, following LearnOpenGL's topics but in Daxa. Windows, MSVC, AMD RX 7800 XT.

## Working with the owner

- **This is a learning project. Guide, don't hand over solutions**, unless they say "do it" for a specific piece. Default to:
  the concept in plain words, what to look for and where, and a checkpoint that proves it works. Then review what they wrote.
- When they ask you to write something, write only that piece, then walk through each changed line.
- Give exact Daxa/cgltf names freely (struct fields, enum values, function names). Looking those up is not the learning;
  the concepts are. Read the headers under `build/Debug/_deps/` to get them right for the fetched version.
- They are solid on ideas and weaker on C++ fundamentals (pointers and null checks, initialisation, arrays). Point at the
  exact line and say why; don't assume a one-line hint is enough.
- Keep a running checklist of the current step at the end of each reply; they asked for it so they don't have to scroll.
- One new concept at a time, with something visible on screen after each step.

## Build and run

```
cmake --preset Debug
cmake --build build/Debug --config Debug
build\Debug\Debug\learndaxa.exe
```

- Dependencies (GLFW, Daxa `master`, glm, cgltf, stb_image) are fetched by `cmake/deps.cmake` into `build/Debug/_deps/`.
- Shaders are **not** compiled by the build. Daxa's pipeline manager compiles the GLSL at program start, so shader errors
  appear in the console (stderr) at launch, not in the build output.
- The app captures the mouse and its stdout is buffered. To run it from a tool: start it with stdout/stderr redirected to
  files, wait a few seconds, close it with `CloseMainWindow()` (not a kill, or the output is lost), then read the files.
- Daxa prints `Detected leaked images/buffers` at exit if a resource was not destroyed. An early `return -1` in `main`
  (for example after a shader error) also triggers it, because resources created before that point are not cleaned up.
- No tests. Not a git repository.

## Layout

| Path | What it holds |
|---|---|
| `src/main.cpp` | Everything at the top level: window, device, swapchain, depth image, G-buffer images, pipelines, scene setup, two task graphs (`Forward_Graph`, `Deferred_Graph`), main loop, cleanup. |
| `include/Model.hpp` | `Model`: loads a `.glb` with cgltf, decodes textures with stb_image, uploads vertex/index buffers and images. |
| `include/GameObjet.hpp` | `TransformComponent` (translation, scale, YXZ rotation) and `GameObject` (model pointer, transform, its own model-matrix buffer). Note the file name is misspelled; `include/prevHeaders/GameObject.hpp` is an old, unused version. |
| `include/Scene.hpp` | `Scene`: owns models (cached by path) and game objects (by id). `add(path, transform)` returns the id. |
| `include/Camera.hpp` | View and projection matrices. |
| `include/KmbMovementController.hpp` | WASD + mouse look. Tab toggles mouse capture, Esc quits, Shift speeds up. |
| `include/App.hpp` | `AppWindow`: GLFW window, resize/minimise flags, native handle for Daxa. |
| `include/Daxa_GLM_helper.hpp` | `to_daxa(...)` conversions from glm to Daxa types (mat4, vec4, vec3, vec2). |
| `src/shader/shared.inl` | Structs shared by C++ and GLSL (compiled by both). |
| `src/shader/forward.glsl` | Forward path (`Forward_Graph`): vertex + fragment in one file, split by `DAXA_SHADER_STAGE`; shades in one pass. |
| `src/shader/geometry.glsl` | Deferred geometry pass: same vertex shader as forward; fragment writes position, normal, albedo to the G-buffer (no lighting). |
| `src/shader/lighting.glsl` | Deferred lighting pass: fullscreen triangle from `gl_VertexIndex`; fragment reads the G-buffer and runs the same light loop as forward. |
| `src/cgltf.cpp`, `src/stb_image.cpp` | The one translation unit each that compiles those single-header libraries. |
| `assets/glb/` | `Box.glb` (red material, no textures) and `SheenChair.glb` (4 meshes, 7 images, uses `KHR_texture_transform`). |

## How a frame comes together

```
LOAD (once per model)                                       Model.hpp
  .glb -- cgltf --> nodes --> meshes --> primitives
     per primitive:  vertices (node transform baked in), indices  --> one vertex buffer, one index buffer per model
                     base colour image --> stb decode --> staging buffer --> copy --> image, slot = cgltf_image_index
                     uv scale, base colour factor                 --> a Draw_Indices record

DRAW (every frame, Deferred_Graph by default)               main.cpp
  task "deferred Geometry"  (writes position, normals, albido + depth)              geometry.glsl
    for each object: set_index_buffer once
      for each Draw_Indices record:
         push_constant(buffer_ptrs): buffer addresses, image view id, sampler id, hasTexture, uv_scale, mat_color
         draw_indexed(first_index = start, index_count = count)
    vertex:    Vertex read by address with gl_VertexIndex; position * model * proj_view; passes world pos, normal, uv
    fragment:  location 0 = world position, 1 = normalize(normal), 2 = (texture(image, uv * uv_scale) or 1) * mat_color

  task "deferred Lighting"  (samples the 3 G-buffer images, writes swapchain)       lighting.glsl
    push_constant(lighting_ptrs): address of the G_Buffer ids buffer, f_info, sampler id
    draw(vertex_count = 3): one triangle covering the screen
    fragment:  read pos/normal/albedo at uv; albedo * (ambient + sum over lights of diffuse + specular)

Forward_Graph (one task, forward.glsl) does the same shading in one pass straight into the swapchain.
```

- The G-buffer is three `ExternalTaskImage`s (`position_image` `R32G32B32A32_SFLOAT`, `normals_image`
  `R16G16B16A16_SFLOAT`, `albido_image` `R8G8B8A8_SRGB`), usage `COLOR_ATTACHMENT | SHADER_SAMPLED`. Location N in
  `geometry.glsl` = index N of the pipeline's `color_attachments` = index N of `begin_renderpass`'s `color_attachments`.
- The lighting task's `.fragment_shader.samples(...)` is what makes the graph put the barrier (and the ordering) between
  the two passes.
- On resize: destroy, `createGBuffer`, `set_image` on each task image, then `rewrite_GBuffer_views()` (the IDs buffer).

- `Model::Draw_Indices` is CPU-only: `start`, `count` (positions in the index buffer), `slot` (index into
  `m_images_IDs`, -1 for no texture), `uv_scale`, `mat_color`.
- Vertex pulling: there are no vertex attributes. The vertex shader reads `Vertex` from the buffer by device address.
- Y is up, like glTF and LearnOpenGL. Vulkan's screen Y points down, so the flip lives in one place: the negated
  `projMatrix[1][1]` in `Camera::setPrespectiveProjection`. Models need no rotation to stand upright. The controller's
  `upDir` is `(0, 1, 0)` and its mouse pitch uses `+=`. The flip also reverses triangle winding, which matters once
  back-face culling is turned on.

## Daxa conventions that matter here

- **IDs, host pointers, device addresses.** An ID is the handle for Daxa calls. `buffer_host_address_as<T>(id)` is the CPU
  pointer for writing. `device_address(id)` is what a shader needs for a buffer. Images and samplers have no address: the
  shader gets the ID itself (`image.default_view()`, the `SamplerId`), and uses `daxa_sampler2D(view_id, sampler_id)`.
- **`shared.inl` is compiled by C++ and GLSL.** Use only `daxa_` types in it (`daxa_f32vec2`, `daxa_b32`, `daxa_ImageViewId`,
  ...). Plain `bool` or `daxa::types::...` breaks one side. `DAXA_DECL_BUFFER_PTR(T)` must precede any `daxa_BufferPtr(T)`.
- **Designated initialisers must follow declaration order** (`push_constant`, pipeline info, `SamplerInfo`). New fields in
  `buffer_ptrs` go at the end, and the push in `main.cpp` must list them in the same order.
- **Push constant holds per-draw values**; anything large or shared goes in a buffer and is pushed by address.
- **Image upload**: `create_image` (usage `TRANSFER_DST | SHADER_SAMPLED`) -> host-writable staging buffer + `memcpy` ->
  own `CommandRecorder`: image barrier (`TO_GENERAL`, dst `TRANSFER_WRITE`), `copy_buffer_to_image`, barrier to
  `FRAGMENT_SHADER_READ` -> `complete_current_commands` -> `submit_commands`. Base colour uses `R8G8B8A8_SRGB`; data maps
  (normal, roughness, occlusion) will need `R8G8B8A8_UNORM`.
- **Destruction is deferred.** `destroy_*` only queues; `collect_garbage()` (once per frame, and after `wait_idle()` at
  exit) frees what the GPU has finished with.
- **Task graph** only needs to know images whose state changes per frame (swapchain, depth, G-buffer), through
  `ExternalTaskImage` + `register_image`. Static textures are not registered. Buffers written by the CPU need no
  declaration; only GPU-written-then-GPU-read ones do (`ExternalTaskBuffer` + `register_buffer` + `.reads`/`.writes`).
- **Two graphs, one at a time.** `Forward_Graph` and `Deferred_Graph` share the task images; the main loop executes
  one of them (switch by commenting the other `execute` line).
- **Sampler defaults are clamp**; the chair needs `REPEAT` on both `address_mode_u` and `address_mode_v`.
- `CommandRecorder` (copies, barriers) and `RenderCommandRecorder` (draws) are the same list in two modes;
  `begin_renderpass` / `end_renderpass` switch between them.

## Status

Progress (dated, oldest first; add a new dated entry, never rewrite an old one):

- **2026-10-15**: glTF loading through nodes, textures per primitive, UV scale, base colour factor, depth test
  (recreated on resize), Tab toggle, Y-up. Point lights: `Scene::addLight(path, intensity, colour, transform)` creates a
  `GameObject` with a `PointLightComponent` (intensity, `light_color`) and records its ID in
  `Scene::m_lightObjects_IDs`. Each frame `update_FrameInfo` in `main.cpp` copies camera position and all lights into
  the `FrameInfo` buffer (`MAX_POINT_LIGHTS` array + `lightCount`), pushed as `f_info`. Forward Phong ambient + diffuse
  + specular over all lights; normals use the inverse-transpose normal matrix in the vertex shader.
- **2026-10-25**: Deferred shading works and gives the same picture as forward (checked by the owner, including after
  resize). Both paths exist side by side as separate task graphs; `Deferred_Graph` is the one executed.

Open (current; remove items as they are done):
- Lighting (LearnOpenGL "Basic Lighting" -> "Light casters"): light cubes are lit like everything else (they push their
  `light_color` as `mat_color`; in deferred an unlit marker would go in the G-buffer, e.g. normal `.w = 0`);
  `PointLight.intensity` is sent but unused; no attenuation yet; `lightCount` is not capped at `MAX_POINT_LIGHTS`.
- Deferred: the G-buffer view IDs reach `lighting.glsl` through a host-written buffer (`G_Buffer`, `g_Buffer_views_ID`,
  `rewrite_GBuffer_views`, `task_buffer`). Planned: push `ti.get(image).view_ids[0]` x3 directly in `lighting_ptrs` and
  delete the buffer, the lambda, the task buffer and the `G_Buffer` struct (removes a one-frame race on resize).
- Deferred: background pixels come out black (albedo cleared to alpha 0); nothing checks alpha yet.
- `KHR_texture_transform` offset and rotation are ignored (only scale is applied).
- `Model.hpp`, non-indexed branch: `draw_range.count = primative.indices->count` dereferences a null pointer; should be
  `pos->count`. Not hit by the current assets.
- `Model::m_hasTexture` and `Model::m_empty_ImageID` are leftovers and can be removed.
- No mipmaps. The normal matrix is computed per vertex in the shader (`inverse` per vertex); it could be computed once
  per object on the CPU.

Update this section when a step is finished: append a dated entry to Progress (the dates are the owner's record of
progress) and update Open.
