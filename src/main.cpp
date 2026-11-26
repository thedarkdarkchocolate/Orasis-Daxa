#include <daxa/daxa.hpp>
#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include "App.hpp"
#include "Model.hpp"
#include "GameObjet.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "KmbMovementController.hpp"
#include "Daxa_GLM_helper.hpp"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // Vulkan depth range is 0..1, GLM defaults to OpenGL's -1..1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>



#include "shader/shared.inl"

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/Task_Graph.hpp>

#include <chrono>
#include <string>
#include <iostream>
#include <random>


daxa::ImageId createDepthAttachment(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::Format format = daxa::Format::D32_SFLOAT);
daxa::ImageId createAttachment(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::Format format, std::string_view name, daxa::ImageUsageFlags flags = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED);
daxa::ImageId createCustomAttachment(daxa::Device& GPU, daxa::Extent2D extent, daxa::Format format, std::string_view name, daxa::ImageUsageFlags flags);
void recreateTaskImage(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::ImageId& image, daxa::ExternalTaskImage& ext_image);

int main(int argc, char const *argv[])
{

    AppWindow window("Orasis_DAXA", 860*2, 640*2);
    window.set_mouse_capture(true); // Can be change with pressing ~

    daxa::Instance instance = daxa::create_instance({}); // creating instance
    
    daxa::Device GPU = instance.create_device_2(instance.choose_device({daxa::ImplicitFeatureFlagBits::BASIC_RAY_TRACING}, {})); // choosing a device
    
    std::cout << GPU.properties().device_name << "\n"; // device name

    
    // SwapChain creation
    
    daxa::Swapchain swapchain = GPU.create_swapchain({
        .native_window_info = window.get_native_window_info(),
        
        .surface_format = GPU.choose_swapchain_surface_format({
            .native_window_info = window.get_native_window_info(),
        }),
        
        .present_mode = daxa::PresentMode::FIFO_RELAXED,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .name = "mi swapchainarous"
        
    });
    
    // ImGui
    ImGuiContext* ImGui_context = ImGui::CreateContext();
    
    ImGui_ImplGlfw_InitForVulkan(window.glfw_window_ptr, true);
    daxa::ImGuiRenderer ImGui_render (daxa::ImGuiRendererInfo{.device = GPU, .format = swapchain.get_format(), .imgui_context = ImGui_context});
    
    // Depth attachment
    daxa::ImageId depthAttachment = createDepthAttachment(GPU, swapchain);


    // Create G-Buffer
    daxa::ImageId g_Buffer[3];

    // Lamda to auto recreate G-buffer on resize
    auto createGBuffer = [&](daxa::Device& GPU, daxa::Swapchain swapchain){

        g_Buffer[0] = createAttachment(GPU, swapchain, daxa::Format::R32G32B32A32_SFLOAT, "Position");
        g_Buffer[1] = createAttachment(GPU, swapchain, daxa::Format::R16G16B16A16_SFLOAT, "Normals");
        g_Buffer[2] = createAttachment(GPU, swapchain, daxa::Format::R8G8B8A8_SRGB, "Albido");
        
    };
    
    auto destroyGBuffer = [&](daxa::Device& GPU){

        GPU.destroy_image(g_Buffer[0]);
        GPU.destroy_image(g_Buffer[1]);
        GPU.destroy_image(g_Buffer[2]);
        
    };

    createGBuffer(GPU, swapchain);

    auto g_Buffer_views_ID = GPU.create_buffer({
        .size = sizeof(G_Buffer),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
    });

    G_Buffer* g_Buffer_view_ptr = GPU.buffer_host_address_as<G_Buffer>(g_Buffer_views_ID).value();

    auto rewrite_GBuffer_views = [&](){
        g_Buffer_view_ptr[0] = {g_Buffer[0].default_view(), g_Buffer[1].default_view(), g_Buffer[2].default_view()};
    };

    rewrite_GBuffer_views();

    // Pipeline manager

    auto pipeline_manager = daxa::PipelineManager({
        .device = GPU,
        .root_paths = {
            DAXA_SHADER_INCLUDE_DIR,
            SHADER_DIR
        },
        .default_language = daxa::ShaderLanguage::GLSL,
        .default_enable_debug_info = true,
        .name = "mi piplane mangareos"
    });

    // Pipeline creation
    std::shared_ptr<daxa::RasterPipeline> geometry_Pipeline;
    {
        auto result = pipeline_manager.add_raster_pipeline2({
            .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"geometry.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"geometry.glsl"}},
            .color_attachments = {{.format = daxa::Format::R32G32B32A32_SFLOAT},
                                  {.format = daxa::Format::R16G16B16A16_SFLOAT},
                                  {.format = daxa::Format::R8G8B8A8_SRGB},
                                },
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::LESS_OR_EQUAL,
            },
            .raster = {},
            .name = "mi G_pipline"
            
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;
        }
        
        geometry_Pipeline = result.value();
        
    }
    
    std::shared_ptr<daxa::RasterPipeline> shadow_depth_Pipeline;
    {
        auto result = pipeline_manager.add_raster_pipeline2({
            .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"shadow_depth.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"shadow_depth.glsl"}},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::LESS_OR_EQUAL,
            },
            .raster = {},
            .name = "mi_shadow_depth_pipline"
            
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;
        }
        
        shadow_depth_Pipeline = result.value();
        
    }

    std::shared_ptr<daxa::RasterPipeline> lighting_pipeline;
    {
        auto result = pipeline_manager.add_raster_pipeline2({
            .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"lighting.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"lighting.glsl"}},
            .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT},
                                  {.format = daxa::Format::R16G16B16A16_SFLOAT}},
            .raster = {},
            .name = "mi L_pipline"
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;        
        }

        lighting_pipeline = result.value();

    }
    
    std::shared_ptr<daxa::RasterPipeline> composite_pipeline;
    {
        auto result = pipeline_manager.add_raster_pipeline2({
            .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"composite.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"composite.glsl"}},
            .color_attachments = {{.format = swapchain.get_format()}},
            .raster = {},
            .name = "mi Comp_pipline"
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;        
        }

        composite_pipeline = result.value();

    }

    std::shared_ptr<daxa::RasterPipeline> forward_pipeline;
    {
        auto result = pipeline_manager.add_raster_pipeline2({
            .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"forward.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"forward.glsl"}},
            .color_attachments = {{.format = swapchain.get_format()}},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::LESS_OR_EQUAL,
            },
            .raster = {},
            .name = "mi F_pipline"
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;        
        }

        forward_pipeline = result.value();

    }

    std::shared_ptr<daxa::ComputePipeline> compute_pipeline_SSAO;
    {
        auto result = pipeline_manager.add_compute_pipeline2({
            .source = daxa::ShaderFile{"ssao.glsl"},
            .push_constant_size = sizeof(ssao_ptrs),
            .name = "mi Comput PIpline blur",
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;        
        }

        compute_pipeline_SSAO = result.value();

    }
    
    std::shared_ptr<daxa::ComputePipeline> compute_pipeline_SSAO_blur;
    {
        auto result = pipeline_manager.add_compute_pipeline2({
            .source = daxa::ShaderFile{"ssao_blur.glsl"},
            .push_constant_size = sizeof(ssao_blur_ptrs),
            .name = "mi Comput PIpline ssao blur",
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;        
        }

        compute_pipeline_SSAO_blur = result.value();

    }

    std::shared_ptr<daxa::ComputePipeline> compute_pipeline_Bloom;
    {
        auto result = pipeline_manager.add_compute_pipeline2({
            .source = daxa::ShaderFile{"bloom_blur.glsl"},
            .push_constant_size = sizeof(bloom_ptrs),
            .name = "mi Comput PIpline Blom blur",
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;        
        }

        compute_pipeline_Bloom = result.value();

    }

    // std::shared_ptr<daxa::RayTracingPipeline> RT_shadows;
    // {
    //     auto result = pipeline_manager.add_ray_tracing_pipeline2({
    //         .source = daxa::ShaderFile{"shadows.glsl"},
    //         .push_constant_size = sizeof(bloom_ptrs),
    //         .name = "mi RT shadows",
    //     });
        
    //     if (result.is_err())
    //     {
    //         std::cerr << result.message() << std::endl;
    //         return -1;        
    //     }

    //     RT_shadows = result.value();

    // }










    
    // PROJECTION MATRIX BUFFER CREATION AND LAMDA TO UPDATE THE BUFFER

    auto proj_view_Buffer_ID = GPU.create_buffer({
        .size = sizeof(Projection_View_Matrix),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
    });

    Projection_View_Matrix* proj_view_matrix_ptr = GPU.buffer_host_address_as<Projection_View_Matrix>(proj_view_Buffer_ID).value();

    // Function to update the view * matrix each frame
    auto updateProjection_View_Matrix = [](Projection_View_Matrix* proj_view_mat_ptr, Camera& c)
    {
        proj_view_mat_ptr[0] = {.proj = to_daxa(c.getProjection()), .view =to_daxa(c.getViewMatrix())};
    };


    // FRAME INFO BUFFER CREATION AND LAMDA TO UPDATE THE BUFFER

    FrameInfo frame_info {};

    auto frameInfo_buff_id = GPU.create_buffer({
        .size = sizeof(FrameInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
    });
    
    FrameInfo* frameInfo_ptr = GPU.buffer_host_address_as<FrameInfo>(frameInfo_buff_id).value();

    auto update_FrameInfo = [](FrameInfo* frameInfo_ptr, FrameInfo& fr, Scene& scene, const GameObject& cameraObj, const Camera camera)
    {
        fr.cameraPos = to_daxa(cameraObj.m_transform.translation);

        fr.plightCount = scene.m_point_light_Objects_IDs.size();
        fr.dlightCount = scene.m_directional_light_Objects_IDs.size();

        if (fr.plightCount > MAX_POINT_LIGHTS)
        {
            std::cerr << "Point lights count greater than MAX_POINT_LIGHTS" << std::endl;
            fr.plightCount = MAX_POINT_LIGHTS;
        }
        if (fr.dlightCount > MAX_DIR_LIGHTS)
        {
            std::cerr << "Directional lights count greater than MAX_DIR_LIGHTS" << std::endl;
            fr.dlightCount = MAX_DIR_LIGHTS;
        }


        glm::mat4x4 viewToWorld = glm::inverse(camera.getViewMatrix());
        glm::mat4x4 projToVIew = glm::inverse(camera.getProjection());

        fr.viewToWorld = to_daxa(viewToWorld);
        fr.projToView = to_daxa(projToVIew);


        for(uint32_t i = 0; i < fr.plightCount; i++)
        {
            auto& p = scene.get(scene.m_point_light_Objects_IDs[i]);
            fr.plights[i] = { .position = to_daxa(p.m_transform.translation), .color = to_daxa(p.m_pointLight->light_color), .intensity = p.m_pointLight->intensity};
        }
        for(uint32_t i = 0; i < fr.dlightCount; i++)
        {
            auto& d = scene.get(scene.m_directional_light_Objects_IDs[i]);
            fr.dlights[i] = {.dir = to_daxa(d.m_dirLight->light_dir), .color = to_daxa(d.m_dirLight->light_color), .intensity = d.m_dirLight->intensity,
                .lightSpace = to_daxa(d.m_dirLight->lightSpace), .viewToLightSpace = to_daxa(d.m_dirLight->lightSpace * viewToWorld)};
        }

        
        frameInfo_ptr[0] = fr;
    };


    // SSAO kernel creation
    SSAO_Kernel ssao;
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;

    for (unsigned int i = 0; i < MAX_SSAO_SAMPLES ; ++i)
    {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator)
        );
        sample  = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = (float)i / MAX_SSAO_SAMPLES; 
        scale   = 0.1f + (scale * scale) * (1.0f - 0.1f);
        sample *= scale;
        ssao.samples[i] = to_daxa(sample);   
    }

    for (unsigned int i = 0; i < 16; i++)
    {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            0.0f); 
        ssao.noise[i] = to_daxa(noise);
    } 

    daxa::BufferId SSAO_buffer_ID;

    SSAO_buffer_ID = GPU.create_buffer({
        .size = sizeof(SSAO_Kernel),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
    });
    
    SSAO_Kernel* SSAO_Kernel_ptr = GPU.buffer_host_address_as<SSAO_Kernel>(SSAO_buffer_ID).value();

    SSAO_Kernel_ptr[0] = ssao;

    daxa::ImageId ssao_image_ID = createAttachment(GPU, swapchain, daxa::Format::R8_UNORM, "ssao image",
                    daxa::ImageUsageFlagBits::SHADER_STORAGE | daxa::ImageUsageFlagBits::SHADER_SAMPLED);
                    
    daxa::ImageId ssao_blur_image_ID = createAttachment(GPU, swapchain, daxa::Format::R8_UNORM, "ssao blur image",
                    daxa::ImageUsageFlagBits::SHADER_STORAGE | daxa::ImageUsageFlagBits::SHADER_SAMPLED);

    
    // HDR & BLOOM color buffer
    daxa::ImageId hdr_image_ID = createAttachment(GPU, swapchain, daxa::Format::R16G16B16A16_SFLOAT, "HDR image");
    daxa::ImageId bloom_image_ID = createAttachment(GPU, swapchain, daxa::Format::R16G16B16A16_SFLOAT, "Bloom image",
        daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE);
    daxa::ImageId bloom_pong_image_ID = createAttachment(GPU, swapchain, daxa::Format::R16G16B16A16_SFLOAT, "Bloom pong image",
        daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE);

    // Depth Shadow Map
    daxa::ImageId shadow_depth_map_ID = createCustomAttachment(GPU, {1024, 1024}, daxa::Format::D32_SFLOAT, "Shadow_Depth_map",
        daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED);


        
    // Scene decleration

    Scene scene{GPU};
    
    uint32_t sponza = scene.add(ASSET_DIR "/glb/Sponza.glb");

    uint32_t sol = scene.add_directional_Light(2.f, glm::vec3(1.f, 0.95f, 0.85f), glm::vec3(-0.3f, -1.f, -0.4f));

    uint32_t lught  = scene.add_point_Light(ASSET_DIR "/glb/Box.glb", 1.f, glm::vec3(1.f), {.translation = {0.f, 3.f, 0.f}, .scale = glm::vec3(0.2f)});

    scene.add_point_Light(ASSET_DIR "/glb/Box.glb", 3.f, glm::vec3(1.f, 0.1f, 0.1f), {.translation = {-10.f, 1.5f,  2.f}, .scale = glm::vec3(0.2f)}); // red
    scene.add_point_Light(ASSET_DIR "/glb/Box.glb", 3.f, glm::vec3(0.1f, 1.f, 0.1f), {.translation = { -5.f, 1.5f, -2.f}, .scale = glm::vec3(0.2f)}); // green
    scene.add_point_Light(ASSET_DIR "/glb/Box.glb", 3.f, glm::vec3(0.1f, 0.2f, 1.f), {.translation = {  5.f, 1.5f,  2.f}, .scale = glm::vec3(0.2f)}); // blue
    scene.add_point_Light(ASSET_DIR "/glb/Box.glb", 3.f, glm::vec3(1.f, 0.6f, 0.1f), {.translation = { 10.f, 1.5f, -2.f}, .scale = glm::vec3(0.2f)}); // orange
    scene.add_point_Light(ASSET_DIR "/glb/Box.glb", 3.f, glm::vec3(0.8f, 0.1f, 1.f), {.translation = {  0.f, 6.f,   0.f}, .scale = glm::vec3(0.2f)}); // purple, higher up

    daxa::ImageId skybox_image_ID
    = scene.add_skybox({
        ASSET_DIR "/skybox/LearnOpenGL/right.jpg",
        ASSET_DIR "/skybox/LearnOpenGL/left.jpg",
        ASSET_DIR "/skybox/LearnOpenGL/top.jpg",
        ASSET_DIR "/skybox/LearnOpenGL/bottom.jpg",
        ASSET_DIR "/skybox/LearnOpenGL/front.jpg",
        ASSET_DIR "/skybox/LearnOpenGL/back.jpg"
    });


    // Sampler
    daxa::types::SamplerId repeat_sampler, clamp_to_edge_sampler;
    
    repeat_sampler = GPU.create_sampler({.address_mode_u = daxa::SamplerAddressMode::REPEAT, .address_mode_v = daxa::SamplerAddressMode::REPEAT});
    clamp_to_edge_sampler = GPU.create_sampler({.address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE, .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE});

    Camera camera;
    GameObject cameraObj = GameObject::createGameObject();
    cameraObj.setPosition({-10, 2, 0});
    cameraObj.m_transform.rotation.y = glm::radians(90.f);

    KmbMovementController cameraController(window);
    cameraController.cursorPrev_X = window.m_width/2;
    cameraController.cursorPrev_Y = window.m_height/2;






    // Task graph: recorded once here, executed every frame in the main loop.

    // The swapchain image changes every frame, so the graph refers to it through this handle.
    daxa::ExternalTaskImage task_swapchain_image{{.is_swapchain_image = true, .name = "swapchain image"}};

    daxa::ExternalTaskImage task_depth_image{{.is_swapchain_image = false, .name = "depth image"}};
    task_depth_image.set_image(depthAttachment);

    
    daxa::TaskGraph Deferred_Graph = daxa::TaskGraph({
        .device = GPU,
        .swapchain = swapchain,
        .name = "Deferred Graph",
    });

    daxa::TaskGraph Forward_Graph = daxa::TaskGraph({
        .device = GPU,
        .swapchain = swapchain,
        .name = "Forward Graph",
    });

    Forward_Graph.register_image(task_swapchain_image);
    Forward_Graph.register_image(task_depth_image);


    // Declaring how the task uses the image lets the graph generate the image barriers.
    Forward_Graph.add_task(daxa::InlineTask::Raster("draw triangle")
        .color_attachment.writes(task_swapchain_image)
        .depth_stencil_attachment.reads_writes(task_depth_image)
        .executes([&](daxa::TaskInterface ti)
        {
            daxa::ImageInfo swapchain_image_info = ti.info(task_swapchain_image.view()).value();
            // daxa::ImageInfo depth_image_info = ti.info(task_depth_image.view()).value();

            daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                .color_attachments = std::array{
                        daxa::RenderAttachmentInfo{
                            .image_view = ti.get(task_swapchain_image).view_ids[0],
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<daxa::f32, 4>{0.f, 0.f, 0.f, 0.f},
                        }
                    },
                .depth_attachment = daxa::RenderAttachmentInfo{
                    .image_view = ti.get(task_depth_image).view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = daxa::DepthValue(1.f),
                },
                .render_area = {.width = swapchain_image_info.size.x, .height = swapchain_image_info.size.y},

            });

            render_recorder.set_pipeline(*forward_pipeline);
            
            
            
            for(auto& [id, obj]: scene.m_objects)
            {
                
                if(obj.m_model == nullptr) continue;


                render_recorder.set_index_buffer({
                    .buffer = obj.m_model->m_index_buffer_ID,
                    .offset = 0,
                    .index_type = daxa::IndexType::uint32
                });
                
                for (auto draw: obj.m_model->m_draw_ranges)
                {
                    render_recorder.push_constant(buffer_ptrs{
                        .vertices = GPU.device_address(obj.m_model->m_vertex_buffer_ID).value(),
                        .proj_view_matrices = GPU.device_address(proj_view_Buffer_ID).value(),
                        .model_M = GPU.device_address(obj.m_world_space_buff_ID).value(),
                        .f_info = GPU.device_address(frameInfo_buff_id).value(),
                        .image_view_ID = draw.slot == -1 ? obj.m_model->m_empty_ImageID.default_view() : obj.m_model->m_images_IDs[draw.slot].default_view(),
                        .sampler_ID = repeat_sampler,
                        .hasTexture = draw.slot == -1 ? false : true,
                        .uv_scale = to_daxa(draw.uv_scale),
                        // Change the 0.f vec to draw.mat_color to use the color of the material
                        .mat_color = obj.m_pointLight == nullptr ? to_daxa(draw.mat_color) : to_daxa(glm::vec4(obj.m_pointLight->light_color, 1)),
                    });
                    
                    
                    render_recorder.draw_indexed({.index_count = draw.count, .first_index = draw.start});
                }    
            }


            ti.recorder = std::move(render_recorder).end_renderpass();
        })
    );

    Forward_Graph.submit({});
    Forward_Graph.present({});
    Forward_Graph.complete({});

    
    daxa::ExternalTaskImage position_image{{.is_swapchain_image = false, .name = "position"}};
    position_image.set_image(g_Buffer[0]);
    daxa::ExternalTaskImage normals_image{{.is_swapchain_image = false, .name = "normals"}};
    normals_image.set_image(g_Buffer[1]);
    daxa::ExternalTaskImage albido_image{{.is_swapchain_image = false, .name = "albido"}};
    albido_image.set_image(g_Buffer[2]);

    Deferred_Graph.register_image(task_swapchain_image);
    Deferred_Graph.register_image(task_depth_image);
    Deferred_Graph.register_image(position_image);
    Deferred_Graph.register_image(normals_image);
    Deferred_Graph.register_image(albido_image);


    Deferred_Graph.add_task(daxa::InlineTask::Raster("deferred Geometry")
        .depth_stencil_attachment.reads_writes(task_depth_image)
        .color_attachment.writes(position_image)
        .color_attachment.writes(normals_image)
        .color_attachment.writes(albido_image)
        .executes([&](daxa::TaskInterface ti)
        {
            daxa::ImageInfo g_buffer_info = ti.info(position_image.view()).value();

            daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                .color_attachments = std::array{
                        daxa::RenderAttachmentInfo{
                            .image_view = ti.get(position_image).view_ids[0],
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<daxa::f32, 4>{0.f, 0.f, 0.f, 0.f},
                        },
                        daxa::RenderAttachmentInfo{
                            .image_view = ti.get(normals_image).view_ids[0],
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<daxa::f32, 4>{0.f, 0.f, 0.f, 0.f},
                        },
                        daxa::RenderAttachmentInfo{
                            .image_view = ti.get(albido_image).view_ids[0],
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<daxa::f32, 4>{0.f, 0.f, 0.f, 0.f},
                        },
                    },
                .depth_attachment = daxa::RenderAttachmentInfo{
                    .image_view = ti.get(task_depth_image).view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = daxa::DepthValue(1.f),
                },
                .render_area = {.width = g_buffer_info.size.x, .height = g_buffer_info.size.y},

            });

            render_recorder.set_pipeline(*geometry_Pipeline);
            
            
            for(auto& [id, obj]: scene.m_objects)
            {
                
                if(obj.m_model == nullptr) continue;


                render_recorder.set_index_buffer({
                    .buffer = obj.m_model->m_index_buffer_ID,
                    .offset = 0,
                    .index_type = daxa::IndexType::uint32
                });
                
                for (auto draw: obj.m_model->m_draw_ranges)
                {
                    render_recorder.push_constant(buffer_ptrs{
                        .vertices = GPU.device_address(obj.m_model->m_vertex_buffer_ID).value(),
                        .proj_view_matrices = GPU.device_address(proj_view_Buffer_ID).value(),
                        .model_M = GPU.device_address(obj.m_world_space_buff_ID).value(),
                        .f_info = GPU.device_address(frameInfo_buff_id).value(),
                        .image_view_ID = draw.slot == -1 ? obj.m_model->m_empty_ImageID.default_view() : obj.m_model->m_images_IDs[draw.slot].default_view(),
                        .normal_map_view_ID = draw.normal_slot == -1 ? obj.m_model->m_empty_ImageID.default_view() : obj.m_model->m_images_IDs[draw.normal_slot].default_view(),
                        .sampler_ID = repeat_sampler,
                        .hasTexture = draw.slot == -1 ? false : true,
                        .hasNormalMap = draw.normal_slot == -1 ? false : true,
                        .uv_scale = to_daxa(draw.uv_scale),
                        // Change the 0.f vec to draw.mat_color to use the color of the material
                        .mat_color = obj.m_pointLight == nullptr ? to_daxa(draw.mat_color) : to_daxa(glm::vec4(obj.m_pointLight->light_color, 1)),
                        .emmisive = obj.m_pointLight != nullptr ? obj.m_pointLight->intensity : 0,
                    });
                    
                    
                    render_recorder.draw_indexed({.index_count = draw.count, .first_index = draw.start});
                }    
            }


            ti.recorder = std::move(render_recorder).end_renderpass();
        })
    );

    daxa::ExternalTaskImage shadow_depth_image({.is_swapchain_image = false, .name = "shadow_depth_output"});
    shadow_depth_image.set_image(shadow_depth_map_ID);

    Deferred_Graph.register_image(shadow_depth_image);

    Deferred_Graph.add_task(daxa::InlineTask::Raster("Shadow Depth")
        .depth_stencil_attachment.reads_writes(shadow_depth_image)
        .executes([&](daxa::TaskInterface ti)
        {

            daxa::ImageInfo shadow_depth_info = ti.info(shadow_depth_image.view()).value();

            daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                .depth_attachment = daxa::RenderAttachmentInfo{
                    .image_view = ti.get(shadow_depth_image).view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = daxa::DepthValue(1.f),
                },
                .render_area = {.width = shadow_depth_info.size.x, .height = shadow_depth_info.size.y},

            });

            render_recorder.set_pipeline(*shadow_depth_Pipeline);

            for(auto& [id, obj]: scene.m_objects)
            {
                
                if(obj.m_model == nullptr) continue;


                render_recorder.set_index_buffer({
                    .buffer = obj.m_model->m_index_buffer_ID,
                    .offset = 0,
                    .index_type = daxa::IndexType::uint32
                });
                
                for (auto draw: obj.m_model->m_draw_ranges)
                {
                    render_recorder.push_constant(shadow_ptrs{
                        .vertices = GPU.device_address(obj.m_model->m_vertex_buffer_ID).value(),
                        .model_M = GPU.device_address(obj.m_world_space_buff_ID).value(),
                        .f_info = GPU.device_address(frameInfo_buff_id).value(),
                    });
                    
                    
                    render_recorder.draw_indexed({.index_count = draw.count, .first_index = draw.start});
                }    
            }


            ti.recorder = std::move(render_recorder).end_renderpass();
            
        })
    );







    daxa::ExternalTaskImage ssao_image({.is_swapchain_image = false, .name = "ssao_output"});
    ssao_image.set_image(ssao_image_ID);

    Deferred_Graph.register_image(ssao_image);

    Deferred_Graph.add_task(daxa::InlineTask::Compute("SSAO compute")
        .compute_shader.samples(position_image.view())
        .compute_shader.samples(normals_image.view())
        .compute_shader.writes(ssao_image.view())
        .executes([&](daxa::TaskInterface ti){
            
            daxa::ImageInfo g_buffer_info = ti.info(position_image.view()).value();

            ti.recorder.set_pipeline(*compute_pipeline_SSAO);

            ti.recorder.push_constant(ssao_ptrs{
                .gBuffer = GPU.device_address(g_Buffer_views_ID).value(),
                .proj_view_matrices = GPU.device_address(proj_view_Buffer_ID).value(),
                .SSAO = GPU.device_address(SSAO_buffer_ID).value(),
                .sampler_ID = clamp_to_edge_sampler,
                .output_view_ID = ti.get(ssao_image).view_ids[0],
            });

            ti.recorder.dispatch({.x = (g_buffer_info.size.x + 7) / 8, .y = (g_buffer_info.size.y + 7) / 8, .z = 1});
        })
    );
        

    daxa::ExternalTaskImage ssao_blur_image({.is_swapchain_image = false, .name = "ssao_blur_output"});
    ssao_blur_image.set_image(ssao_blur_image_ID);

    Deferred_Graph.register_image(ssao_blur_image);
    
    Deferred_Graph.add_task(daxa::InlineTask::Compute("SSAO blur compute")
        .compute_shader.samples(ssao_image.view())
        .compute_shader.writes(ssao_blur_image.view())
        .executes([&](daxa::TaskInterface ti){
            
            daxa::ImageInfo ssao_image_info = ti.info(ssao_image.view()).value();

            ti.recorder.set_pipeline(*compute_pipeline_SSAO_blur);

            ti.recorder.push_constant(ssao_blur_ptrs{
                .input_view_ID = ti.get(ssao_image).view_ids[0],
                .output_view_ID = ti.get(ssao_blur_image).view_ids[0],
            });

            ti.recorder.dispatch({.x = (ssao_image_info.size.x + 7) / 8, .y = (ssao_image_info.size.y + 7) / 8, .z = 1});
        })
    );

    
    daxa::ExternalTaskBuffer task_buffer({.buffer = g_Buffer_views_ID, .name = "G-buffer image views"});
    Deferred_Graph.register_buffer(task_buffer);
    
    daxa::ExternalTaskImage hdr_image ({.is_swapchain_image = false, .name = "hdr_output"});
    hdr_image.set_image(hdr_image_ID);
    Deferred_Graph.register_image(hdr_image);
    
    daxa::ExternalTaskImage bloom_image ({.is_swapchain_image = false, .name = "bloom"});
    bloom_image.set_image(bloom_image_ID);
    Deferred_Graph.register_image(bloom_image);
    
    Deferred_Graph.add_task(daxa::InlineTask::Raster("deferred Lighting")
        .color_attachment.writes(hdr_image)
        .color_attachment.writes(bloom_image)
        .fragment_shader.reads(task_buffer)
        .fragment_shader.samples(position_image.view())
        .fragment_shader.samples(normals_image.view())
        .fragment_shader.samples(albido_image.view())
        .fragment_shader.samples(ssao_blur_image.view())
        .fragment_shader.samples(shadow_depth_image.view())
        .executes([&](daxa::TaskInterface ti)
        {
            daxa::ImageInfo hdr_image_info = ti.info(hdr_image.view()).value();
            
            daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                .color_attachments = std::array{
                    daxa::RenderAttachmentInfo{
                        .image_view = ti.get(hdr_image).view_ids[0],
                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                        .clear_value = std::array<f32, 4>{0.f, 0.f, 0.f, 0.f}
                    },
                    daxa::RenderAttachmentInfo{
                        .image_view = ti.get(bloom_image).view_ids[0],
                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                        .clear_value = std::array<f32, 4>{0.f, 0.f, 0.f, 0.f}
                    }},
                    .render_area = {.width = hdr_image_info.size.x, .height = hdr_image_info.size.y}
                });
                
                render_recorder.set_pipeline(*lighting_pipeline);
                
                render_recorder.push_constant(lighting_ptrs{
                    .gBuffer = GPU.device_address(g_Buffer_views_ID).value(),
                    .f_info = GPU.device_address(frameInfo_buff_id).value(),
                    .proj_view_matrices = GPU.device_address(proj_view_Buffer_ID).value(),
                    .sampler_ID = repeat_sampler,
                    .sampler_clamp_to_edge_ID = clamp_to_edge_sampler,
                    .ssao_view_ID = ti.get(ssao_blur_image).view_ids[0],
                    .shadow_map_view_ID = ti.get(shadow_depth_image).view_ids[0],
                    .skybox_view_ID = scene.m_skybox_image_view,
                });
                
                render_recorder.draw({.vertex_count = 3});
                
                ti.recorder = std::move(render_recorder).end_renderpass();
                
            })
        );


    daxa::ExternalTaskImage bloom_pong_image({.is_swapchain_image = false, .name = "bloom_bong"});
    bloom_pong_image.set_image(bloom_pong_image_ID);
    Deferred_Graph.register_image(bloom_pong_image);

    int iter_amount = 5;
    bool horizontal = true;
    daxa::TaskImageView bloom_output = bloom_pong_image;
    
    for(int i = 0; i < iter_amount; i++)
    {

        auto src_view = horizontal ? bloom_image.view() : bloom_pong_image.view();
        auto dst_view = horizontal ? bloom_pong_image.view() : bloom_image.view();
        

        Deferred_Graph.add_task(daxa::InlineTask::Compute("Bloom blur compute")
            .compute_shader.samples(src_view)
            .compute_shader.writes(dst_view)
            .executes([&, src_view, dst_view, horizontal](daxa::TaskInterface ti){
                    
                    daxa::ImageInfo bloom_image_info = ti.info(bloom_image.view()).value();
        
                    ti.recorder.set_pipeline(*compute_pipeline_Bloom);
        
                    ti.recorder.push_constant(bloom_ptrs{
                        .input_view_ID = ti.get(src_view).view_ids[0],
                        .output_view_ID = ti.get(dst_view).view_ids[0],
                        .horizontal = horizontal,
                    });
        
                    ti.recorder.dispatch({.x = (bloom_image_info.size.x + 7) / 8, .y = (bloom_image_info.size.y + 7) / 8, .z = 1});
                })
            );   

        horizontal = !horizontal; 
        bloom_output = dst_view;
    }

    float exposure = 1.f;

    Deferred_Graph.add_task(daxa::InlineTask::Raster("composite lighting")
        .color_attachment.writes(task_swapchain_image)
        .fragment_shader.samples(hdr_image.view())
        .fragment_shader.samples(bloom_output)
        .executes([&, bloom_output](daxa::TaskInterface ti)
        {
            daxa::ImageInfo swapchain_info = ti.info(task_swapchain_image.view()).value();
            
            daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                .color_attachments = std::array{daxa::RenderAttachmentInfo{
                    .image_view = ti.get(task_swapchain_image).view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.f, 0.f, 0.f, 0.f}
                }},
                .render_area = {.width = swapchain_info.size.x, .height = swapchain_info.size.y}
            });


            render_recorder.set_pipeline(*composite_pipeline);

            render_recorder.push_constant(composite_ptrs{
                .gamma = 2.2,
                .exposure = exposure,
                .sampler_ID = clamp_to_edge_sampler,
                .hdr_view_ID = ti.get(hdr_image).view_ids[0],
                .bloom_view_ID = ti.get(bloom_output).view_ids[0],
            });

            render_recorder.draw({.vertex_count = 3});

            ti.recorder = std::move(render_recorder).end_renderpass();

        })
    );



    Deferred_Graph.add_task(daxa::InlineTask::Raster("ImGui interface")
        .color_attachment.writes(task_swapchain_image)
        .executes([&](daxa::TaskInterface ti){

            daxa::ImageInfo swapchain_info = ti.info(task_swapchain_image.view()).value();

            ImGui_render.record_commands({ .draw_data = ImGui::GetDrawData(), .recorder = ti.recorder, .target_image = task_swapchain_image.id(),
                 .size_x = swapchain_info.size.x , .size_y = swapchain_info.size.y});

        })
    );

    Deferred_Graph.submit({});
    Deferred_Graph.present({});
    Deferred_Graph.complete({});

    auto currTime = std::chrono::high_resolution_clock::now();

    // FPS counter: count frames, print the average once per second
    float fps_timer = 0.f;
    int fps_frames = 0;

    while(!window.should_close())
    {
        // GLFW poll events
        window.update();

        // A minimized window has a zero-sized surface, so there is nothing to render into.
        if (window.minimized)
        {
            continue;
        }

        if (window.swapchain_out_of_date)
        {
            swapchain.resize();

            recreateTaskImage(GPU, swapchain, depthAttachment, task_depth_image);

            destroyGBuffer(GPU);
            createGBuffer(GPU, swapchain);
            position_image.set_image(g_Buffer[0]);
            normals_image.set_image(g_Buffer[1]);
            albido_image.set_image(g_Buffer[2]);

            rewrite_GBuffer_views();

            recreateTaskImage(GPU, swapchain, hdr_image_ID, hdr_image);
            recreateTaskImage(GPU, swapchain, bloom_image_ID, bloom_image);
            recreateTaskImage(GPU, swapchain, bloom_pong_image_ID, bloom_pong_image);
            
            recreateTaskImage(GPU, swapchain, ssao_image_ID, ssao_image);
            recreateTaskImage(GPU, swapchain, ssao_blur_image_ID, ssao_blur_image);

            window.swapchain_out_of_date = false;
        }

        // ImGui
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("UI");
        ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.f);
        ImGui::End();
        ImGui::Render();

        // Time step
        auto newTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currTime).count();
        currTime = newTime;

        fps_timer += dt;
        fps_frames++;
        if (fps_timer >= 1.f)
        {
            std::cout << "FPS: " << fps_frames / fps_timer << "  (" << 1000.f * fps_timer / fps_frames << " ms)" << std::endl;
            fps_timer = 0.f;
            fps_frames = 0;
        }

        cameraController.moveInPlaneXZ(window.glfw_window_ptr, cameraObj, dt);
        camera.setViewMatrixYXZ(cameraObj.m_transform.translation, cameraObj.m_transform.rotation);
        camera.setCameraPos(cameraObj.m_transform.translation);
        camera.setPrespectiveProjection(glm::radians(60.f), window.getAspect(), 0.1f, 100.f);

        updateProjection_View_Matrix(proj_view_matrix_ptr, camera);

        
        // Frame Info Update
        update_FrameInfo(frameInfo_ptr, frame_info, scene, cameraObj, camera);
        
       


        daxa::ImageId swapchain_image = swapchain.acquire_next_image();
        task_swapchain_image.set_image(swapchain_image);
        if (swapchain_image.is_empty()) continue; // A swapchain image will be empty when there is no available image from the swapchain

        // Records, submits and presents everything declared in the task graph.
        Deferred_Graph.execute({});
        // Forward_Graph.execute({});
        GPU.wait_idle();

        // The device(GPU) performs all memory reclaiming in the collect_garbage call.
        // It's best to call it once at the end of each frame.
        GPU.collect_garbage();
    }

    ImGui_render = {};
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(ImGui_context);

    GPU.destroy_buffer(proj_view_Buffer_ID);
    GPU.destroy_buffer(frameInfo_buff_id);
    GPU.destroy_buffer(g_Buffer_views_ID);
    GPU.destroy_buffer(SSAO_buffer_ID);
    scene.clear();
    GPU.destroy_sampler(repeat_sampler);
    GPU.destroy_sampler(clamp_to_edge_sampler);

    destroyGBuffer(GPU);
    GPU.destroy_image(depthAttachment);
    GPU.destroy_image(ssao_image_ID);
    GPU.destroy_image(ssao_blur_image_ID);
    GPU.destroy_image(hdr_image_ID);
    GPU.destroy_image(bloom_image_ID);
    GPU.destroy_image(bloom_pong_image_ID);
    GPU.destroy_image(shadow_depth_map_ID);



    GPU.wait_idle();
    GPU.collect_garbage();

    return 0;
}


