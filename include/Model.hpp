#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <daxa/daxa.hpp>
#include "cgltf.h"
#include "stb_image.h"
#include "Daxa_GLM_helper.hpp"
#include "shader/shared.inl"

#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <array>


struct Vertex_HOST
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
    
};


struct Model {

    struct Draw_Indices {
        uint32_t start = 0;
        uint32_t count = 0;
        int32_t slot = -1;

        glm::vec2 uv_scale{1.f};
        glm::vec4 mat_color{1.f};
    };

    std::vector<Vertex_HOST> m_vertices;
    std::vector<uint32_t> m_indices;

    const char* filepath;
    bool hasIndexBuffer = false;
    std::vector<bool> m_hasTexture;

    daxa::Device m_GPU;
    daxa::BufferId m_vertex_buffer_ID;
    daxa::BufferId m_index_buffer_ID;
    daxa::ImageId m_empty_ImageID;
    std::vector<daxa::ImageId> m_images_IDs;
    std::vector<Draw_Indices> m_draw_ranges;
    daxa::u32 index_count = 0;

    Model(daxa::Device GPU, const char* fp)
    : m_GPU{GPU}, filepath{fp}
    {
        // Called outside the assert: in Release builds assert() and its argument are compiled out.
        bool loaded = loadmodel();
        if (!loaded) std::cerr << "Failed to load model: " << filepath << std::endl;
        assert(loaded);

        createBuffer();
    }
    
    Model(Model const &) = delete;
    Model & operator=(Model const &) = delete;
    Model(Model && other) noexcept;
    Model & operator=(Model && other) noexcept;

    ~Model()
    {
        if (!m_vertex_buffer_ID.is_empty()) m_GPU.destroy_buffer(m_vertex_buffer_ID);
        if (!m_index_buffer_ID.is_empty())  m_GPU.destroy_buffer(m_index_buffer_ID);
        
        for (auto& image: m_images_IDs)
            if (!image.is_empty())
                m_GPU.destroy_image(image);
    }


    bool createBuffer()
    {

        // Vertex Buffer

        m_vertex_buffer_ID = m_GPU.create_buffer({
            .size = sizeof(Vertex_HOST) * m_vertices.size(),
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
        });

        Vertex* vertex_buff_ptr = m_GPU.buffer_host_address_as<Vertex>(m_vertex_buffer_ID).value();
        std::memcpy(vertex_buff_ptr, m_vertices.data(), sizeof(Vertex_HOST) * m_vertices.size());

        // Index buffer
        if (hasIndexBuffer){
        
                daxa::types::usize size = sizeof(u32) * m_indices.size();
        
                m_index_buffer_ID = m_GPU.create_buffer({
                    .size = size,
                    .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
                });
        
                daxa::types::u32* index_buff_ptr = m_GPU.buffer_host_address_as<daxa::types::u32>(m_index_buffer_ID).value();
                std::memcpy(index_buff_ptr, m_indices.data(), size);

        }

        return true;

    }

