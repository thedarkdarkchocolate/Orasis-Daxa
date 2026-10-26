#include <daxa/daxa.hpp>
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


daxa::ImageId createDepthAttachment(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::Format format = daxa::Format::D32_SFLOAT);
daxa::ImageId createAttachment(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::Format format, std::string_view name);


int main(int argc, char const *argv[])
{

    AppWindow window("Orasis_DAXA", 860, 640);
    window.set_mouse_capture(true); // Can be change with pressing TAB

    daxa::Instance instance = daxa::create_instance({}); // creating instance

    daxa::Device GPU = instance.create_device_2(instance.choose_device({}, {})); // choosing a device
    
    std::cout << GPU.properties().device_name << "\n"; // device name


    // SwapChain creation

    daxa::Swapchain swapchain = GPU.create_swapchain({
        .native_window_info = window.get_native_window_info(),

        .surface_format = GPU.choose_swapchain_surface_format({
            .native_window_info = window.get_native_window_info(),
        }),

        .present_mode = daxa::PresentMode::FIFO,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .name = "mi swapchainarous"

    });

    
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
    
    std::shared_ptr<daxa::RasterPipeline> lighting_pipeline;
    {
        auto result = pipeline_manager.add_raster_pipeline2({
            .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"lighting.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"lighting.glsl"}},
            .color_attachments = {{.format = swapchain.get_format()}},
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
            .name = "mi L_pipline"
        });
        
        if (result.is_err())
        {
            std::cerr << result.message() << std::endl;
            return -1;        
        }

        forward_pipeline = result.value();

    }











    
    // PROJECTION MATRIX BUFFER CREATION AND LAMDA TO UPDATE THE BUFFER

    auto proj_view_Buffer_ID = GPU.create_buffer({
        .size = sizeof(Projection_View_Matrix),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
    });

    Projection_View_Matrix* proj_view_matrix_ptr = GPU.buffer_host_address_as<Projection_View_Matrix>(proj_view_Buffer_ID).value();

    // Function to update the view * matrix each frame
    auto updateProjection_View_Matrix = [](Projection_View_Matrix* proj_view_mat_ptr, Camera& c)
    {
        proj_view_mat_ptr[0] = {.proj_view = to_daxa(c.getProjection() * c.getViewMatrix())};
    };


    // FRAME INFO BUFFER CREATION AND LAMDA TO UPDATE THE BUFFER

    FrameInfo frame_info {};

    auto frameInfo_buff_id = GPU.create_buffer({
        .size = sizeof(FrameInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
    });
    
    FrameInfo* frameInfo_ptr = GPU.buffer_host_address_as<FrameInfo>(frameInfo_buff_id).value();

    auto update_FrameInfo = [](FrameInfo* frameInfo_ptr, FrameInfo& fr, Scene& scene, const GameObject& cameraObj)
    {
        fr.cameraPos = to_daxa(cameraObj.m_transform.translation);
        fr.lightCount = scene.m_lightObjects_IDs.size();
        for(int i = 0; i < fr.lightCount; i++)
        {
            auto& p = scene.get(scene.m_lightObjects_IDs[i]);
            fr.lights[i] = { .position = to_daxa(p.m_transform.translation), .color = to_daxa(p.m_pointLight->light_color), .intensity = p.m_pointLight->intensity};
        }
        frameInfo_ptr[0] = fr;
    };

    // Scene decleration

    Scene scene{GPU};
    uint32_t cube  = scene.add(ASSET_DIR "/glb/Box.glb", {.translation = {0.f, 0.f, 1.f}});
    uint32_t chair  = scene.add(ASSET_DIR "/glb/SheenChair.glb", {.translation = {10.f, 0.f, 1.f}});
    uint32_t floor = scene.add(ASSET_DIR "/glb/Box.glb", {.translation = {0.f, 0.f, 5.f}, .scale = {5.f, 0.1f, 5.f}});
    uint32_t lught  = scene.addLight(ASSET_DIR "/glb/Box.glb", 10.f, glm::vec3(1.f), {.translation = {-5.f, 10.f, 14.f}, .scale = glm::vec3(0.2f)});
    uint32_t redLught  = scene.addLight(ASSET_DIR "/glb/Box.glb", 10.f, glm::vec3(1.f, 0.f, 0.f), {.translation = {5.f, 10.f, 14.f}, .scale = glm::vec3(0.2f)});




    // Sampler
    daxa::types::SamplerId sampler;
    sampler = GPU.create_sampler({.address_mode_u = daxa::SamplerAddressMode::REPEAT, .address_mode_v = daxa::SamplerAddressMode::REPEAT});

    scene.get(cube).setPosition({1.f, 0.f, 1.f});
    
    Camera camera;
    GameObject cameraObj = GameObject::createGameObject();
    cameraObj.setPosition({0.f, 0.f, -1.f});

    KmbMovementController cameraController(window);
    cameraController.cursorPrev_X = window.m_width/2;
    cameraController.cursorPrev_Y = window.m_height/2;






    // Task graph: recorded once here, executed every frame in the main loop.

    // The swapchain image changes every frame, so the graph refers to it through this handle.
    daxa::ExternalTaskImage task_swapchain_image{{.is_swapchain_image = true, .name = "swapchain image"}};

    daxa::ExternalTaskImage task_depth_image{{.is_swapchain_image = false, .name = "depth image"}};
    task_depth_image.set_image(depthAttachment);
    
    daxa::ExternalTaskImage position_image{{.is_swapchain_image = false, .name = "position"}};
    position_image.set_image(g_Buffer[0]);
    daxa::ExternalTaskImage normals_image{{.is_swapchain_image = false, .name = "normals"}};
    normals_image.set_image(g_Buffer[1]);
    daxa::ExternalTaskImage albido_image{{.is_swapchain_image = false, .name = "albido"}};
    albido_image.set_image(g_Buffer[2]);

    
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
                        .sampler_ID = sampler,
                        .hasTexture = draw.slot == -1 ? false : true,
                        .uv_scale = to_daxa(draw.uv_scale),
                        // Change the 0.f vec to draw.mat_color to use the color of the material
                        .mat_color = obj.m_pointLight == nullptr ? to_daxa(draw.mat_color) : to_daxa(glm::vec4(obj.m_pointLight->light_color, 1)),
                    });
                    
                    
                    render_recorder.draw_indexed({.index_count = draw.count, .first_index = draw.start});
                }    
            }


            ti.recorder = std::move(render_recorder).end_renderpass();
        }));

    Forward_Graph.submit({});
    Forward_Graph.present({});
    Forward_Graph.complete({});



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
                        .sampler_ID = sampler,
                        .hasTexture = draw.slot == -1 ? false : true,
                        .uv_scale = to_daxa(draw.uv_scale),
                        // Change the 0.f vec to draw.mat_color to use the color of the material
                        .mat_color = obj.m_pointLight == nullptr ? to_daxa(draw.mat_color) : to_daxa(glm::vec4(obj.m_pointLight->light_color, 1)),
                    });
                    
                    
                    render_recorder.draw_indexed({.index_count = draw.count, .first_index = draw.start});
                }    
            }


            ti.recorder = std::move(render_recorder).end_renderpass();
        }));

    

    daxa::ExternalTaskBuffer task_buffer({.buffer = g_Buffer_views_ID, .name = "G-buffer image views"});
    Deferred_Graph.register_buffer(task_buffer);

    Deferred_Graph.add_task(daxa::InlineTask::Raster("deferred Lighting")
    .color_attachment.writes(task_swapchain_image)
    .fragment_shader.reads(task_buffer)
    .fragment_shader.samples(position_image.view())
    .fragment_shader.samples(normals_image.view())
    .fragment_shader.samples(albido_image.view())
    .executes([&](daxa::TaskInterface ti)
    {
        daxa::ImageInfo swapchain_image_info = ti.info(task_swapchain_image.view()).value();

        daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{daxa::RenderAttachmentInfo{
                .image_view = ti.get(task_swapchain_image).view_ids[0],
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.f, 0.f, 0.f, 0.f}
            }},
            .render_area = {.width = swapchain_image_info.size.x, .height = swapchain_image_info.size.y}
        });


        render_recorder.set_pipeline(*lighting_pipeline);

        render_recorder.push_constant(lighting_ptrs{
            .gBuffer = GPU.device_address(g_Buffer_views_ID).value(),
            .f_info = GPU.device_address(frameInfo_buff_id).value(),
            .sampler_ID = sampler,
        });

        render_recorder.draw({.vertex_count = 3});

        ti.recorder = std::move(render_recorder).end_renderpass();

    })

    );

    Deferred_Graph.submit({});
    Deferred_Graph.present({});
    Deferred_Graph.complete({});

    auto currTime = std::chrono::high_resolution_clock::time_point();

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
            GPU.destroy_image(depthAttachment);
            depthAttachment = createDepthAttachment(GPU, swapchain);
            task_depth_image.set_image(depthAttachment);

            destroyGBuffer(GPU);
            createGBuffer(GPU, swapchain);
            position_image.set_image(g_Buffer[0]);
            normals_image.set_image(g_Buffer[1]);
            albido_image.set_image(g_Buffer[2]);

            rewrite_GBuffer_views();

            window.swapchain_out_of_date = false;
        }

        // Time step
        auto newTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currTime).count();
        currTime = newTime;

        cameraController.moveInPlaneXZ(window.glfw_window_ptr, cameraObj, dt);
        camera.setViewYXZ(cameraObj.m_transform.translation, cameraObj.m_transform.rotation);
        camera.setCameraPos(cameraObj.m_transform.translation);
        camera.setPrespectiveProjection(glm::radians(60.f), window.getAspect(), 0.1f, 100.f);

        updateProjection_View_Matrix(proj_view_matrix_ptr, camera);

        // Frame Info Update

        update_FrameInfo(frameInfo_ptr, frame_info, scene, cameraObj);
        


        daxa::ImageId swapchain_image = swapchain.acquire_next_image();
        task_swapchain_image.set_image(swapchain_image);
        if (swapchain_image.is_empty()) continue; // A swapchain image will be empty when there is no available image from the swapchain

        // Records, submits and presents everything declared in the task graph.
        Deferred_Graph.execute({});
        // Forward_Graph.execute({});


        // The device(GPU) performs all memory reclaiming in the collect_garbage call.
        // It's best to call it once at the end of each frame.
        GPU.collect_garbage();
    }

    GPU.destroy_buffer(proj_view_Buffer_ID);
    GPU.destroy_buffer(frameInfo_buff_id);
    GPU.destroy_buffer(g_Buffer_views_ID);
    scene.clear();
    GPU.destroy_sampler(sampler);

    destroyGBuffer(GPU);
    GPU.destroy_image(depthAttachment);
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
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT
    });
}

daxa::ImageId createAttachment(daxa::Device& GPU, daxa::Swapchain swapchain, daxa::Format format, std::string_view name)
{
    daxa::Extent2D extent = swapchain.get_surface_extent();
    
    return GPU.create_image({
        .format = format,
        .size = {extent.x, extent.y, 1},
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
        .name = name,
    });
}


