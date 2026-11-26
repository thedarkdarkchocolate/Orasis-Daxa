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
build\Debug\Debug\Orasis_daxa.exe
```

- Dependencies (GLFW, Daxa `master`, glm, cgltf, stb_image) are fetched by `cmake/deps.cmake` into `build/Debug/_deps/`.
- Shaders are **not** compiled by the build. Daxa's pipeline manager compiles the GLSL at program start, so shader errors
  appear in the console (stderr) at launch, not in the build output.
- The app captures the mouse and its stdout is buffered. To run it from a tool: start it with stdout/stderr redirected to
  files, wait a few seconds, close it with `CloseMainWindow()` (not a kill, or the output is lost), then read the files.
- Daxa prints `Detected leaked images/buffers` at exit if a resource was not destroyed. An early `return -1` in `main`
  (for example after a shader error) also triggers it, because resources created before that point are not cleaned up.
- No tests. It is a git repository (`main`); commit only when the owner asks.

## Layout

| Path | What it holds |
|---|---|
| `src/main.cpp` | Everything at the top level: window, device, swapchain, depth image, G-buffer images, pipelines, scene setup, two task graphs (`Forward_Graph`, `Deferred_Graph`), main loop, cleanup. |
| `include/Model.hpp` | `Model`: loads a `.glb` with cgltf, decodes textures with stb_image, uploads vertex/index buffers and images. |
| `include/GameObjet.hpp` | `TransformComponent` (translation, scale, YXZ rotation) and `GameObject` (model pointer, transform, its own model-matrix buffer). Note the file name is misspelled; `include/prevHeaders/GameObject.hpp` is an old, unused version. |
| `include/Scene.hpp` | `Scene`: owns models (cached by path) and game objects (by id). `add(path, transform)` returns the id. |
| `include/Camera.hpp` | View and projection matrices. |
| `include/KmbMovementController.hpp` | WASD + mouse look. The grave-accent key (left of 1) toggles mouse capture, Esc quits, Shift speeds up. |
| `include/App.hpp` | `AppWindow`: GLFW window, resize/minimise flags, native handle for Daxa. |
| `include/Daxa_GLM_helper.hpp` | `to_daxa(...)` conversions from glm to Daxa types (mat4, vec4, vec3, vec2). |
| `src/shader/shared.inl` | Structs shared by C++ and GLSL (compiled by both). |
| `src/shader/forward.glsl` | Forward path (`Forward_Graph`): vertex + fragment in one file, split by `DAXA_SHADER_STAGE`; shades in one pass. |
| `src/shader/geometry.glsl` | Deferred geometry pass: vertex shader outputs view-space position, normal, tangent, bitangent; fragment applies the normal map and writes position, normal, albedo to the G-buffer (no lighting). |
| `src/shader/ssao.glsl`, `src/shader/ssao_blur.glsl` | Compute passes: SSAO from the G-buffer position and normal, then a 4x4 box blur. Each writes an `R8_UNORM` storage image. |
| `src/shader/lighting.glsl` | Deferred lighting pass: fullscreen triangle from `gl_VertexIndex`; fragment reads the G-buffer and the blurred SSAO and lights in view space. |
| `src/shader/composite.glsl` | Final pass: fullscreen triangle; samples `hdr_image`, tone maps, applies gamma, writes the swapchain. |
| `src/cgltf.cpp`, `src/stb_image.cpp` | The one translation unit each that compiles those single-header libraries. |
| `assets/glb/` | `Box.glb` (red material, no textures) and `SheenChair.glb` (4 meshes, 7 images, uses `KHR_texture_transform`). `Sponza.glb` (Khronos sample, packed from `.gltf` into one `.glb` so images are embedded; 103 primitives, 69 images, has `TANGENT`, node scale 0.008, 3 alpha-`MASK` materials; Cryengine license in `Sponza_LICENSE.md`, ~50 MB). |

## How a frame comes together

```
LOAD (once per model)                                       Model.hpp
  .glb -- cgltf --> nodes --> meshes --> primitives
     per primitive:  vertices (node transform baked in), indices  --> one vertex buffer, one index buffer per model
                     base colour (SRGB) and normal map (UNORM) --> Model::loadTexture: stb decode --> staging buffer
                        --> copy --> image, slot = cgltf_image_index
                     uv scale, base colour factor                 --> a Draw_Indices record

