#include "window_manager.hpp"

#include "window.hpp"

#include <spdlog/spdlog.h>

#include <format>
#include <stdexcept>

namespace
{
    std::size_t getThreadNum(const std::thread::id& threadId)
    {
        auto hash{ std::hash<std::thread::id>{} };
        return hash(threadId);
    }

}

class WindowManager::ErrorUninitialized : public std::runtime_error
{
public:
    ErrorUninitialized()
        : std::runtime_error{ "(WindowManager) Instance hasn't created yet!" }
    {
    }
};

class WindowManager::ErrorAccessFromWrongThread : public std::runtime_error
{
public:
    ErrorAccessFromWrongThread(std::thread::id initThreadId)
        : std::runtime_error{
            std::format(
                "(WindowManager) Instance accessed from different thread from initialization! "
                "[at init: {} | current: {}]",
                getThreadNum(initThreadId),
                getThreadNum(std::this_thread::get_id())
            ),
        }
    {
    }
};

WindowManager::WindowManager(std::thread::id threadId)
    : m_attachedThreadId{ threadId }
{
    // yeah, that's it.
    // attached thread id will not be changed for the lifetime of this class instance.
}

void WindowManager::createInstance()
{
    if (!s_instance) {
        s_instance.reset(new WindowManager{ std::this_thread::get_id() });
    }
}

void WindowManager::destroyInstance() { s_instance.reset(nullptr); }

std::optional<Window> WindowManager::createWindow(std::string title, int width, int height)
{
    validateAccess(true);

    UniqueGLFWwindow glfwWindow{ glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr) };
    if (!glfwWindow) {
        spdlog::critical("(WindowManager) Window creation failed");
        return {};
    }

    auto [id, handle] = s_instance.writeValue([&](WindowManager& wm) {
        auto  id     = ++wm.m_windowCount;
        auto* handle = glfwWindow.get();
        wm.m_windows.emplace(id, std::move(glfwWindow));
        return std::make_pair(id, handle);
    });

    spdlog::info("(WindowManager) Window ({}) created", id);

    return Window{
        id,
        handle,
        {
            .m_title       = title,
            .m_width       = width,
            .m_height      = height,
            .m_cursorPos   = {},
            .m_mouseButton = {},
        },
    };
}

void WindowManager::requestDeleteWindow(std::size_t id)
{
    validateAccess(false);
    s_instance.writeValue([id](auto& wm) {
        // accept request only for available windows
        if (wm.m_windows.contains(id)) {
            wm.m_windowDeleteQueue.push(id);
        }
    });
}

void WindowManager::pollEvents(std::optional<std::chrono::milliseconds> msPollRate)
{
    validateAccess(true);

    if (msPollRate) {
        auto sleepUntilTime{ std::chrono::steady_clock::now() + *msPollRate };

        glfwPollEvents();
        s_instance.writeValue(&WindowManager::checkTasks);

        if (sleepUntilTime > std::chrono::steady_clock::now()) {
            std::this_thread::sleep_until(sleepUntilTime);
        }
    } else {
        glfwPollEvents();
        s_instance.writeValue(&WindowManager::checkTasks);
    }
}

void WindowManager::waitEvents()
{
    validateAccess(true);
    glfwWaitEvents();
    s_instance.writeValue(&WindowManager::checkTasks);
}

bool WindowManager::hasWindowOpened()
{
    validateAccess(false);
    return s_instance.readValue([](const WindowManager& wm) { return !wm.m_windows.empty(); });
}

void WindowManager::enqueueWindowTask(std::size_t windowId, std::function<void()>&& task)
{
    validateAccess(false);
    s_instance.writeValue([&](WindowManager& wm) { wm.m_windowTaskQueue.emplace(windowId, std::move(task)); });
}

void WindowManager::enqueueTask(std::function<void()>&& task)
{
    validateAccess(false);
    s_instance.writeValue([&](WindowManager& wm) { wm.m_taskQueue.emplace(std::move(task)); });
}

std::thread::id WindowManager::getAttachedThreadId()
{
    validateAccess(false);
    return s_instance.getValue(&WindowManager::m_attachedThreadId);
}

void WindowManager::validateAccess(bool checkThread)
{
    if (!s_instance) {
        throw ErrorUninitialized{};
    };

    auto attachedThreadId{ s_instance.readValue([](const WindowManager& wm) { return wm.m_attachedThreadId; }) };
    if (checkThread && attachedThreadId != std::this_thread::get_id()) {
        throw ErrorAccessFromWrongThread{ attachedThreadId };
    }
}

void WindowManager::checkTasks()
{
    // window deletion
    while (!m_windowDeleteQueue.empty()) {
        std::size_t windowId{ m_windowDeleteQueue.front() };
        m_windowDeleteQueue.pop();

        if (auto found{ m_windows.find(windowId) }; found != m_windows.end()) {
            const auto& [id, window]{ *found };
            spdlog::info("(WindowManager) Window ({}) deleted", id);
            m_windows.erase(found);
        }
    }

    // window task requests
    while (!m_windowTaskQueue.empty()) {
        auto [id, fun]{ std::move(m_windowTaskQueue.front()) };
        m_windowTaskQueue.pop();
        if (m_windows.contains(id)) {
            fun();
        } else {
            spdlog::warn("(WindowManager) Task for window ({}) failed: window has destroyed", id);
        }
    }

    // general task request
    while (!m_taskQueue.empty()) {
        auto fun{ std::move(m_taskQueue.front()) };
        m_taskQueue.pop();
        fun();
    }
}
