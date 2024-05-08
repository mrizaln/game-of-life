#ifndef APPLICATION_HPP_32WF98D4
#define APPLICATION_HPP_32WF98D4

#include "camera.hpp"
#include "double_buffer_atomic.hpp"
#include "game.hpp"
#include "renderer.hpp"
#include "simulation.hpp"

#include <glfw_cpp/glfw_cpp.hpp>
#include <glbinding/glbinding.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <concepts>
#include <cmath>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

class Application
{
public:
    struct InitParam
    {
        int                  m_windowWidth;
        int                  m_windowHeight;
        int                  m_gridWidth;
        int                  m_gridHeight;
        float                m_startDensity;
        std::size_t          m_delay;    // in milliseconds
        bool                 m_vsync;
        Grid::UpdateStrategy m_updateStrategy;
    };

    Application()                              = delete;
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    Application(InitParam param)
        : m_glfw{ glfwInit() }
        , m_wm{ m_glfw->createWindowManager() }
        , m_window{ m_wm.createWindow({}, s_defaultTitle.data(), 800, 600) }
        , m_simulation{ param.m_gridWidth, param.m_gridHeight, param.m_updateStrategy, param.m_delay }
        , m_renderer{ m_window, *m_simulation.read([](auto& grid) { return &grid; }) }    // kinda a hack, but eh
        , m_interp{ -1, -1 }
    {
        m_window.setVsync(param.m_vsync);

        // initialize the grid and the buffer
        m_simulation.write([&](Grid& grid) {
            spdlog::info("(Application) Populating grid...");
            grid.populate(std::clamp(param.m_startDensity, 0.0f, 1.0f));    // clamp, just a sanity check
            auto data = grid.data();
            m_buffer.reset(std::move(data));
            spdlog::info("(Application) Populating grid done.");
        });
    }

    static glfw_cpp::Instance::Handle glfwInit()
    {
        glfw_cpp::Api::OpenGL api = {
            .m_major   = 3,
            .m_minor   = 3,
            .m_profile = glfw_cpp::Api::OpenGL::Profile::CORE,
            .m_loader  = [](auto handle, auto proc) {
                glbinding::initialize((glbinding::ContextHandle)handle, proc);
            }
        };
        return glfw_cpp::init(std::move(api), glfwLogCallback);
    }

    static void glfwLogCallback(glfw_cpp::Instance::LogLevel level, std::string msg)
    {
        // clang-format off
        auto spdloglevel = [&] { switch (level) {
            using L = glfw_cpp::Instance::LogLevel;
            case L::DEBUG:                          return spdlog::level::debug;
            case L::INFO:                           return spdlog::level::info;
            case L::WARNING:                        return spdlog::level::warn;
            case L::ERROR:                          return spdlog::level::err;
            case L::CRITICAL:                       return spdlog::level::critical;
            case L::NONE:       [[fallthrough]];
            default:            [[unlikely]]        return spdlog::level::info;
        } }();
        // clang-format on
        spdlog::log(spdloglevel, "(GLFW) {}", msg);
    }

    void run()
    {
        // launch the simulation
        m_simulation.launch([this](Grid& grid) {
            m_buffer.updateBuffer([&](auto& data) {
                data = grid.data();
            });
        });

        // render the grid
        m_window.run([this, timeSum = 0.0](auto&& events) mutable {
            handleEvents(std::move(events));

            const auto& front = m_buffer.swapBuffers();
            m_renderer.render(m_window, front, m_simulation.isPaused());

            const auto fps = 1.0 / m_window.deltaTime();
            const auto tps = m_simulation.getTickRate();

            // update title every 1 seconds
            if ((timeSum += m_window.deltaTime()) > 1.0) {
                m_window.updateTitle(std::format("{} [{:.2f}FPS|{:.2f}TPS]", s_defaultTitle, fps, tps));
                timeSum = 0.0;
            }

            m_wm.pollEvents();
        });

        // stop the simulation thread if window is exited
        m_simulation.stop();
    }

private:
    class InterpolationHelper
    {
    public:
        InterpolationHelper(int x, int y)
            : m_xLast{ x }
            , m_yLast{ y }
        {
        }

        void interpolate(int x, int y, std::invocable<int, int> auto&& fn)
        {
            const int xDelta = x - m_xLast;
            const int yDelta = y - m_yLast;

            if (xDelta == 0 && yDelta == 0) {
                return;
            }

            const auto grad{ (float)yDelta / (float)xDelta };
            const auto grad_inv{ (float)xDelta / (float)yDelta };

            int x1 = std::exchange(m_xLast, x);
            int y1 = std::exchange(m_yLast, y);
            int x2 = x;
            int y2 = y;

            if (x1 > x2) {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }

            // TODO: use a more sophisticated algorithm
            if (std::abs(grad) <= 1.0f) {
                if (x1 > x2) {
                    std::swap(x1, x2);
                    std::swap(y1, y2);
                }
                for (int col{ x1 }; col <= x2; ++col) {
                    const auto row{ static_cast<int>(grad * (col - x1) + y1) };
                    fn(col, row);
                }
            } else {
                if (y1 > y2) {
                    std::swap(x1, x2);
                    std::swap(y1, y2);
                }
                for (int row{ y1 }; row <= y2; ++row) {
                    const auto col{ static_cast<int>(grad_inv * (row - y1) + x1) };
                    fn(col, row);
                }
            }
        }

