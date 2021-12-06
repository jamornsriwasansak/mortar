#pragma once

#include "pch/pch.h"

#include "logger.h"
#include "stopwatch.h"
#include "vmath.h"

struct GlfwHandler
{
    static GlfwHandler &
    Inst()
    {
        static GlfwHandler singleton;
        return singleton;
    }

    std::vector<const char *>
    query_glfw_extensions() const
    {
        // query extensions and number of extensions
        uint32_t      num_glfw_extension = 0;
        const char ** glfw_extensions    = glfwGetRequiredInstanceExtensions(&num_glfw_extension);

        // turn it into vector
        std::vector<const char *> result(num_glfw_extension);
        for (size_t i_ext = 0; i_ext < num_glfw_extension; i_ext++)
        {
            result[i_ext] = glfw_extensions[i_ext];
        }

        // return result
        return result;
    }

    void
    poll_events()
    {
        glfwPollEvents();
    }

private:
    GlfwHandler()
    {
        Logger::Info("GLFWwindow instance is being constructed");

        // init glfw3
        auto init_result = glfwInit();
        if (init_result == GLFW_FALSE)
        {
            Logger::Critical<true>("glfwInit failed");
        }

        // check if vulkan is supported or not
        auto is_vulkan_supported = glfwVulkanSupported();
        if (is_vulkan_supported == GLFW_FALSE)
        {
            Logger::Critical<true>("glfw does not support vulkan");
            exit(0);
        }
    }

    GlfwHandler(const GlfwHandler &) = delete;

    ~GlfwHandler() { glfwTerminate(); }
};

enum class KeyEventEnum
{
    JustPress,
    JustRelease,
    Hold,
    Release
};

template <int TGlfwKey>
struct KeyEvent
{
    KeyEventEnum m_event;
};

#define DECL_KEY_EVENT(KEY)   KeyEvent<GLFW_KEY_##KEY> m_##KEY
#define UPDATE_KEY_EVENT(KEY) update_key(&m_##KEY)

struct Window
{
    DECL_KEY_EVENT(A);
    DECL_KEY_EVENT(B);
    DECL_KEY_EVENT(C);
    DECL_KEY_EVENT(D);
    DECL_KEY_EVENT(E);
    DECL_KEY_EVENT(F);
    DECL_KEY_EVENT(G);
    DECL_KEY_EVENT(H);
    DECL_KEY_EVENT(I);
    DECL_KEY_EVENT(J);
    DECL_KEY_EVENT(K);
    DECL_KEY_EVENT(L);
    DECL_KEY_EVENT(M);
    DECL_KEY_EVENT(N);
    DECL_KEY_EVENT(O);
    DECL_KEY_EVENT(P);
    DECL_KEY_EVENT(Q);
    DECL_KEY_EVENT(R);
    DECL_KEY_EVENT(S);
    DECL_KEY_EVENT(T);
    DECL_KEY_EVENT(U);
    DECL_KEY_EVENT(V);
    DECL_KEY_EVENT(W);
    DECL_KEY_EVENT(X);
    DECL_KEY_EVENT(Y);
    DECL_KEY_EVENT(Z);
    DECL_KEY_EVENT(0);
    DECL_KEY_EVENT(1);
    DECL_KEY_EVENT(2);
    DECL_KEY_EVENT(3);
    DECL_KEY_EVENT(4);
    DECL_KEY_EVENT(5);
    DECL_KEY_EVENT(6);
    DECL_KEY_EVENT(7);
    DECL_KEY_EVENT(8);
    DECL_KEY_EVENT(9);
    DECL_KEY_EVENT(LEFT_CONTROL);

    GLFWwindow *                  m_glfw_window = nullptr;
    std::string                   m_title;
    AvgFrameTimeStopWatch         m_stop_watch;
    std::optional<vk::SurfaceKHR> m_vk_surface;

    Window(const std::string title, const int2 & resolution)
    {
        // call instance at least once to init singleton
        GlfwHandler::Inst();

        // setup glfw hints
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        // create glfw windows
        m_title       = title;
        m_glfw_window = glfwCreateWindow(resolution.x, resolution.y, title.c_str(), nullptr, nullptr);
        if (m_glfw_window == nullptr)
        {
            Logger::Critical<true>("glfw could not create window");
        }

        glfwSetWindowUserPointer(m_glfw_window, this);
        // glfwSetFramebufferSizeCallback(m_glfw_window, framebufferResizeCallback);

        // focus on created glfw window
        glfwMakeContextCurrent(m_glfw_window);
    }