DRAW (every frame, Deferred_Graph by default)               main.cpp
  task "deferred Geometry"  (writes position, normals, albido + depth)              geometry.glsl
    for each object: set_index_buffer once
      for each Draw_Indices record:
         push_constant(buffer_ptrs): buffer addresses, image and normal map view ids, sampler id, hasTexture, hasNormalMap,
                                     uv_scale, mat_color
         draw_indexed(first_index = start, index_count = count)
    vertex:    Vertex read by address with gl_VertexIndex; passes view-space pos, normal, tangent, bitangent, plus uv
    fragment:  location 0 = view position, 1 = view normal (through the TBN if there is a normal map),
               2 = texture(image, uv * uv_scale) * mat_color, or mat_color alone; discards alpha < 0.5

  task "SSAO compute"  (samples position, normals; writes ssao_image)               ssao.glsl
  task "SSAO blur compute"  (samples ssao_image; writes ssao_blur_image)            ssao_blur.glsl

  task "deferred Lighting"  (samples G-buffer and ssao_blur_image, writes swapchain) lighting.glsl
    push_constant(lighting_ptrs): address of the G_Buffer ids buffer, f_info, proj_view_matrices, sampler id, ssao view id
    draw(vertex_count = 3): one triangle covering the screen
    fragment:  read pos/normal/albedo/AO at uv; lights moved to view space with attenuation and intensity; result * AO

Forward_Graph (one task, forward.glsl) does the same shading in one pass straight into the swapchain.
```

- The G-buffer is three `ExternalTaskImage`s (`position_image` `R32G32B32A32_SFLOAT`, `normals_image`
  `R16G16B16A16_SFLOAT`, `albido_image` `R8G8B8A8_SRGB`), usage `COLOR_ATTACHMENT | SHADER_SAMPLED`. Location N in
  `geometry.glsl` = index N of the pipeline's `color_attachments` = index N of `begin_renderpass`'s `color_attachments`.
- The lighting task's `.fragment_shader.samples(...)` is what makes the graph put the barrier (and the ordering) between
  the two passes.
- On resize: `recreateTaskImage` for depth and both SSAO images (keeps every `ImageInfo` field, new size, `set_image`).
  G-buffer: destroy, `createGBuffer`, `set_image` on each task image, then `rewrite_GBuffer_views()` (the IDs buffer).

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
- **2026-11-01**: SSAO committed (`c680dd1`): compute passes for SSAO and blur, applied in `lighting.glsl`. The deferred
  path now works in view space. Forward stays in world space with no attenuation or AO, so the two no longer give the same picture.
- **2026-11-02**: In the working tree, not committed and not yet confirmed on screen by the owner: normal mapping
  (`Model::loadTexture`, `Vertex.tangent`, TBN in `geometry.glsl`) and `recreateTaskImage` for resize. Its assert came from
  copying `ImageInfo.flags` into `.usage`, which gave usage 0.
- **2026-11-23**: In the working tree, not committed: HDR. Lighting writes `hdr_image` (`R16G16B16A16_SFLOAT`); a new
  fullscreen pass `composite.glsl` samples it, applies Reinhard and gamma (pushed, 2.2) and writes the swapchain. The
  swapchain is `R8G8B8A8_UNORM`, so gamma must be done by hand; everything before this was displayed too dark. Owner
  verified no light + no ambient gives a black screen. Emissive light cubes: `buffer_ptrs.emmisive` (light intensity, 0
  otherwise) is written to the normals image `.w` in `geometry.glsl`; `lighting.glsl` outputs `albedo * w` and returns
  early when `w > 0`. Five coloured lights added in `main.cpp`. FPS counter in the console (143 FPS = vsync/FIFO cap).
- **2026-11-24**: In the working tree, not committed: directional light + first shadow map, seen on screen by the owner.
  `DirectionalLightComponent` (dir, colour, intensity, `lightSpace`, `updateLightSpace(center, radius)` with a local
  `Camera`); `Scene` keeps `m_point_light_Objects_IDs` / `m_directional_light_Objects_IDs`, `add_point_Light` /
  `add_directional_Light`, `m_shadowCenter` / `m_shadowRadius`. `FrameInfo` has `plights`/`dlights` with capped counts;
  `DirectionalLight` carries `lightSpace` (world → light, shadow pass) and `viewToLightSpace` (view → light, lighting
  pass). Shadow pass: task "Shadow Depth", `shadow_depth.glsl`, 1024² `D32_SFLOAT`. `lighting.glsl` has a directional loop
  with `ShadowCalculation`, no bias yet (acne visible). `lighting.glsl` reads `FrameInfo` through
  `#define fr (deref(push.f_info))` instead of copying it (8.0 → 5.5 ms). The earlier "143 FPS = vsync cap" was the
  frame cost, not vsync.
