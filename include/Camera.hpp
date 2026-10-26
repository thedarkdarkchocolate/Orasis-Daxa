#pragma once 

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cassert>
#include <limits>

struct Camera {

    private:

    glm::mat4 projMatrix {1.f};
    glm::mat4 viewMatrix {1.f};
    glm::vec3 cameraPos {0.f};


    public:


    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, 1.f, 0.f})
    {
        const glm::vec3 w{glm::normalize(direction)};
        const glm::vec3 u{glm::normalize(glm::cross(w, up))};
        const glm::vec3 v{glm::cross(w, u)};
        
        viewMatrix = glm::mat4{1.f};
        viewMatrix[0][0] = u.x;
        viewMatrix[1][0] = u.y;
        viewMatrix[2][0] = u.z;
        viewMatrix[0][1] = v.x;
        viewMatrix[1][1] = v.y;
        viewMatrix[2][1] = v.z;
        viewMatrix[0][2] = w.x;
        viewMatrix[1][2] = w.y;
        viewMatrix[2][2] = w.z;
        viewMatrix[3][0] = -glm::dot(u, position);
        viewMatrix[3][1] = -glm::dot(v, position);
        viewMatrix[3][2] = -glm::dot(w, position);
    }
    
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, 1.f, 0.f})
    {
        setViewDirection(position, target - position, up);
    }

    void setViewYXZ(glm::vec3 position, glm::vec3 rotation)
    {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        const glm::vec3 u{(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)};
        const glm::vec3 v{(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)};
        const glm::vec3 w{(c2 * s1), (-s2), (c1 * c2)};
        viewMatrix = glm::mat4{1.f};
        viewMatrix[0][0] = u.x;
        viewMatrix[1][0] = u.y;
        viewMatrix[2][0] = u.z;
        viewMatrix[0][1] = v.x;
        viewMatrix[1][1] = v.y;
        viewMatrix[2][1] = v.z;
        viewMatrix[0][2] = w.x;
        viewMatrix[1][2] = w.y;
        viewMatrix[2][2] = w.z;
        viewMatrix[3][0] = -glm::dot(u, position);
        viewMatrix[3][1] = -glm::dot(v, position);
        viewMatrix[3][2] = -glm::dot(w, position);
    }

    void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far)
    {
        projMatrix = glm::mat4{1.0f};
        projMatrix[0][0] = 2.f / (right - left);
        projMatrix[1][1] = 2.f / (bottom - top);
        projMatrix[2][2] = 1.f / (far - near);
        projMatrix[3][0] = -(right + left) / (right - left);
        projMatrix[3][1] = -(bottom + top) / (bottom - top);
        projMatrix[3][2] = -near / (far - near);
    }

    void setPrespectiveProjection(float yfov, float aspect, float near, float far)
    {
        assert(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);
        const float tanHalfFov = tan(yfov / 2.f);
        
        projMatrix = glm::mat4{0.0f};
        projMatrix[0][0] = 1.f / (aspect * tanHalfFov);
        projMatrix[1][1] = -1.f / (tanHalfFov);
        projMatrix[2][2] = far / (far - near);
        projMatrix[2][3] = 1.f;
        projMatrix[3][2] = -(far * near) / (far - near);
    }


    void setCameraPos(glm::vec3 position)
    {
        cameraPos = position;
    }


    const glm::vec3 getCameraPos() const
    {
        return cameraPos;
    } 

    const glm::mat4& getProjection() const
    {
        return projMatrix;
    } 
    
    const glm::mat4& getViewMatrix() const
    {
        return viewMatrix;
    } 

};