    Window(Window && rhs)
    {
        assert(rhs.m_glfw_window != nullptr);
        m_glfw_window     = rhs.m_glfw_window;
        m_title           = rhs.m_title;
        m_stop_watch      = m_stop_watch;
        rhs.m_glfw_window = nullptr;
    }

    Window &
    operator=(Window && rhs)
    {
        if (this != &rhs)
        {
            assert(rhs.m_glfw_window != nullptr);
            m_glfw_window     = rhs.m_glfw_window;
            m_title           = rhs.m_title;
            m_stop_watch      = m_stop_watch;
            rhs.m_glfw_window = nullptr;
        }
        return *this;
    }

    ~Window()
    {
        if (m_glfw_window != nullptr)
        {
            glfwDestroyWindow(m_glfw_window);
        }
    }

    HWND
    get_hwnd() const
    {
        return glfwGetWin32Window(m_glfw_window);
    }

    int2
    get_resolution() const
    {
        int2 result;
        glfwGetWindowSize(m_glfw_window, &result.x, &result.y);
        return result;
    }

    bool
    should_close_window() const
    {
        return glfwWindowShouldClose(m_glfw_window);
    }

    template <int GlfwKey>
    void
    update_key(KeyEvent<GlfwKey> * key_event) const
    {
        const bool is_press       = glfwGetKey(m_glfw_window, GlfwKey) == GLFW_PRESS;
        const auto prev_key_event = key_event->m_event;
        if ((prev_key_event == KeyEventEnum::Release) || (prev_key_event == KeyEventEnum::JustRelease))
        {
            if (is_press)
            {
                key_event->m_event = KeyEventEnum::JustPress;
            }
            else
            {
                key_event->m_event = KeyEventEnum::Release;
            }
        }
        else if ((prev_key_event == KeyEventEnum::JustPress) || (prev_key_event == KeyEventEnum::Hold))
        {
            if (is_press)
            {
                key_event->m_event = KeyEventEnum::Hold;
            }
            else
            {
                key_event->m_event = KeyEventEnum::JustRelease;
            }
        }
        else
        {
            key_event->m_event = KeyEventEnum::Release;
        }
    }

    void
    update()
    {
        m_stop_watch.tick();

        UPDATE_KEY_EVENT(A);
        UPDATE_KEY_EVENT(B);
        UPDATE_KEY_EVENT(C);
        UPDATE_KEY_EVENT(D);
        UPDATE_KEY_EVENT(E);
        UPDATE_KEY_EVENT(F);
        UPDATE_KEY_EVENT(G);
        UPDATE_KEY_EVENT(H);
        UPDATE_KEY_EVENT(I);
        UPDATE_KEY_EVENT(J);
        UPDATE_KEY_EVENT(K);
        UPDATE_KEY_EVENT(L);
        UPDATE_KEY_EVENT(M);
        UPDATE_KEY_EVENT(N);
        UPDATE_KEY_EVENT(O);
        UPDATE_KEY_EVENT(P);
        UPDATE_KEY_EVENT(Q);
        UPDATE_KEY_EVENT(R);
        UPDATE_KEY_EVENT(S);
        UPDATE_KEY_EVENT(T);
        UPDATE_KEY_EVENT(U);
        UPDATE_KEY_EVENT(V);
        UPDATE_KEY_EVENT(W);
        UPDATE_KEY_EVENT(X);
        UPDATE_KEY_EVENT(Y);
        UPDATE_KEY_EVENT(Z);
        UPDATE_KEY_EVENT(0);
        UPDATE_KEY_EVENT(1);
        UPDATE_KEY_EVENT(2);
        UPDATE_KEY_EVENT(3);
        UPDATE_KEY_EVENT(4);
        UPDATE_KEY_EVENT(5);
        UPDATE_KEY_EVENT(6);
        UPDATE_KEY_EVENT(7);
        UPDATE_KEY_EVENT(8);
        UPDATE_KEY_EVENT(9);
        UPDATE_KEY_EVENT(LEFT_CONTROL);

        if (m_stop_watch.m_just_updated)
        {
            std::string title =
                m_title + " [" + std::to_string(m_stop_watch.m_average_frame_time) + "]";
            glfwSetWindowTitle(m_glfw_window, title.c_str());
        }
    }
};
