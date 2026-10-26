#pragma once 

#include "Model.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

struct TransformComponent 
{
    glm::vec3 translation{};
    glm::vec3 scale{1.f};
    glm::vec3 rotation{};

    glm::mat4 mat4() {
        
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        
        return glm::mat4{
            {
                scale.x * (c1 * c3 + s1 * s2 * s3),
                scale.x * (c2 * s3),
                scale.x * (c1 * s2 * s3 - c3 * s1),
                0.0f,
            },
            {
                scale.y * (c3 * s1 * s2 - c1 * s3),
                scale.y * (c2 * c3),
                scale.y * (c1 * c3 * s2 + s1 * s3),
                0.0f,
            },
            {
                scale.z * (c2 * s1),
                scale.z * (-s2),
                scale.z * (c1 * c2),
                0.0f,
            },
            {translation.x, translation.y, translation.z, 1.0f}};
        }
};

struct PointLightComponent
{
    float intensity = 1.f;
    glm::vec3 light_color;
};


struct GameObject
{
    private:
    
    static uint32_t currID;

    public:


    
    std::shared_ptr<Model> m_model;
    uint32_t ID;
    TransformComponent m_transform;
    daxa::BufferId m_world_space_buff_ID;
    Model_Matrix* m_world_space_ptr = nullptr;
    glm::vec3 color;

    // Light Compomnent
    std::unique_ptr<PointLightComponent> m_pointLight;
    

    private:

    GameObject(uint32_t id) 
    : ID{id}
    {}


    GameObject(const GameObject&) = delete;
    GameObject &operator=(const GameObject&) = delete;
    
    public:

    GameObject(GameObject&& other)
    : 
    m_model {other.m_model},
    ID {other.ID},
    m_transform {other.m_transform},
    m_world_space_buff_ID {other.m_world_space_buff_ID},
    m_world_space_ptr {other.m_world_space_ptr},
    color {other.color},
    m_pointLight {std::move(other.m_pointLight)}
    {   
        other.m_model.reset();
    } 
    GameObject& operator=(GameObject&& other)
    {   
        m_model = other.m_model;
        ID = other.ID;
        m_transform = other.m_transform;
        m_world_space_buff_ID = other.m_world_space_buff_ID;
        m_world_space_ptr = other.m_world_space_ptr;
        color = other.color;
        m_pointLight = std::move(other.m_pointLight);
        other.m_model.reset();
        return *this;
    } 




    void createBuffer(daxa::Device GPU)
    {
        m_world_space_buff_ID = GPU.create_buffer({
            .size = sizeof(Model_Matrix),
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE
        });

        m_world_space_ptr = GPU.buffer_host_address_as<Model_Matrix>(m_world_space_buff_ID).value();
        updateTransform();
    }
    
    
    static GameObject createGameObject()
    {
        return GameObject(currID++);
    }
            
    static GameObject createPointLight(float intensity = 10.f, glm::vec3 color = glm::vec3(1.f))
    {

        GameObject gameObj = GameObject::createGameObject();
        gameObj.m_pointLight = std::make_unique<PointLightComponent>();
        gameObj.m_pointLight->intensity = intensity;
        gameObj.m_pointLight->light_color = color;

        return gameObj;
            
    }

    void setPosition(glm::vec3 pos)
    {
        m_transform.translation = pos;
        updateTransform();
    }
    
    void updateTransform()
    {
        
        if (m_world_space_ptr != nullptr)
            m_world_space_ptr[0] = {.model = to_daxa(m_transform.mat4())};
    }
};
uint32_t GameObject::currID = 0;