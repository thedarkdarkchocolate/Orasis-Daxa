#pragma once

#include "GameObjet.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>


struct Scene {

    daxa::Device m_GPU;
    std::unordered_map<std::string, std::shared_ptr<Model>> m_models;
    std::unordered_map<uint32_t, GameObject> m_objects;
    std::vector<uint32_t> m_point_light_Objects_IDs;
    std::vector<uint32_t> m_directional_light_Objects_IDs;

    // Skybox image
    daxa::ImageId m_skybox_ID;
    daxa::ImageViewId m_skybox_image_view;

    // Can change based on the scene temporaely placed here
    glm::vec3 m_shadowCenter{0.f, 6.f, 0.f}; 
    float m_shadowRadius = 20.f;

    Scene(daxa::Device GPU)
    : m_GPU{GPU}
    {}

    uint32_t add(const std::string& path, TransformComponent t = {})
    {
        if(m_models[path] == nullptr)
            m_models[path] = std::make_shared<Model>(m_GPU, path.c_str());
        
        GameObject tmp = GameObject::createGameObject();
        tmp.m_model = m_models[path];
        tmp.m_transform = t;
        tmp.createModelMatrixBuffer(m_GPU);
        
        uint32_t id = tmp.ID;
        m_objects.emplace(id, std::move(tmp));
        return id;
    }

    uint32_t add_point_Light(const std::string& path, float intensity, glm::vec3 color, TransformComponent t = {})
    {
        if(m_models[path] == nullptr)
            m_models[path] = std::make_shared<Model>(m_GPU, path.c_str());
        
        GameObject tmp = GameObject::createPointLight(intensity, color);
        tmp.m_model = m_models[path];
        tmp.m_transform = t;
        tmp.createModelMatrixBuffer(m_GPU);

        uint32_t id = tmp.ID;
        m_point_light_Objects_IDs.push_back(id);
        m_objects.emplace(id, std::move(tmp));
        return id;

    }
    
    uint32_t add_directional_Light(float intensity, glm::vec3 color, glm::vec3 dir)
    {
        GameObject tmp = GameObject::createDirectionalLight(intensity, dir, color);
        tmp.m_dirLight->updateLightSpace(m_shadowCenter, m_shadowRadius);
        
        uint32_t id = tmp.ID;
        m_directional_light_Objects_IDs.push_back(id);
        m_objects.emplace(id, std::move(tmp));
        return id;
    }
    
    daxa::ImageId add_skybox(const std::vector<std::string>& images_path)
    {
        int width, height, nrChannels, requestedChannels = 4;

        if(!m_skybox_ID.is_empty())
            m_GPU.destroy_image(m_skybox_ID);
        
        for (unsigned i = 0; i < images_path.size(); i++)
        {
            if (i == 0 || i == 1 || i == 2 || i ==3 )
                stbi_set_flip_vertically_on_load(false);
            stbi_uc* data = stbi_load(images_path[i].c_str(), &width, &height, &nrChannels, requestedChannels);
            

            if(data)
            {


                if(m_skybox_ID.is_empty())
                {
                    m_skybox_ID = m_GPU.create_image({
                        .flags = daxa::ImageCreateFlagBits::COMPATIBLE_CUBE, 
                        .format = daxa::Format::R8G8B8A8_SRGB,
                        .size = {.x = (unsigned)width, .y = (unsigned)height, .z = 1},
                        .array_layer_count = (unsigned)images_path.size(),
                        .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_DST ,
                        .name = "Skybox cubemap"
                    });
    
                    m_skybox_image_view = m_GPU.create_image_view({
                        .type = daxa::ImageViewType::CUBE,
                        .format = daxa::Format::R8G8B8A8_SRGB,
                        .image = m_skybox_ID,
                        .slice = { .base_array_layer = 0, .layer_count = (unsigned)images_path.size(),},
                        .name = "Skybox cubemap view",
                    });

                }


                daxa::BufferId staging_buff;
                
                size_t size = sizeof(stbi_uc) * width * height * requestedChannels;
                
                staging_buff = m_GPU.create_buffer({
                    .size = size,
                    .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
                });

                stbi_uc* staging_buff_ptr = m_GPU.buffer_host_address_as<stbi_uc>(staging_buff).value();

                memcpy(staging_buff_ptr, data, size);

                stbi_image_free(data);

                auto cmnd_recorder = m_GPU.create_command_recorder({
                    .queue_type = daxa::QueueType::MAIN
                });

                cmnd_recorder.pipeline_image_barrier({
                    .src_access = daxa::AccessConsts::NONE,
                    .dst_access = daxa::AccessConsts::TRANSFER_WRITE,
                    .image = m_skybox_ID,
                    .layout_operation = daxa::ImageLayoutOperation::TO_GENERAL,
                });

                cmnd_recorder.copy_buffer_to_image({
                    .src_buffer = staging_buff,
                    .buffer_offset = 0,
                    .dst_image = m_skybox_ID,
                    .image_slice = {.base_array_layer = i},
                    .image_extent = {(uint32_t)width, (uint32_t)height, 1},
                });
                
                cmnd_recorder.pipeline_image_barrier({
                    .src_access = daxa::AccessConsts::TRANSFER_WRITE,
                    .dst_access = daxa::AccessConsts::FRAGMENT_SHADER_READ,
                    .image = m_skybox_ID,
                });

                
                std::array<daxa::ExecutableCommandList, 1> cmndList = {cmnd_recorder.complete_current_commands()};

                m_GPU.submit_commands({
                    .command_lists = cmndList
                });
                
                m_GPU.destroy_buffer(staging_buff);


            
            }
            else
            {
                std::cout << "Cubemap tex failed to load: " << stbi_failure_reason() << "\n";
                stbi_image_free(data);

            }


        }

        return m_skybox_ID;

    }   

    uint32_t addEmpty(TransformComponent t = {})
    {
        GameObject tmp = GameObject::createGameObject();
        
        tmp.m_transform = t;
        uint32_t id = tmp.ID;
        m_objects.emplace(id, std::move(tmp));

        return id;

    }
    
    GameObject& get(uint32_t id)
    {
        return m_objects.at(id);
    }

    void remove(uint32_t id)
    {
        auto it = m_objects.find(id);
        if (it == m_objects.end()) return;
        if (!it->second.m_world_space_buff_ID.is_empty())
            m_GPU.destroy_buffer(it->second.m_world_space_buff_ID);
        
        m_objects.erase(it);
        std::erase(m_point_light_Objects_IDs, id);
        std::erase(m_directional_light_Objects_IDs, id);

    }   
    
    void clear()
    {
        for (auto it = m_objects.begin(); it != m_objects.end(); it++)
        {
            if (it == m_objects.end()) break;
            if (!it->second.m_world_space_buff_ID.is_empty())
                m_GPU.destroy_buffer(it->second.m_world_space_buff_ID);
            
        }

        m_point_light_Objects_IDs.clear();
        m_directional_light_Objects_IDs.clear();
        m_objects.clear();
        m_models.clear();

        if(!m_skybox_ID.is_empty())
            m_GPU.destroy_image(m_skybox_ID);
        if(!m_skybox_image_view.is_empty())
            m_GPU.destroy_image_view(m_skybox_image_view);
    }   

    
};