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
    std::vector<uint32_t> m_lightObjects_IDs;

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
        tmp.createBuffer(m_GPU);
        
        uint32_t id = tmp.ID;
        m_objects.emplace(id, std::move(tmp));
        return id;
    }

    uint32_t addLight(const std::string& path, float intensity, glm::vec3 color, TransformComponent t = {})
    {
        if(m_models[path] == nullptr)
            m_models[path] = std::make_shared<Model>(m_GPU, path.c_str());
        
        GameObject tmp = GameObject::createPointLight(intensity, color);
        tmp.m_model = m_models[path];
        tmp.m_transform = t;
        tmp.createBuffer(m_GPU);

        uint32_t id = tmp.ID;
        m_lightObjects_IDs.push_back(id);
        m_objects.emplace(id, std::move(tmp));
        return id;

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
        std::erase(m_lightObjects_IDs, id);

    }   
    
    void clear()
    {
        for (auto it = m_objects.begin(); it != m_objects.end(); it++)
        {
            if (it == m_objects.end()) break;
            if (!it->second.m_world_space_buff_ID.is_empty())
                m_GPU.destroy_buffer(it->second.m_world_space_buff_ID);
            
        }

        m_lightObjects_IDs.clear();
        m_objects.clear();
        m_models.clear();
    }

    
};