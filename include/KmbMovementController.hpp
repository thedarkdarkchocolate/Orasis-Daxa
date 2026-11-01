#pragma once 

#include "GameObjet.hpp"
#include "App.hpp"


class KmbMovementController {


    public:

        struct KeyMappings {

            int moveLeft = GLFW_KEY_A;
            int moveRight = GLFW_KEY_D;
            int moveForward = GLFW_KEY_W;
            int moveBackward = GLFW_KEY_S;
            int moveUp = GLFW_KEY_SPACE;
            int moveDown = GLFW_KEY_LEFT_CONTROL;
            int lookLeft = GLFW_KEY_LEFT;
            int lookRight = GLFW_KEY_RIGHT;
            int lookUp = GLFW_KEY_UP;
            int lookDown = GLFW_KEY_DOWN;
            int esc = GLFW_KEY_ESCAPE;
            int shift = GLFW_KEY_LEFT_SHIFT;
            int tab = GLFW_KEY_TAB;

        };

        KeyMappings keys{};
        float moveSpeed{8.f};
        float lookSpeed{1.5f};
        double cursorPrev_X;
        double cursorPrev_Y;
        double sensitivity{0.001f};
        bool firstMouse{true};
        bool mouseCaptured{true};   // main.cpp captures the mouse at startup
        bool tabWasDown{false};     // Tab state on the previous frame, to detect the moment it is pressed

        AppWindow& m_app;


        KmbMovementController(AppWindow& app)
        : m_app{app}
        {}

        void moveInPlaneXZ(GLFWwindow* window, GameObject& gameObject, float dt)
        {
            
            float mvspeed = moveSpeed;

            // Toggle on the frame Tab goes down, not on every frame it is held.
            const bool tabIsDown = glfwGetKey(window, keys.tab) == GLFW_PRESS;
            if (tabIsDown && !tabWasDown)
            {
                mouseCaptured = !mouseCaptured;
                m_app.set_mouse_capture(mouseCaptured);

                // set_mouse_capture recentres the cursor; restart the delta so the camera does not jump.
                firstMouse = true;
            }
            tabWasDown = tabIsDown;

            if (glfwGetKey(window, keys.esc) == GLFW_PRESS) 
                glfwSetWindowShouldClose(window, GL_TRUE);

            if (glfwGetKey(window, keys.shift) == GLFW_PRESS) 
                mvspeed += 10.f;
            
            double cursorCurr_X, cursorCurr_Y;
            
            glfwGetCursorPos(window, &cursorCurr_X, &cursorCurr_Y);

            if (firstMouse) {               
                cursorPrev_X = cursorCurr_X;
                cursorPrev_Y = cursorCurr_Y;
                firstMouse = false;
            }
            
            
            // With the cursor released the mouse is used for something else, so it must not turn the camera.
            if (mouseCaptured)
            {
                gameObject.m_transform.rotation.y += (cursorCurr_X - cursorPrev_X) * sensitivity;
                gameObject.m_transform.rotation.x += (cursorCurr_Y - cursorPrev_Y) * sensitivity;
            }

            cursorPrev_X = cursorCurr_X;
            cursorPrev_Y = cursorCurr_Y;

            gameObject.m_transform.rotation.y = glm::mod(gameObject.m_transform.rotation.y, glm::two_pi<float>());
            gameObject.m_transform.rotation.x = glm::clamp(gameObject.m_transform.rotation.x, -1.5f, 1.5f);
            
            glm::vec3 moveDir{0.f};

            float yaw = gameObject.m_transform.rotation.y;
            const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
            const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
            const glm::vec3 upDir{0.f, 1.f, 0.f};
            
            if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
            if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
            if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
            if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
            if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
            if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

                
            if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
                gameObject.m_transform.translation += glm::normalize(moveDir) * mvspeed * dt;
        }

    
    
};


