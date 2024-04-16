#include "window.hpp"

#include "timer.hpp"
#include "window_manager.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace
{
    std::size_t getThreadNum(const std::thread::id& threadId)
    {
        auto hash{ std::hash<std::thread::id>{} };
        return hash(threadId);
    }

    // helper function that decides whether to execute the task immediately or enqueue it.
    void runTask(Window* windowPtr, std::function<void()>&& func)
    {
        // If the Window is attached to the same thread as the windowManager, execute the task immediately, else enqueue
        // the task.
        if (windowPtr->getAttachedThreadId() == WindowManager::getAttachedThreadId()) {
            func();
        } else {
            windowPtr->enqueueTask(std::move(func));
        }
    }
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Window* windowWindow{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
    if (windowWindow == nullptr) {
        return;
    }

    runTask(windowWindow, [windowWindow, width, height] {
        if (windowWindow->m_framebufferSizeCallback) {
            windowWindow->m_framebufferSizeCallback(*windowWindow, width, height);
        }
        windowWindow->setWindowSize(width, height);
    });
}

void Window::keyCallback(GLFWwindow* window, int key, int /* scancode */, int action, int mods)
{
    auto* windowWindow{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
    if (windowWindow == nullptr) {
        return;
    }

    runTask(windowWindow, [windowWindow, key, action, mods] {
        auto& keyMap{ windowWindow->m_keyMap };
        auto  range{ keyMap.equal_range(key) };
        for (auto& [_, handler] : std::ranges::subrange(range.first, range.second)) {
            auto& [hmod, haction, hfun]{ handler };
            if (haction != KeyActionType::CALLBACK) {
                continue;
            }

            // ignore release and repeat event for now
            if (action == GLFW_RELEASE || action == GLFW_REPEAT) {
                continue;
            }

            // modifier match
            if ((mods & hmod) != 0) {
                hfun(*windowWindow);
            }

            // don't have modifier
            if (mods == 0 && hmod == 0) {
                hfun(*windowWindow);
            }
        }
    });
}

void Window::cursorPosCallback(GLFWwindow* window, double xPos, double yPos)
{
    auto* windowWindow{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
    if (windowWindow == nullptr) {
        return;
    }

    runTask(windowWindow, [windowWindow, xPos, yPos] {
        if (windowWindow->m_cursorPosCallback) {
            windowWindow->m_cursorPosCallback(*windowWindow, xPos, yPos);
        }
        windowWindow->m_properties.m_cursorPos = { xPos, yPos };
    });
}

void Window::scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    auto* windowWindow{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
    if (windowWindow == nullptr) {
        return;
    }

    runTask(windowWindow, [windowWindow, xOffset, yOffset] {
        if (windowWindow->m_scrollCallback) {
            windowWindow->m_scrollCallback(*windowWindow, xOffset, yOffset);
        }
    });
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* windowWindow{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
    if (windowWindow == nullptr) {
        return;
    }

    MouseButton::Button buttonButton;
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        buttonButton = MouseButton::Button::LEFT;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        buttonButton = MouseButton::Button::RIGHT;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        buttonButton = MouseButton::Button::MIDDLE;
        break;
    default:
        return;    // ignore other buttons
    }

    MouseButton::State buttonState{ action == GLFW_PRESS ? MouseButton::State::PRESSED
                                                         : MouseButton::State::RELEASED };

    runTask(windowWindow, [windowWindow, buttonButton, buttonState, mods] {
        if (windowWindow->m_mouseButtonCallback) {
            windowWindow->m_mouseButtonCallback(*windowWindow, buttonButton, buttonState, mods);
        }
        windowWindow->m_properties.m_mouseButton[buttonButton] = buttonState;
    });
}

// this constructor must be called only from main thread (WindowManager run in main thread)
Window::Window(std::size_t id, GLFWwindow* handle, WindowProperties&& prop)
    : m_id{ id }
    , m_windowHandle{ handle }
    , m_properties{ std::move(prop) }
    , m_attachedThreadId{ std::nullopt }
{
    useHere();
    if (!m_contextInitialized) {
        // this is a very exceptional case, throwing is OK
        if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
            throw std::runtime_error{ "Failed to initialize glad" };
        }
        m_contextInitialized = true;

        glfwSetFramebufferSizeCallback(m_windowHandle, Window::framebufferSizeCallback);
        glfwSetKeyCallback(m_windowHandle, Window::keyCallback);
        glfwSetCursorPosCallback(m_windowHandle, Window::cursorPosCallback);
        glfwSetScrollCallback(m_windowHandle, Window::scrollCallback);
        glfwSetMouseButtonCallback(m_windowHandle, mouseButtonCallback);
    }
    setVsync(m_vsync);
    glfwSetWindowUserPointer(m_windowHandle, this);
    unUse();
}

// clang-format off
    Window::Window(Window&& other) noexcept
        : m_id                     { std::exchange(other.m_id, 0) }
        , m_contextInitialized     { std::exchange(other.m_contextInitialized, false) }
        , m_windowHandle           { std::exchange(other.m_windowHandle, nullptr) }
        , m_properties             { std::move(other.m_properties) }
        , m_vsync                  { other.m_vsync }
        , m_keyMap                 { std::move(other.m_keyMap) }
        , m_cursorPosCallback      { std::exchange(other.m_cursorPosCallback, nullptr) }
        , m_scrollCallback         { std::exchange(other.m_scrollCallback, nullptr) }
        , m_framebufferSizeCallback{ std::exchange(other.m_framebufferSizeCallback, nullptr) }
        , m_mouseButtonCallback    { std::exchange(other.m_mouseButtonCallback, nullptr) }
        , m_taskQueue              { std::move(other.m_taskQueue) }
        , m_lastFrameTime          { other.m_lastFrameTime }
        , m_deltaTime              { other.m_deltaTime }
        , m_attachedThreadId       { std::exchange(other.m_attachedThreadId, std::nullopt) }
// clang-format on
{
    glfwSetWindowUserPointer(m_windowHandle, this);
}

Window& Window::operator=(Window&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (m_windowHandle != nullptr && m_id != 0) {
        unUse();
        glfwSetWindowUserPointer(m_windowHandle, nullptr);    // remove user pointer
        WindowManager::requestDeleteWindow(m_id);
    }

    m_id                      = std::exchange(other.m_id, 0);
    m_contextInitialized      = std::exchange(other.m_contextInitialized, false);
    m_windowHandle            = std::exchange(other.m_windowHandle, nullptr);
    m_properties              = std::move(other.m_properties);
    m_vsync                   = other.m_vsync;
    m_keyMap                  = std::move(other.m_keyMap);
    m_cursorPosCallback       = std::exchange(other.m_cursorPosCallback, nullptr);
    m_scrollCallback          = std::exchange(other.m_scrollCallback, nullptr);
    m_framebufferSizeCallback = std::exchange(other.m_framebufferSizeCallback, nullptr);
    m_mouseButtonCallback     = std::exchange(other.m_mouseButtonCallback, nullptr);
    m_taskQueue               = std::move(other.m_taskQueue);
    m_lastFrameTime           = other.m_lastFrameTime;
    m_deltaTime               = other.m_deltaTime;
    m_attachedThreadId        = other.m_attachedThreadId;

    glfwSetWindowUserPointer(m_windowHandle, this);

    return *this;
}

Window::~Window()
{
    if (m_windowHandle != nullptr && m_id != 0) {
        unUse();
        glfwSetWindowUserPointer(m_windowHandle, nullptr);    // remove user pointer
        WindowManager::requestDeleteWindow(m_id);
    } else {
        // window is in invalid state (probably moved)
    }
}

void Window::useHere()
{
    if (!m_attachedThreadId.has_value()) {
        // no thread attached, attach to this thread

        m_attachedThreadId = std::this_thread::get_id();

        spdlog::info("(Window) Context ({}) attached (+)", m_id);

        glfwMakeContextCurrent(m_windowHandle);

    } else if (m_attachedThreadId == std::this_thread::get_id()) {

        // same thread, do nothing

    } else {
        // different thread, cannot attach

        spdlog::critical(
            "(Window) Context ({}) already attached to another thread [{:#x}], cannot attach to this thread "
            "[{:#x}].",
            m_id,
            getThreadNum(*m_attachedThreadId),
            getThreadNum(std::this_thread::get_id())
        );

        assert(false && "Context already attached to another thread");
    }
}

void Window::unUse()
{
    glfwMakeContextCurrent(nullptr);
    if (m_attachedThreadId.has_value()) {
        spdlog::info("(Window) Context ({}) detached (-)", m_id);
    }

    m_attachedThreadId.reset();
}

Window& Window::setVsync(bool value)
{
    m_vsync = value;

    // 0 = immediate updates, 1 = update synchronized with vertical retrace
    glfwSwapInterval(static_cast<int>(value));

    return *this;
}

void Window::setWindowSize(int width, int height)
{
    m_properties.m_width  = width;
    m_properties.m_height = height;
}

void Window::updateTitle(const std::string& title)
{
    m_properties.m_title = title;
    WindowManager::enqueueWindowTask(m_id, [this] {
        glfwSetWindowTitle(m_windowHandle, m_properties.m_title.c_str());
    });
}

void Window::run(std::function<void()>&& func)
{
    for (std::scoped_lock lock{ m_windowMutex }; glfwWindowShouldClose(m_windowHandle) == 0;) {
        util::Timer timer{ "Window::run [loop]" };

        updateDeltaTime();
        processInput();
        processQueuedTasks();

        func();
        glfwSwapBuffers(m_windowHandle);
    }
}

void Window::enqueueTask(std::function<void()>&& func)
{
    std::scoped_lock lock{ m_queueMutex };
    m_taskQueue.push(std::move(func));
}

void Window::requestClose()
{
    glfwSetWindowShouldClose(m_windowHandle, 1);
    spdlog::info("(Window) Window ({}) requested to close", m_id);
}

double Window::getDeltaTime() const { return m_deltaTime; }

Window& Window::setCaptureMouse(bool value)
{
    m_captureMouse = value;
    WindowManager::enqueueTask([this] {
        if (m_captureMouse) {
            glfwGetCursorPos(
                m_windowHandle, &m_properties.m_cursorPos.x, &m_properties.m_cursorPos.y
            );    // prevent sudden jump when cursor first captured
            glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    });
    return *this;
}

Window& Window::setCursorPosCallback(CursorPosCallbackFun&& func)
{
    m_cursorPosCallback = std::move(func);
    return *this;
}

Window& Window::setScrollCallback(ScrollCallbackFun&& func)
{
    m_scrollCallback = std::move(func);
    return *this;
}

Window& Window::setFramebuffersizeCallback(FramebufferSizeCallbackFun&& func)
{
    m_framebufferSizeCallback = std::move(func);
    return *this;
}

Window& Window::setMouseButtonCallback(MouseButtonCallback&& func)
{
    m_mouseButtonCallback = std::move(func);
    return *this;
}

Window& Window::addKeyEventHandler(
    KeyEvent                       key,
    KeyModifier                    mods,
    KeyActionType                  action,
    std::function<void(Window&)>&& func
)
{
    m_keyMap.emplace(key, KeyEventHandler{ .mods = mods, .action = action, .handler = func });
    return *this;
}

Window& Window::addKeyEventHandler(
    std::initializer_list<KeyEvent> keys,
    KeyModifier                     mods,
    KeyActionType                   action,
    std::function<void(Window&)>&&  func
)
{
    for (auto key : keys) {
        m_keyMap.emplace(key, KeyEventHandler{ .mods = mods, .action = action, .handler = func });
    }
    return *this;
}

void Window::processInput()
{
    util::Timer timer{ "processInput" };

    // TODO: move this part to 'main thread' (glfwGetKey must be called from main thread [for now it's okay, idk
    // why tho])
    const auto getMods = [win = m_windowHandle] {
        int mods{ 0 };
        mods |= glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) & GLFW_MOD_SHIFT;
        mods |= glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) & GLFW_MOD_SHIFT;
        mods |= glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) & GLFW_MOD_CONTROL;
        mods |= glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) & GLFW_MOD_CONTROL;
        mods |= glfwGetKey(win, GLFW_KEY_LEFT_ALT) & GLFW_MOD_ALT;
        mods |= glfwGetKey(win, GLFW_KEY_RIGHT_ALT) & GLFW_MOD_ALT;
        mods |= glfwGetKey(win, GLFW_KEY_LEFT_SUPER) & GLFW_MOD_SUPER;
        mods |= glfwGetKey(win, GLFW_KEY_RIGHT_SUPER) & GLFW_MOD_SUPER;

        return mods;
    };
    auto mods{ getMods() };

    // continuous key input
    for (auto& [key, handler] : m_keyMap) {
        auto& [hmod, haction, hfun]{ handler };
        if (haction != KeyActionType::CONTINUOUS) {
            continue;
        }
        if (glfwGetKey(m_windowHandle, key) != GLFW_PRESS) {
            continue;
        }

        if (((mods & hmod) != 0) || hmod == 0) {    // modifier match or don't have any modifier
            hfun(*this);
        }
    }
}

void Window::processQueuedTasks()
{
    util::Timer timer{ "processQueuedTasks" };

    decltype(m_taskQueue) taskQueue;
    {
        std::scoped_lock lock{ m_queueMutex };
        taskQueue.swap(m_taskQueue);
    }
    while (!taskQueue.empty()) {
        auto&& func{ taskQueue.front() };
        func();
        taskQueue.pop();
    }
}

void Window::updateDeltaTime()
{
    double currentTime{ glfwGetTime() };
    m_deltaTime     = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;
}
