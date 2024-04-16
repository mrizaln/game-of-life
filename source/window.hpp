#ifndef WINDOW_HPP_IROQWEOX
#define WINDOW_HPP_IROQWEOX

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <functional>
#include <optional>
#include <thread>
#include <queue>

class WindowManager;

struct MouseButton
{
    enum class State
    {
        RELEASED,
        PRESSED,
    };

    enum class Button
    {
        LEFT,
        RIGHT,
        MIDDLE,
    };

    State m_right;
    State m_left;
    State m_middle;

    State& operator[](Button button)
    {
        switch (button) {
        case Button::LEFT:
            return m_left;
        case Button::RIGHT:
            return m_right;
        case Button::MIDDLE:
            return m_middle;
        default:
            [[unlikely]] return m_left;    // just to suppress the warning
        }
    }

    State operator[](Button button) const
    {
        switch (button) {
        case Button::LEFT:
            return m_left;
        case Button::RIGHT:
            return m_right;
        case Button::MIDDLE:
            return m_middle;
        default:
            [[unlikely]] return m_left;    // just to suppress the warning
        }
    }
};

struct WindowProperties
{
    std::string         m_title;
    int                 m_width;
    int                 m_height;
    glm::vec<2, double> m_cursorPos;
    MouseButton         m_mouseButton;
};

class Window
{
public:
    friend WindowManager;

    using KeyEvent    = int;    // GLFW_KEY_*
    using KeyModifier = int;    // GLFW_MOD_*
    enum class KeyActionType
    {
        CALLBACK,
        CONTINUOUS,
    };
    struct KeyEventHandler
    {
        KeyModifier                  mods;
        KeyActionType                action;
        std::function<void(Window&)> handler;
    };
    using KeyMap = std::unordered_multimap<KeyEvent, KeyEventHandler>;

    using CursorPosCallbackFun       = std::function<void(Window& window, double xPos, double yPos)>;
    using ScrollCallbackFun          = std::function<void(Window& window, double xOffset, double yOffset)>;
    using FramebufferSizeCallbackFun = std::function<void(Window& window, int width, int height)>;
    using MouseButtonCallback        = std::function<
        void(Window& window, MouseButton::Button button, MouseButton::State state, KeyModifier mods)>;

    Window(const Window&)           = delete;
    Window operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;
    ~Window();

    // use the context on current thread;
    void useHere();
    void unUse();
    void setWindowSize(int width, int height);
    void updateTitle(const std::string& title);

    // main rendering loop
    void    run(std::function<void()>&& func);
    void    enqueueTask(std::function<void()>&& func);
    void    requestClose();
    double  getDeltaTime() const;
    Window& setVsync(bool value);
    Window& setCaptureMouse(bool value);
    Window& setCursorPosCallback(CursorPosCallbackFun&& func);
    Window& setScrollCallback(ScrollCallbackFun&& func);
    Window& setFramebuffersizeCallback(FramebufferSizeCallbackFun&& func);
    Window& setMouseButtonCallback(MouseButtonCallback&& func);

    // The function added will be called from the window thread.
    Window& addKeyEventHandler(
        KeyEvent                       key,
        KeyModifier                    mods,
        KeyActionType                  action,
        std::function<void(Window&)>&& func
    );
    Window& addKeyEventHandler(
        std::initializer_list<KeyEvent> keys,
        KeyModifier                     mods,
        KeyActionType                   action,
        std::function<void(Window&)>&&  func
    );

    bool                    isVsyncEnabled() const { return m_vsync; }
    bool                    isMouseCaptured() const { return m_captureMouse; }
    WindowProperties&       getProperties() { return m_properties; }
    const WindowProperties& getProperties() const { return m_properties; }
    GLFWwindow*             getHandle() const { return m_windowHandle; }

    const std::optional<std::thread::id>& getAttachedThreadId() const { return m_attachedThreadId; };

private:
    Window() = default;
    Window(std::size_t id, GLFWwindow* handle, WindowProperties&& prop);

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xPos, double yPos);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

    void processInput();
    void processQueuedTasks();
    void updateDeltaTime();

    void swap(Window& other) noexcept;

    // window stuff
    std::size_t      m_id;
    bool             m_contextInitialized{ false };
    GLFWwindow*      m_windowHandle;
    WindowProperties m_properties;
    bool             m_vsync{ true };

    // input
    KeyMap                     m_keyMap;
    CursorPosCallbackFun       m_cursorPosCallback;
    ScrollCallbackFun          m_scrollCallback;
    FramebufferSizeCallbackFun m_framebufferSizeCallback;
    MouseButtonCallback        m_mouseButtonCallback;

    std::queue<std::function<void()>> m_taskQueue;

    double m_lastFrameTime{ 0.0 };
    double m_deltaTime{ 0.0 };

    bool                           m_captureMouse{ false };
    std::optional<std::thread::id> m_attachedThreadId;

    mutable std::mutex m_windowMutex;
    mutable std::mutex m_queueMutex;
};

#endif /* end of include guard: WINDOW_HPP_IROQWEOX */