daxa::ImageId createDepthAttachment(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::Format format)
{
    daxa::Extent2D extent = swapchain.get_surface_extent();
    return GPU.create_image({
        .format = format,
        .size = {extent.x, extent.y, 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
    });
}

daxa::ImageId createAttachment(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::Format format, std::string_view name, daxa::ImageUsageFlags flags)
{
    daxa::Extent2D extent = swapchain.get_surface_extent();
    
    return GPU.create_image({
        .format = format,
        .size = {extent.x, extent.y, 1},
        .usage = flags,
        .name = name,
    });
}

daxa::ImageId createCustomAttachment(daxa::Device& GPU, daxa::Extent2D extent, daxa::Format format, std::string_view name, daxa::ImageUsageFlags flags)
{
    return GPU.create_image({
        .format = format,
        .size = {extent.x, extent.y, 1},
        .usage = flags,
        .name = name,
    });
}

void recreateTaskImage(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::ImageId& image, daxa::ExternalTaskImage& ext_image)
{
    daxa::Optional<daxa::ImageInfo> imageInfo = GPU.image_info(image);
    daxa::Extent2D extent = swapchain.get_surface_extent();

    GPU.destroy_image(image);

    image = GPU.create_image({
        .flags = imageInfo.value().flags,
        .dimensions = imageInfo.value().dimensions,
        .format = imageInfo.value().format,
        .size = {extent.x, extent.y, 1},
        .mip_level_count = imageInfo.value().mip_level_count,
        .array_layer_count = imageInfo.value().array_layer_count,
        .sample_count = imageInfo.value().sample_count,
        .usage = imageInfo.value().usage,
        .memory_flags = imageInfo.value().memory_flags,
        .name = imageInfo.value().name,
    });

    ext_image.set_image(image);
}
