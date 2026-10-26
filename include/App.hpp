#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

#include <GLFW/glfw3.h>


#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
    #define GLFW_NATIVE_INCLUDE_NONE
    using HWND = void*;
#elif defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3native.h> // Platform-specific GLFW functions



struct AppWindow {

    GLFWwindow* glfw_window_ptr;
    u32 m_width, m_height;
    bool minimized = false;
    bool swapchain_out_of_date = false;


    AppWindow(char const* window_name, u32 sx = 800, u32 sy = 600)
    : m_width{sx}, m_height{sy}
    {
        glfwInit(); // Initialize GLFW

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        glfw_window_ptr = glfwCreateWindow(static_cast<i32>(m_width), static_cast<i32>(m_height), window_name, nullptr, nullptr);

        glfwSetWindowUserPointer(glfw_window_ptr, this);

        glfwSetWindowSizeCallback(glfw_window_ptr, [](GLFWwindow* window, int width, int height) {

            auto* win = static_cast<AppWindow*>(glfwGetWindowUserPointer(window));
            win->m_width = width;
            win->m_height = height;
            win->swapchain_out_of_date = true;
            
        });
        
        glfwSetWindowIconifyCallback(glfw_window_ptr, [](GLFWwindow* window, int iconofied) {
            auto* win = static_cast<AppWindow*>(glfwGetWindowUserPointer(window));
            win->minimized = iconofied == GLFW_TRUE;
        });

        // glfwSetCursorPosCallback(glfw_window_ptr, )


        
    }

    ~AppWindow() {
        glfwDestroyWindow(glfw_window_ptr); // Destroy the GLFW window
        glfwTerminate();                    // Terminate GLFW
    }

    
    
    auto get_native_window_info() const -> daxa::NativeWindowInfo {
        #if defined(_WIN32)
        return daxa::NativeWindowInfoWin32{glfwGetWin32Window(glfw_window_ptr)};
        #elif defined(__linux__)
        switch (glfwGetPlatform()) {
            case GLFW_PLATFORM_WAYLAND:
            return daxa::NativeWindowInfoWayland{
                .display = glfwGetWaylandDisplay(),
                .surface = glfwGetWaylandWindow(glfw_window_ptr),
                .width = m_width,
                .height = m_height,
            };
            case GLFW_PLATFORM_X11:
            default:
            return daxa::NativeWindowInfoXlib{
                .window = reinterpret_cast<void *>(glfwGetX11Window(glfw_window_ptr))
            };
        }
        #endif
    }

    inline void set_mouse_capture(bool should_capture) const {
        glfwSetCursorPos(glfw_window_ptr, static_cast<f64>(m_width/2.), static_cast<f64>(m_height/2.));
        glfwSetInputMode(glfw_window_ptr, GLFW_CURSOR, should_capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        glfwSetInputMode(glfw_window_ptr, GLFW_RAW_MOUSE_MOTION, should_capture);
    }

    inline bool should_close() const {
        return glfwWindowShouldClose(glfw_window_ptr);
    }

    inline void update() const {
        if (minimized)
            glfwWaitEvents();
        else    
            glfwPollEvents();
    }

    float getAspect()
    {
        return (float)m_width/(float)m_height;
    }

};