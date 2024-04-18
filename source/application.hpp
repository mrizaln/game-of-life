#ifndef APPLICATION_HPP_32WF98D4
#define APPLICATION_HPP_32WF98D4

#include "camera.hpp"
#include "double_buffer_atomic.hpp"
#include "game.hpp"
#include "renderer.hpp"
#include "simulation.hpp"
#include "window.hpp"
#include "window_manager.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
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
        : m_simulation{ param.m_gridWidth, param.m_gridHeight, param.m_updateStrategy, param.m_delay }
        , m_window{ std::nullopt }
        , m_renderer{ std::nullopt }
        , m_interp{ -1, -1 }
    {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        WindowManager::createInstance();
        m_window = WindowManager::createWindow(s_defaultTitle.data(), 800, 600);
        if (!m_window.has_value()) {
            spdlog::critical("(Application) Failed to create window");
            throw std::runtime_error{ "Failed to create window" };
        }
        m_window->useHere();
        m_window->setVsync(param.m_vsync);
        m_simulation.read([this](const Grid& grid) { m_renderer.emplace(*m_window, grid); });

        // initialize the grid and the buffer
        m_simulation.write([&](Grid& grid) {
            spdlog::info("(Application) Populating grid...");
            grid.populate(std::clamp(param.m_startDensity, 0.0f, 1.0f));    // clamp, just a sanity check
            auto data = grid.data();
            m_buffer.reset(std::move(data));
            spdlog::info("(Application) Populating grid done.");
        });

        configureControl();
    }

    ~Application()
    {
        m_simulation.stop();
        m_renderer.reset();
        m_window.reset();

        WindowManager::destroyInstance();
        glfwTerminate();
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
        m_window->run([this, timeSum = 0.0]() mutable {
            const auto& front = m_buffer.swapBuffers();
            m_renderer->render(*m_window, front, m_simulation.isPaused());

            const auto fps = 1.0 / m_window->getDeltaTime();
            const auto tps = m_simulation.getTickRate();

            // update title every 1 seconds
            if ((timeSum += m_window->getDeltaTime()) > 1.0) {
                m_window->updateTitle(std::format("{} [{:.2f}FPS|{:.2f}TPS]", s_defaultTitle, fps, tps));
                timeSum = 0.0;
            }

            WindowManager::pollEvents();
        });
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

    Simulation              m_simulation;
    std::optional<Window>   m_window;
    std::optional<Renderer> m_renderer;

    InterpolationHelper m_interp;
    bool                m_previouslyPaused       = false;
    bool                m_previouslyLeftPressed  = false;
    bool                m_previouslyRightPressed = false;

    DoubleBufferAtomic<Grid::Grid_type> m_buffer;

    void configureControl()
    {
        // most of the code is here :) (It took me so long to debug the controls)

        auto& w = *m_window;

        w.setCursorPosCallback([this](Window& window, double xPos, double yPos) {
            const auto& winProp{ window.getProperties() };
            const auto& lastX{ winProp.m_cursorPos.x };
            const auto& lastY{ winProp.m_cursorPos.y };

            const float xOffset = static_cast<float>(xPos - lastX);
            const float yOffset = static_cast<float>(lastY - yPos);

            // instead of normal camera (yaw, pitch, etc.), i'm using this as sliding movement
            // m_camera.processMouseMovement(xOffset, yOffset);
            if (window.isMouseCaptured()) {
                m_renderer->offsetCameraPosition(xOffset, yOffset, 200.0f);
            }

            const auto& camPos  = m_renderer->getCameraPosition();
            const auto& camZoom = m_renderer->getCameraZoom();

            const auto xDelta = (float)winProp.m_width / camZoom;
            const auto yDelta = (float)winProp.m_height / camZoom;

            // current pos
            yPos = winProp.m_height - yPos;                                                  // flip y-axis
            const int x{ static_cast<int>(camPos.x - (xDelta - 2.0f * xPos / camZoom)) };    // col
            const int y{ static_cast<int>(camPos.y - (yDelta - 2.0f * yPos / camZoom)) };    // row

            if (winProp.m_mouseButton.m_left == MouseButton::State::PRESSED) {
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
            } else if (winProp.m_mouseButton.m_right == MouseButton::State::PRESSED) {
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

            if (winProp.m_mouseButton.m_left == MouseButton::State::RELEASED) {
                m_previouslyLeftPressed = false;
            } else if (winProp.m_mouseButton.m_right == MouseButton::State::RELEASED) {
                m_previouslyRightPressed = false;
            }
        });

        w.setScrollCallback([this](Window&, double, double yOffset) {
            m_renderer->offsetCameraZoom(static_cast<float>(yOffset));
        });

        w.setMouseButtonCallback([this](Window&, MouseButton::Button button, MouseButton::State state, Window::KeyModifier) {
            const auto& winProp = m_window->getProperties();
            const auto& camPos  = m_renderer->getCameraPosition();
            const auto& camZoom = m_renderer->getCameraZoom();

            const auto xPos   = winProp.m_cursorPos.x;
            const auto yPos   = winProp.m_height - winProp.m_cursorPos.y;
            const auto xDelta = (float)winProp.m_width / camZoom;
            const auto yDelta = (float)winProp.m_height / camZoom;

            // current pos
            const int x{ static_cast<int>(camPos.x - (xDelta - 2.0f * xPos / camZoom)) };    // col
            const int y{ static_cast<int>(camPos.y - (yDelta - 2.0f * yPos / camZoom)) };    // row

            if (button == MouseButton::Button::LEFT && state == MouseButton::State::PRESSED) {
                m_previouslyPaused = m_simulation.isPaused();
                m_simulation.setPause(true);
                m_simulation.wakeUp();

                m_simulation.write([x, y](Grid& grid) {
                    if (grid.isInBound(x, y)) {
                        grid.get(x, y) = Grid::LIVE_STATE;
                    };
                });
                m_interp = InterpolationHelper{ x, y };
            } else if (button == MouseButton::Button::LEFT && state == MouseButton::State::RELEASED) {
                m_simulation.setPause(m_previouslyPaused);
            }

            if (button == MouseButton::Button::RIGHT && state == MouseButton::State::PRESSED) {
                m_previouslyPaused = m_simulation.isPaused();
                m_simulation.setPause(true);
                m_simulation.wakeUp();

                m_simulation.write([x, y](Grid& grid) {
                    if (grid.isInBound(x, y)) {
                        grid.get(x, y) = Grid::DEAD_STATE;
                    };
                });
                m_interp = InterpolationHelper{ x, y };
            } else if (button == MouseButton::Button::RIGHT && state == MouseButton::State::RELEASED) {
                m_simulation.setPause(m_previouslyPaused);
            }
        });

        using A = Window::KeyActionType;

        // close window
        w.addKeyEventHandler({ GLFW_KEY_Q, GLFW_KEY_ESCAPE }, 0, A::CALLBACK, [](Window& window) {
            window.requestClose();
        });

        // camera movement
        w.addKeyEventHandler({ GLFW_KEY_W, GLFW_KEY_UP }, 0, A::CONTINUOUS, [this](Window& window) {
            m_renderer->processCameraMovement(Camera::UPWARD, (float)window.getDeltaTime());
        });
        w.addKeyEventHandler({ GLFW_KEY_S, GLFW_KEY_DOWN }, 0, A::CONTINUOUS, [this](Window& window) {
            m_renderer->processCameraMovement(Camera::DOWNWARD, (float)window.getDeltaTime());
        });
        w.addKeyEventHandler({ GLFW_KEY_A, GLFW_KEY_LEFT }, 0, A::CONTINUOUS, [this](Window& window) {
            m_renderer->processCameraMovement(Camera::LEFT, (float)window.getDeltaTime());
        });
        w.addKeyEventHandler({ GLFW_KEY_D, GLFW_KEY_RIGHT }, 0, A::CONTINUOUS, [this](Window& window) {
            m_renderer->processCameraMovement(Camera::RIGHT, (float)window.getDeltaTime());
        });

        // camera speed
        w.addKeyEventHandler(GLFW_KEY_I, 0, A::CONTINUOUS, [this](Window&) {
            m_renderer->multiplyCameraSpeed(1.01f);
        });
        w.addKeyEventHandler(GLFW_KEY_K, 0, A::CONTINUOUS, [this](Window&) {
            m_renderer->multiplyCameraSpeed(1.0f / 1.01f);
        });

        // simulation delay
        w.addKeyEventHandler(GLFW_KEY_L, 0, A::CONTINUOUS, [this](Window&) {
            auto newDelay = std::size_t((float)m_simulation.getDelay() * 1.01f);
            m_simulation.setDelay(newDelay);
        });
        w.addKeyEventHandler(GLFW_KEY_J, 0, A::CONTINUOUS, [this](Window&) {
            auto newDelay = std::size_t((float)m_simulation.getDelay() / 1.01f);
            if (newDelay <= 5) {
                m_simulation.setDelay(5);    // minimum of 5 ms
            } else {
                m_simulation.setDelay(newDelay);
            }
        });

        // toggle mouse capture
        w.addKeyEventHandler(GLFW_KEY_C, GLFW_MOD_ALT, A::CALLBACK, [](Window& window) {
            window.setCaptureMouse(!window.isMouseCaptured());
        });

        // toggle vsync
        w.addKeyEventHandler(GLFW_KEY_V, GLFW_MOD_ALT, A::CALLBACK, [](Window& window) {
            window.setVsync(!window.isVsyncEnabled());
        });

        // reset camera
        w.addKeyEventHandler(GLFW_KEY_BACKSPACE, 0, A::CALLBACK, [this](Window&) {
            m_renderer->resetCamera(false);
        });

        // toggle pause
        w.addKeyEventHandler(GLFW_KEY_SPACE, 0, A::CALLBACK, [this](Window&) {
            m_simulation.togglePause();
        });

        // force update (if paused, do nothing instead)
        w.addKeyEventHandler(GLFW_KEY_U, 0, A::CALLBACK, [this](Window&) {
            m_simulation.forceUpdate();
        });

        // fit grid to window
        w.addKeyEventHandler(GLFW_KEY_F, 0, A::CALLBACK, [this](Window&) {
            m_renderer->fitToWindow();
        });

        // cycle through the grid modes
        w.addKeyEventHandler(GLFW_KEY_G, 0, A::CALLBACK, [this](Window&) {
            m_renderer->cycleGridMode();
        });

        // reset grid -> set all cell to dead
        w.addKeyEventHandler(GLFW_KEY_R, 0, A::CALLBACK, [this](Window&) {
            m_simulation.write(&Grid::clear);
        });

        // populate grid with new random cells
        w.addKeyEventHandler(GLFW_KEY_P, 0, A::CALLBACK, [this](Window&) {
            m_simulation.write([](Grid& grid) {
                spdlog::info("(Application) Populating grid...");
                auto density = Grid::getRandomProbability() * 0.6f + 0.2f;
                grid.populate(density);
                spdlog::info("(Application) Populating grid done.");
            });
        });

        // wireframe mode
        w.addKeyEventHandler(GLFW_KEY_TAB, 0, A::CALLBACK, [](Window&) {
            GLint polygonMode[2];
            glGetIntegerv(GL_POLYGON_MODE, polygonMode);

            bool isWireframe = polygonMode[0] == GL_LINE;
            if (isWireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }
        });
    }
};

#endif /* end of include guard: APPLICATION_HPP_32WF98D4 */