    private:
        int m_xLast;
        int m_yLast;
    };

    static constexpr std::string_view s_defaultTitle = "Game of Life";

    glfw_cpp::Instance::Handle m_glfw;
    glfw_cpp::WindowManager    m_wm;
    glfw_cpp::Window           m_window;

    Simulation m_simulation;
    Renderer   m_renderer;

    InterpolationHelper m_interp;
    bool                m_previouslyPaused       = false;
    bool                m_previouslyLeftPressed  = false;
    bool                m_previouslyRightPressed = false;

    std::pair<double, double> m_lastCursor = {};

    DoubleBufferAtomic<Grid::Grid_type> m_buffer;

    void handleEvents(std::deque<glfw_cpp::Event>&& events)
    {
        for (auto& event : events) {
            event.visit([this](auto& e) {
                using EV = std::decay_t<decltype(e)>;
                using E  = glfw_cpp::Event;
                if constexpr (std::same_as<EV, E::CursorMoved>) {
                    handleCursorMovedEvent(std::move(e));
                } else if constexpr (std::same_as<EV, E::ButtonPressed>) {
                    handleButtonPressedEvent(std::move(e));
                } else if constexpr (std::same_as<EV, E::KeyPressed>) {
                    handleKeyPressedEvent(std::move(e));
                } else if constexpr (std::same_as<EV, E::Scrolled>) {
                    m_renderer.offsetCameraZoom(static_cast<float>(e.m_yOffset));
                } else {
                    /* do nothing */
                }
            });
        }

        // continuous key press
        // --------------------
        const auto& keys = m_window.properties().m_keyState;
        using K          = glfw_cpp::KeyCode;

        // camera movement
        if (keys.anyPressed({ K::W, K::UP })) {
            m_renderer.processCameraMovement(Camera::UPWARD, (float)m_window.deltaTime());
        }
        if (keys.anyPressed({ K::S, K::DOWN })) {
            m_renderer.processCameraMovement(Camera::DOWNWARD, (float)m_window.deltaTime());
        }
        if (keys.anyPressed({ K::A, K::LEFT })) {
            m_renderer.processCameraMovement(Camera::LEFT, (float)m_window.deltaTime());
        }
        if (keys.anyPressed({ K::D, K::RIGHT })) {
            m_renderer.processCameraMovement(Camera::RIGHT, (float)m_window.deltaTime());
        }

        // camera speed
        if (keys.isPressed(K::I)) {
            m_renderer.multiplyCameraSpeed(1.01f);
        } else if (keys.isPressed(K::K)) {
            m_renderer.multiplyCameraSpeed(1.0f / 1.01f);
        }

        // simulation delay
        if (keys.isPressed(K::L)) {
            auto newDelay = std::size_t((float)m_simulation.getDelay() * 1.01f);
            m_simulation.setDelay(newDelay);
        } else if (keys.isPressed(K::J)) {
            auto newDelay = std::size_t((float)m_simulation.getDelay() / 1.01f);
            if (newDelay <= 5) {
                m_simulation.setDelay(5);    // minimum of 5 ms
            } else {
                m_simulation.setDelay(newDelay);
            }
        }
        // --------------------
    }

    void handleCursorMovedEvent(glfw_cpp::Event::CursorMoved&& event)
    {
        auto [xPos, yPos]          = event;
        const auto& [lastX, lastY] = std::exchange(m_lastCursor, { xPos, yPos });

        const auto& [_, __, dim, ___, keys, buttons] = m_window.properties();

        const float xOffset = static_cast<float>(xPos - lastX);
        const float yOffset = static_cast<float>(lastY - yPos);

        // instead of normal camera (yaw, pitch, etc.), i'm using this as sliding movement
        // m_camera.processMouseMovement(xOffset, yOffset);
        if (m_window.isMouseCaptured()) {
            m_renderer.offsetCameraPosition(xOffset, yOffset, 200.0f);
        }

        const auto& camPos  = m_renderer.getCameraPosition();
        const auto& camZoom = m_renderer.getCameraZoom();

        const auto xDelta = (float)dim.m_width / camZoom;
        const auto yDelta = (float)dim.m_height / camZoom;

        // current pos
        yPos = dim.m_height - yPos;                                                      // flip y-axis
        const int x{ static_cast<int>(camPos.x - (xDelta - 2.0f * xPos / camZoom)) };    // col
        const int y{ static_cast<int>(camPos.y - (yDelta - 2.0f * yPos / camZoom)) };    // row

        using M = glfw_cpp::MouseButton;
        if (buttons.isPressed(M::LEFT)) {
            if (m_previouslyLeftPressed) {
                m_simulation.write([this, x, y](Grid& grid) {
                    m_interp.interpolate(x, y, [&](int col, int row) {
                        if (grid.isInBound(col, row)) {
                            grid.get(col, row) = Grid::LIVE_STATE;
                        };
                    });
                });
            }
            m_previouslyLeftPressed = true;
        } else if (buttons.isPressed(M::RIGHT)) {
            if (m_previouslyRightPressed) {
                m_simulation.write([this, x, y](Grid& grid) {
                    m_interp.interpolate(x, y, [&](int col, int row) {
                        if (grid.isInBound(col, row)) {
                            grid.get(col, row) = Grid::DEAD_STATE;
                        };
                    });
                });
            }
            m_previouslyRightPressed = true;
        }

        if (!buttons.isPressed(M::LEFT)) {
            m_previouslyLeftPressed = false;
        } else if (!buttons.isPressed(M::RIGHT)) {
            m_previouslyRightPressed = false;
        }
    }