- **2026-11-25**: Owner reports bloom finished and shadow bias added (not committed). Bloom: lighting writes a second
  output `bloom_image` (MRT, luminance > 1, emissive cubes go in whole); `bloom_blur.glsl` compute, 9-tap separable
  Gaussian, ping-pong between `bloom_image` and `bloom_pong_image` (`iter_amount` passes, `bloom_output` = last
  destination); `composite.glsl` adds bloom before tone mapping, which is now exposure (`1 - exp(-hdr * exposure)`,
  exposure pushed as 1.0). Shadow: slope-scaled bias `max(0.05 * (1 - dot(n, l)), 0.005)` in `lighting.glsl`. Present
  mode is now `FIFO_RELAXED`.
- **2026-11-25**: Dear ImGui added (not committed), clean exit verified. `DAXA_ENABLE_UTILS_IMGUI ON` in `deps.cmake`
  (Daxa fetches ImGui and its GLFW backend). Setup after the swapchain: `ImGui::CreateContext` (pointer kept), then
  `ImGui_ImplGlfw_InitForVulkan(window, true)`, then `daxa::ImGuiRenderer` (swapchain format; it owns its pipeline and
  picks a gamma shader for UNORM targets). Each frame: backend `NewFrame`, `ImGui::NewFrame`, a "UI" window with an
  `exposure` slider (captured by the composite task), `ImGui::Render`. Task "ImGui interface" after composite calls
  `record_commands` (it begins its own render pass with `LOAD`). Exit, reverse order before the final
  `collect_garbage`: `ImGui_render = {}`, `ImGui_ImplGlfw_Shutdown`, `DestroyContext`. Mouse-capture toggle moved from
  Tab to the grave-accent key (`GLFW_KEY_GRAVE_ACCENT`). Project renamed: the exe is now `Orasis_daxa.exe`.
- **2026-11-26**: Skybox cubemap shows on screen (owner, not committed). `Scene::add_skybox(paths)` loads 6 JPGs
  (`assets/skybox/LearnOpenGL/`, order right, left, top, bottom, front, back) with stb forced to 4 channels into one
  `R8G8B8A8_SRGB` image with `COMPATIBLE_CUBE` and 6 layers, one staging buffer + submit per face; `m_skybox_ID` is
  destroyed in `Scene`'s cleanup. The view goes to the shader as `cubemap_view_ID` (`main.cpp`).

