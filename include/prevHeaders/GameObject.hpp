#pragma once

#include "Model.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>


namespace Orasis {

    struct TransformComponent 
    {
        glm::vec3 translation{};
        glm::vec3 scale{1.f};
        glm::vec3 rotation;

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
    
    struct PointLightComponent {
       float intensity = 1.f; 
    };

    
    class GameObject
    {

        public:

            using uint = unsigned int;
            using uMap = std::unordered_map<uint, GameObject>;
            
            uint id;
            std::shared_ptr<Model> model{};
            glm::vec3 color;
            TransformComponent transform{};

            std::unique_ptr<PointLightComponent> pointLight = nullptr;
        
        
        private:

            GameObject(uint _id) 
            : id{_id}
            {}


        public:
            
            GameObject(const GameObject&) = delete;
            GameObject &operator=(const GameObject&) = delete;
            
            GameObject(GameObject&&) = default; 
            GameObject &operator=(GameObject&&) = default;
            
            


            static GameObject createGameObject()
            {
                static uint currID = 0;
                return GameObject(currID++);
            }

            uint getID() const
            {
                return id;
            }

            static GameObject makeModel(daxa::Device GPU, const char* fp)

            static GameObject makePointLight(float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.f))
            {
                GameObject gameObj = GameObject::createGameObject();
                gameObj.color = color;
                gameObj.transform.scale.x = radius;
                gameObj.pointLight = std::make_unique<PointLightComponent>();
                gameObj.pointLight->intensity = intensity;

                return gameObj;

            }
            


    };





}