    void handleButtonPressedEvent(glfw_cpp::Event::ButtonPressed&& event)
    {
        const auto [button, state, mods]            = event;
        const auto& [_, __, dim, cursor, ___, ____] = m_window.properties();

        const auto& camPos  = m_renderer.getCameraPosition();
        const auto& camZoom = m_renderer.getCameraZoom();

        const auto xPos   = cursor.m_x;
        const auto yPos   = dim.m_height - cursor.m_y;
        const auto xDelta = (float)dim.m_width / camZoom;
        const auto yDelta = (float)dim.m_height / camZoom;

        // current pos
        const int x{ static_cast<int>(camPos.x - (xDelta - 2.0f * xPos / camZoom)) };    // col
        const int y{ static_cast<int>(camPos.y - (yDelta - 2.0f * yPos / camZoom)) };    // row

        using M = glfw_cpp::MouseButton;
        using S = glfw_cpp::MouseButtonState;

        if (button == M::LEFT && state == S::PRESS) {
            m_previouslyPaused = m_simulation.isPaused();
            m_simulation.setPause(true);
            m_simulation.wakeUp();

            m_simulation.write([x, y](Grid& grid) {
                if (grid.isInBound(x, y)) {
                    grid.get(x, y) = Grid::LIVE_STATE;
                };
            });
            m_interp = InterpolationHelper{ x, y };
        } else if (button == M::LEFT && state == S::RELEASE) {
            m_simulation.setPause(m_previouslyPaused);
        }

        if (button == M::RIGHT && state == S::PRESS) {
            m_previouslyPaused = m_simulation.isPaused();
            m_simulation.setPause(true);
            m_simulation.wakeUp();

            m_simulation.write([x, y](Grid& grid) {
                if (grid.isInBound(x, y)) {
                    grid.get(x, y) = Grid::DEAD_STATE;
                };
            });
            m_interp = InterpolationHelper{ x, y };
        } else if (button == M::RIGHT && state == S::RELEASE) {
            m_simulation.setPause(m_previouslyPaused);
        }
    }

    void handleKeyPressedEvent(glfw_cpp::Event::KeyPressed&& event)
    {
        const auto [key, _, state, mods] = event;

        if (state != glfw_cpp::KeyState::PRESS) {
            return;
        }

        using K = glfw_cpp::KeyCode;
        using M = glfw_cpp::ModifierKey;

        if (mods.test(M::ALT)) {
            switch (key) {
            case K::C:
                m_window.setCaptureMouse(!m_window.isMouseCaptured());
                break;
            case K::V:
                m_window.setVsync(!m_window.isVsyncEnabled());
                break;
            default:
                [[unlikely]];
            }
        } else {
            switch (key) {
            case K::Q:
                m_window.requestClose();
                break;
            case K::U:
                m_simulation.forceUpdate();
                break;
            case K::F:
                m_renderer.fitToWindow();
                break;
            case K::G:
                m_renderer.cycleGridMode();
                break;
            case K::R:
                m_simulation.write(&Grid::clear);
                break;
            case K::P:
                m_simulation.write([](Grid& grid) {
                    spdlog::info("(Application) Populating grid...");
                    auto density = Grid::getRandomProbability() * 0.6f + 0.2f;
                    grid.populate(density);
                    spdlog::info("(Application) Populating grid done.");
                });
                break;
            case K::SPACE:
                m_simulation.togglePause();
                break;
            case K::BACKSPACE:
                m_renderer.resetCamera(false);
                break;
            case K::TAB:
            {
                gl::GLint polygonMode[2];
                gl::glGetIntegerv(gl::GL_POLYGON_MODE, polygonMode);

                bool isWireframe = (gl::GLenum)polygonMode[0] == gl::GL_LINE;
                if (isWireframe) {
                    gl::glPolygonMode(gl::GL_FRONT_AND_BACK, gl::GL_FILL);
                } else {
                    gl::glPolygonMode(gl::GL_FRONT_AND_BACK, gl::GL_LINE);
                }
            } break;
            default:
                [[unlikely]];
            }
        }
    }
};

#endif /* end of include guard: APPLICATION_HPP_32WF98D4 */