Open (current; remove items as they are done):
- `Scene::add_skybox` upload bugs that happen to work on this driver (usage is now `TRANSFER_DST`, fixed): the
  `TO_GENERAL` barrier (`ImageBarrierInfo` has no slice, so it covers all 6 layers, from `UNDEFINED`, with
  `src_access = NONE`) runs once per face, in a separate submit, so it may discard or race faces already uploaded. On a
  second call, `destroy_image` leaves the stale ID in `m_skybox_ID`, `is_empty()` stays false, nothing is recreated,
  Daxa aborts on the destroyed ID, and the old view is never destroyed. `stbi_set_flip_vertically_on_load(true)`:
  cubemap faces are not flipped (LearnOpenGL doesn't flip them), and the flag is global stb state.
- Skybox direction in `lighting.glsl:68`: `vec3(FragUV, 1.0) - (view * (0,0,-1,0)).xyz` is not the ray through the
  pixel (UV is 0..1, not NDC; the result is in view space, but the cube is in world space). Expected: the sky stretches
  or slides when looking around. The usual approach: NDC → inverse projection → view-space ray →
  `transpose(mat3(view))` → world. `GameObject::createSkybox()` is an empty leftover.
- **FIX: remove the `GPU.wait_idle()` at the top of the main loop (`main.cpp`, before `update_FrameInfo`).** It is a test
  that proved a CPU/GPU race: `FrameInfo` and the camera-matrix buffer exist once, and the CPU overwrites them for the
  next frame while the GPU still renders the previous one, so shadows break up when the camera moves fast. The wait hides
  it at the cost of all CPU/GPU overlap. Real fix: give each frame in flight its own copy of that data (rotating buffers,
  or Daxa's per-task `ti.allocator`). Owner designs it; Claude reviews only.
- Bloom review (2026-11-25), raised as questions to the owner: `iter_amount = 5` is odd, so passes are H,V,H,V,H
  (3 horizontal, 2 vertical: blur wider horizontally); `composite.glsl` hardcodes `const float gamma = 2.2` and ignores
  `push.gamma`; `bloom_blur.glsl` has no early return for invocations past the image edge (dispatch rounds up to 8).
  Later: blurring at full resolution gives a small radius; a downsampled mip chain is the usual next step.
  Gamma must stay 2.2 (owner tried 1.2 for looks; tune ambient/light/exposure instead).
- **MAJOR FIX, must be done (owner asked not to forget it, 2026-11-23): stop recomputing matrices per vertex in
  `geometry.glsl`.** Line 37 does `transpose(inverse(view * model))` for every vertex, every frame, with the same result
  for all vertices of an object; lines 33/35/41 also redo `proj * view * model` / `view * model`. Do it before bloom is
  finished or at the latest before parallax mapping. The owner proposed uploading
  `transpose(inverse(view * model))`, `view * model` and `proj * view * model`. Advice given on 2026-11-02, owner's choice
  still to confirm: per-object per-frame uploads aren't needed. Store only the world normal matrix (add
  `daxa_f32mat4x4 normal` after `model` in `Model_Matrix`; mat4 because buffer layout pads mat3 columns; write
  `transpose(inverse(m))` in `GameObject::updateTransform()`). `view` is rigid, so the view-space normal is
  `mat3(view) * (mat3(deref(push.model_M).normal) * vert.normal)`. Position: `viewPos = view * (model * pos)`, then
  `gl_Position = proj * viewPos`. Tangent: `mat3(view) * (mat3(model) * tangent)`. `forward.glsl` can use the same normal
  matrix without `view`.
- Perf: `forward.glsl:70` still copies the whole `FrameInfo` (`FrameInfo fr = deref(push.f_info);`); `lighting.glsl`
  is fixed. Not urgent while `Forward_Graph` is not executed.
- Shadows, still open: bias values were copied from LearnOpenGL (far plane 7.5) but here the ortho depth spans
  `far - near` = 49 world units linearly, so 0.05 ≈ 2.45 units and 0.005 ≈ 0.25 units (risk of peter-panning; owner to
  check where shadows meet the floor). Nothing outside the shadow map treated as lit (`clamp_to_edge` stretches edge
  texels; z > 1 not handled), point-light cubes cast shadows,
  hard edges (PCF), only `dlights[0]` has a shadow map. `update_FrameInfo` takes `Camera` by value (should be `const&`).
- **Check before parallax: bitangent sign.** `geometry.glsl:42` uses `cross(out_tang, out_norm) * w`; glTF defines
  `cross(normal, tangent) * w`, so it looks inverted. Parallax builds on the same TBN.
- Forward still lights the light cubes (only deferred has the emissive path).
- Deferred: the G-buffer view IDs reach `lighting.glsl` through a host-written buffer (`G_Buffer`, `g_Buffer_views_ID`,
  `rewrite_GBuffer_views`, `task_buffer`). Planned: push `ti.get(image).view_ids[0]` x3 directly in `lighting_ptrs` and
  delete the buffer, the lambda, the task buffer and the `G_Buffer` struct (removes a one-frame race on resize).
- Deferred: background pixels come out black (albedo cleared to alpha 0); nothing checks alpha yet.
- `KHR_texture_transform` offset and rotation are ignored (only scale is applied).
- `Model.hpp`, non-indexed branch: `draw_range.count = primative.indices->count` dereferences a null pointer; should be
  `pos->count`. Not hit by the current assets.
- `Model::m_hasTexture` and `Model::m_empty_ImageID` are leftovers and can be removed.
- No mipmaps.
- Later: parallax mapping, after the two items at the top of this list.

Update this section when a step is finished: append a dated entry to Progress (the dates are the owner's record of
progress) and update Open.