    bool uploadImage(stbi_uc* pixels, int width, int height, int file_channels, daxa::ImageId& imageID)
    {

        if (width == 0 || height == 0)
            return false;

        daxa::BufferId m_image_staging_buffer_ID;


        imageID = m_GPU.create_image({
            .format = daxa::Format::R8G8B8A8_SRGB,
            .size = {.x = (uint32_t)width, .y = (uint32_t)height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
        });

        size_t size = sizeof(stbi_uc) * width * height * file_channels;

        m_image_staging_buffer_ID = m_GPU.create_buffer({
            .size = sizeof(stbi_uc) * width * height * file_channels,
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
        });


        stbi_uc* image_buff_ptr = m_GPU.buffer_host_address_as<stbi_uc>(m_image_staging_buffer_ID).value();
        std::memcpy(image_buff_ptr, pixels, size);

        auto cmnd_recorder = m_GPU.create_command_recorder({
            .queue_type = daxa::QueueType::MAIN
        });

        cmnd_recorder.pipeline_image_barrier({
            .src_access = daxa::AccessConsts::NONE,
            .dst_access = daxa::AccessConsts::TRANSFER_WRITE,
            .image = imageID,
            .layout_operation = daxa::ImageLayoutOperation::TO_GENERAL,
        });
        
        cmnd_recorder.copy_buffer_to_image({
            .src_buffer = m_image_staging_buffer_ID,
            .buffer_offset = 0,
            .dst_image = imageID,
            .image_extent = {(uint32_t)width, (uint32_t)height, 1},
        });
        
        cmnd_recorder.pipeline_image_barrier({
            .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            .dst_access = daxa::AccessConsts::FRAGMENT_SHADER_READ,
            .image = imageID,
        });

        std::array<daxa::ExecutableCommandList, 1> cmndList = {cmnd_recorder.complete_current_commands()};

        m_GPU.submit_commands({
            .command_lists = cmndList
        });
        
        m_GPU.destroy_buffer(m_image_staging_buffer_ID);


        return true;
    }

    bool loadmodel()
    {
        cgltf_options options = {};
        cgltf_data* data = nullptr;

        if (cgltf_parse_file(&options, filepath, &data) != cgltf_result_success)
            return false;

        if (cgltf_load_buffers(&options, data, filepath) != cgltf_result_success) {
            cgltf_free(data);
            return false;
        }

        const cgltf_size totalImages = data->images_count;

        m_images_IDs.resize(totalImages);
        m_hasTexture.resize(totalImages);
        
        // A mesh is only a shape; the node that references it says where that shape is placed.
        for (cgltf_size n = 0; n < data->nodes_count; n++)
        {

            const cgltf_node& node = data->nodes[n];

            // Nodes can also be cameras, lights or empty groups.
            if (node.mesh == nullptr) continue;

            const cgltf_mesh& mesh = *node.mesh;

            // Node-to-model transform: this node's own transform combined with all of its parents'. cgltf calls it
            // "world", meaning the root of the file; for the renderer that is still the model's local space.
            cgltf_float node_matrix[16];
            cgltf_node_transform_world(&node, node_matrix);
            const glm::mat4 node_to_model = glm::make_mat4(node_matrix);
            // Normals are directions: they rotate with the node but must not be translated.
            const glm::mat3 node_to_model_rotation = glm::mat3(node_to_model);

            for (cgltf_size p = 0; p < mesh.primitives_count; p++)
            {
                const cgltf_primitive& primative = mesh.primitives[p];

                Draw_Indices draw_range;
                
                if (primative.material != nullptr) {

                    cgltf_pbr_metallic_roughness pbr_mtlc_roughness = primative.material->pbr_metallic_roughness;

                    if (pbr_mtlc_roughness.base_color_texture.texture != nullptr)
                        if (pbr_mtlc_roughness.base_color_texture.texture->image != nullptr)
                        {
                            cgltf_image* _image = pbr_mtlc_roughness.base_color_texture.texture->image;
                            cgltf_texture_view _texture = pbr_mtlc_roughness.base_color_texture;

                            if (_image->buffer_view != nullptr)
                            {
                                const cgltf_buffer_view* view = _image->buffer_view;

                                // The view is a window onto the .glb's binary blob: these are the compressed file bytes.
                                const stbi_uc* file_bytes = static_cast<const stbi_uc*>(view->buffer->data) + view->offset;

                                // Retriving the index of the image index value ranges from 0 to data->images_count
                                draw_range.slot = cgltf_image_index(data, _image);

                                if (_texture.has_transform)
                                    draw_range.uv_scale = {_texture.transform.scale[0], _texture.transform.scale[1]};
                                

                                if (m_images_IDs[draw_range.slot].is_empty())
                                {
                                    // Decode into raw pixels. Asking for 4 channels gives RGBA even if the file has 3.
                                    int width = 0, height = 0, file_channels = 0, final_channels = 4;
                                    stbi_uc* pixels = stbi_load_from_memory(file_bytes, static_cast<int>(view->size), &width, &height, &file_channels, final_channels);
                                    
                                    if (pixels == nullptr)
                                        std::cout << "Decode failed: " << stbi_failure_reason() << "\n";
                                    else
                                    {
                                        m_hasTexture[draw_range.slot] = uploadImage(pixels, width, height, final_channels, m_images_IDs[draw_range.slot]);          
                                        stbi_image_free(pixels);
                                    }

                                }
                            }
                        }

                        draw_range.mat_color = glm::make_vec4(pbr_mtlc_roughness.base_color_factor);
                }
                
                if (primative.type != cgltf_primitive_type_triangles) continue;

                // Find the accessor

                const cgltf_accessor* pos = nullptr;
                const cgltf_accessor* color = nullptr;
                const cgltf_accessor* norm = nullptr;
                const cgltf_accessor* uv = nullptr;

                for (cgltf_size a = 0; a < primative.attributes_count; ++a) {
                    const cgltf_attribute& attr = primative.attributes[a];
                    if (attr.type == cgltf_attribute_type_position)                    pos = attr.data;
                    if (attr.type == cgltf_attribute_type_normal)                      norm = attr.data;
                    if (attr.type == cgltf_attribute_type_color)                       color = attr.data;
                    if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uv  = attr.data;
                }
        
                if (!pos)
                    continue;
                
                // Pos hold all vertex positional data thus the count also 
                const uint32_t base = (uint32_t)m_vertices.size();
                m_vertices.resize(base + pos->count);
                draw_range.start = m_indices.size();
                

                for (cgltf_size i = 0; i < pos->count; i++)
                {
                    Vertex_HOST& vert = m_vertices[base + i];
                    cgltf_accessor_read_float(pos, i, glm::value_ptr(vert.position), 3);
                    vert.position = glm::vec3(node_to_model * glm::vec4(vert.position, 1.f));

                    if (norm)
                    {
                        cgltf_accessor_read_float(norm, i, glm::value_ptr(vert.normal), 3);
                        vert.normal = glm::normalize(node_to_model_rotation * vert.normal);
                    }
                    if (color) cgltf_accessor_read_float(color, i, glm::value_ptr(vert.color), 3);
                    if (uv) cgltf_accessor_read_float(uv, i, glm::value_ptr(vert.uv), 2);
                }

                if (primative.indices) {
                    for (cgltf_size i = 0; i < primative.indices->count; ++i)
                        m_indices.push_back(base + (uint32_t)cgltf_accessor_read_index(primative.indices, i));

                    draw_range.count = primative.indices->count;
                    m_draw_ranges.push_back(draw_range);
                } else {
                    /* Non-indexed primitive: vertices are already in triangle order */
                    for (cgltf_size i = 0; i < pos->count; ++i)
                        m_indices.push_back(base + (uint32_t)i);
                    
                    draw_range.count = primative.indices->count;
                    m_draw_ranges.push_back(draw_range);
                }

                
            }
        }
        
        
        index_count = static_cast<daxa::u32>(m_indices.size());
        if (index_count) hasIndexBuffer = true;
        
        cgltf_free(data);

        return true;
    }   



};