#ifndef RENDERER_HPP_23RYFJ3HF3
#define RENDERER_HPP_23RYFJ3HF3

#include "camera.hpp"
#include "game.hpp"
#include "grid_tile.hpp"
#include "tile.hpp"
#include "window.hpp"
#include "window_manager.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>
#include <utility>

class Renderer
{
public:
    enum class GridMode
    {
        OFF,
        ON,
        AUTO,

        numOfGridModes,
    };

    struct Simulation
    {
        GridMode          m_gridMode      = GridMode::AUTO;
        float             m_timeSum       = 0.0f;
        float             m_delay         = 1e-5f;
        std::atomic<bool> m_pause         = false;
        std::atomic<bool> m_inhibitUpdate = false;
    };

    struct Border
    {
        int m_xStart = 0;
        int m_xEnd   = 0;
        int m_yStart = 0;
        int m_yEnd   = 0;

        bool operator==(const Border& other) const
        {
            return m_xStart == other.m_xStart
                   && m_xEnd == other.m_xEnd
                   && m_yStart == other.m_yStart
                   && m_yEnd == other.m_yEnd;
        }

        bool operator!=(const Border& other) const { return !(*this == other); }
    };

    enum class WorkerState
    {
        IDLE,
        UPDATE_STATE,
        UPDATE_INDICES
    };

    struct Worker
    {
        std::optional<std::future<void>> m_future;
        std::atomic<WorkerState>         m_state    = WorkerState::IDLE;
        std::atomic<bool>                m_doUpdate = false;
    };

    Renderer()                           = delete;
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    Renderer(Window&& window, Grid& grid, float delay)
        : m_grid{ grid }
        , m_window{ std::move(window) }
        , m_borderTile{
            1.0f,
            "./resources/shaders/shader.vs",
            "./resources/shaders/shader.fs",
            "./resources/textures/grid.png",
            GL_LINEAR,
            GL_LINEAR,
            GL_REPEAT,
            glm::vec3{ (float)grid.getLength() / 2.0f, (float)grid.getWidth() / 2.0f, 0.0f },    // pos
            glm::vec3{ 1.0f, 1.0f, 1.0f },                                                       // color
            glm::vec3{ grid.getLength(), grid.getWidth(), 0.0f },                                // scale
        }
        , m_gridTile{
            grid.getLength(),                                                                    //
            grid.getWidth(),                                                                     //
            "./resources/shaders/gridShader.vert",                                               //
            "./resources/shaders/gridShader.frag",                                               //
            "./resources/textures/cell.png",                                                     //
            GL_LINEAR,                                                                           //
            GL_LINEAR,                                                                           //
            GL_REPEAT,                                                                           //
            glm::vec3{ (float)grid.getLength() / 2.0f, (float)grid.getWidth() / 2.0f, 0.0f },    // pos
            glm::vec3{ 1.0f, 1.0f, 1.0f },                                                       // color
            glm::vec3{ grid.getLength(), grid.getWidth(), 0.0f },                                // scale
        }
        , m_simulation{ .m_delay = std::max(delay, 1e-5f) }
        , m_camera{}
        , m_currentBorder{}
        , m_previousBorder{}
        , m_worker{}
    {
        m_borderTile.getPlane().multiplyTexCoords((float)grid.getLength(), (float)grid.getWidth());
        m_camera.speed = 100.0f;
    }

    void run()
    {
        configureControl();
        resetCamera(true);

        m_window.run([this] {
            if (!m_simulation.m_pause) {
                glClearColor(0.1f, 0.1f, 0.11f, 1.0f);
            } else {
                glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // orthogonal frustum
            // clang-format off
            auto winProp = m_window.getProperties();
            glViewport(0, 0, winProp.m_width, winProp.m_height);

            const float left  { -winProp.m_width  / static_cast<float>(m_camera.zoom) };
            const float right {  winProp.m_width  / static_cast<float>(m_camera.zoom) };
            const float bottom{ -winProp.m_height / static_cast<float>(m_camera.zoom) };
            const float top   {  winProp.m_height / static_cast<float>(m_camera.zoom) };
            const float near  { -10.0f };
            const float far   {  10.0f };
            // clang-format on

            // projection matrix
            auto projMat{ glm::ortho(left, right, bottom, top, near, far) };

            // view matrix
            auto viewMat{ m_camera.getViewMatrix() };

            const auto& xPos{ m_camera.position.x };
            const auto& yPos{ m_camera.position.y };
            const auto  width  = (float)m_grid.getLength();
            const auto  height = (float)m_grid.getWidth();

            constexpr float offset{ 1.5f };

            // culling (update currentBorder)
            // clang-format off
            const int rowLeftBorder  { static_cast<int>( xPos + left       - offset > 0      ? xPos + left   - offset : 0) };
            const int rowRightBorder { static_cast<int>( xPos + right + 1  + offset < width  ? xPos + right  + offset : width) };
            const int colTopBorder   { static_cast<int>( yPos + top        + offset < height ? yPos + top    + offset : height) };
            const int colBottomBorder{ static_cast<int>( yPos + bottom + 1 - offset > 0      ? yPos + bottom - offset : 0) };
            // clang-format on

            m_currentBorder = { rowLeftBorder, rowRightBorder, colBottomBorder, colTopBorder };
            updateStates();
            m_previousBorder = m_currentBorder;

            // draw
            drawBorder(projMat, viewMat);
            drawGrid(projMat, viewMat);

            // poll events
            WindowManager::pollEvents();
        });
    }

private:
    Grid&      m_grid;
    Window     m_window;
    Tile       m_borderTile;
    GridTile   m_gridTile;
    Simulation m_simulation;
    Camera     m_camera;
    Border     m_currentBorder;
    Border     m_previousBorder;
    Worker     m_worker;

    bool updateStates()
    {
        if (m_simulation.m_timeSum >= m_simulation.m_delay && m_worker.m_state == WorkerState::IDLE) {
            m_worker.m_doUpdate    = true;
            m_simulation.m_timeSum = 0.0f;
        } else {
            m_simulation.m_timeSum += (float)m_window.getDeltaTime();
        }

        if (!m_worker.m_future.has_value() || m_worker.m_future->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            m_worker.m_future = std::async(std::launch::async, [this, curr = m_currentBorder, prev = m_previousBorder] {
                util::Timer timer{ "workerThread" };

                if (curr != prev || !m_simulation.m_inhibitUpdate || !m_simulation.m_pause) {
                    m_worker.m_state             = WorkerState::UPDATE_INDICES;
                    const auto& [x1, x2, y1, y2] = curr;
                    m_gridTile.getPlane().customizeIndices(m_grid.data(), x1, x2, y1, y2);
                }

                if (!m_simulation.m_pause && m_worker.m_doUpdate) {
                    m_worker.m_state = WorkerState::UPDATE_STATE;
                    m_grid.updateState();
                    m_worker.m_doUpdate = false;
                }

                m_worker.m_state = WorkerState::IDLE;
            });

            return false;
        }

        return true;
    }

    void drawBorder(const glm::mat4& projMat, const glm::mat4& viewMat)
    {
        bool shouldDraw{};

        switch (m_simulation.m_gridMode) {
        case GridMode::ON:
        {
            shouldDraw = true;
            break;
        }
        case GridMode::AUTO:
        {
            const float   xDelta{ m_window.getProperties().m_width / m_camera.zoom };    // (number of cells per width of screen)/2
            constexpr int minNumberOfCellsPerScreenHeight{ 100 };                        // change accordingly
            if (xDelta <= minNumberOfCellsPerScreenHeight / 2.0f) {
                shouldDraw = true;
            }
            break;
        }
        default:
            shouldDraw = false;
        }

        if (!shouldDraw) {
            return;
        }

        // use shader
        m_borderTile.m_shader.use();

        // upload view and projection matrix
        m_borderTile.m_shader.setMat4("view", viewMat);
        m_borderTile.m_shader.setMat4("projection", projMat);

        // model matrix
        glm::mat4 model{ 1.0f };
        model = glm::translate(model, m_borderTile.m_position);
        model = glm::scale(model, m_borderTile.m_scale);
        m_borderTile.m_shader.setMat4("model", model);

        // change color if simulation::pause
        if (!m_simulation.m_pause) {
            m_borderTile.m_color = { 1.0f, 1.0f, 1.0f };
        } else {
            m_borderTile.m_color = { 0.7f, 1.0f, 0.7f };
        }

        // draw
        m_borderTile.draw();
    }

    void drawGrid(const glm::mat4& projMat, const glm::mat4& viewMat)
    {
        m_gridTile.m_shader.use();
        m_gridTile.m_shader.setMat4("view", viewMat);
        m_gridTile.m_shader.setMat4("projection", projMat);

        glm::mat4 model{ 1.0f };
        model = glm::translate(model, m_gridTile.m_position);
        model = glm::scale(model, m_gridTile.m_scale);

        m_gridTile.m_shader.setMat4("model", model);

        m_gridTile.draw();
    }

    // i'm not sure what is this for
    void updateCamera()
    {
        float delay{ m_simulation.m_delay };

        // if lag
        if (delay < 1e-5f) {
            delay = 1e-5f;
        }

        if (auto delta = (float)m_window.getDeltaTime(); delay < delta) {
            delay = delta;
        }
    }

    void resetCamera(bool resetZoom)
    {
        auto& [numOfColumns, numOfRows]{ m_grid.getDimension() };

        // center camera
        m_camera.position.x = (float)numOfColumns / 2.0f;
        m_camera.position.y = (float)numOfRows / 2.0f;

        constexpr int maxCol{ 75 };

        if (resetZoom) {
            auto width    = (float)m_window.getProperties().m_width;
            m_camera.zoom = std::max({
                2.0f * width / float(numOfColumns),    // all grid visible
                4.0f * width / float(numOfColumns),    // half column
                // 4/1.1f * height/float(numOfRows),     // half row

                2.0f * width / float(maxCol)    // show only number of maxCol
            });
        }
    }

    void configureControl()
    {
        m_window.setCursorPosCallback([this](Window& window, double xPos, double yPos) {
            if (!window.isMouseCaptured()) {
                return;
            }

            auto& winProp{ window.getProperties() };
            auto& lastX{ winProp.m_cursorPos.x };
            auto& lastY{ winProp.m_cursorPos.y };

            float xOffset = static_cast<float>(xPos - lastX);
            float yOffset = static_cast<float>(lastY - yPos);

            // instead of normal camera (yaw, pitch, etc.), i'm using this as sliding movement
            // m_camera.processMouseMovement(xOffset, yOffset);
            m_camera.position.x += xOffset * m_camera.speed / 200.0f;
            m_camera.position.y += yOffset * m_camera.speed / 200.0f;

            m_camera.processMouseMovement(xOffset, yOffset);
        });

        m_window.setScrollCallback([this](Window&, double, double yOffset) {
            m_camera.processMouseScroll((float)yOffset, Camera::ZOOM);
        });

        m_window.setMouseButtonCallback([](Window& window, MouseButton::Button button, MouseButton::State state, Window::KeyModifier mods) {
            if (button == MouseButton::Button::LEFT && state == MouseButton::State::PRESSED) {
                // TODO: implement
            } else if (button == MouseButton::Button::LEFT && state == MouseButton::State::RELEASED) {
                // TODO: implement
            }

            if (button == MouseButton::Button::RIGHT && state == MouseButton::State::PRESSED) {
                // TODO: implement
            } else if (button == MouseButton::Button::RIGHT && state == MouseButton::State::RELEASED) {
                // TODO: implement
            }
        });

        using A = Window::KeyActionType;
        m_window.addKeyEventHandler({ GLFW_KEY_Q, GLFW_KEY_ESCAPE }, 0, A::CALLBACK, [](Window& window) {
            window.requestClose();
        });

        // camera movement
        m_window.addKeyEventHandler({ GLFW_KEY_W, GLFW_KEY_UP }, 0, A::CONTINUOUS, [this](Window&) {
            processCameraMovement(Camera::UPWARD);
        });
        m_window.addKeyEventHandler({ GLFW_KEY_S, GLFW_KEY_DOWN }, 0, A::CONTINUOUS, [this](Window&) {
            processCameraMovement(Camera::DOWNWARD);
        });
        m_window.addKeyEventHandler({ GLFW_KEY_A, GLFW_KEY_LEFT }, 0, A::CONTINUOUS, [this](Window&) {
            processCameraMovement(Camera::LEFT);
        });
        m_window.addKeyEventHandler({ GLFW_KEY_D, GLFW_KEY_RIGHT }, 0, A::CONTINUOUS, [this](Window&) {
            processCameraMovement(Camera::RIGHT);
        });

        // camera speed
        m_window.addKeyEventHandler(GLFW_KEY_I, 0, A::CONTINUOUS, [this](Window&) {
            m_camera.speed *= 1.01f;
        });
        m_window.addKeyEventHandler(GLFW_KEY_K, 0, A::CONTINUOUS, [this](Window&) {
            m_camera.speed /= 1.01f;
        });

        // simulation delay
        m_window.addKeyEventHandler(GLFW_KEY_L, 0, A::CONTINUOUS, [this](Window&) {
            m_simulation.m_delay *= 1.01f;
        });
        m_window.addKeyEventHandler(GLFW_KEY_J, 0, A::CONTINUOUS, [this](Window&) {
            m_simulation.m_delay /= 1.01f;
            if (m_simulation.m_delay < 1e-5f) {
                m_simulation.m_delay = 1e-5f;
            }
        });
    }

    void processCameraMovement(Camera::CameraMovement movement)
    {
        const auto& winProp = m_window.getProperties();

        float xPos{ m_camera.position.x };
        float yPos{ m_camera.position.y };
        float xDelta{ winProp.m_width / m_camera.zoom };
        float yDelta{ winProp.m_height / m_camera.zoom };
        auto& [numOfColumns, numOfRows]{ m_grid.getDimension() };    // potential data race

        auto shouldMove = [&] {
            // clang-format off
            switch (movement) {
            case Camera::RIGHT:     return (xPos + xDelta) < (float)numOfColumns;
            case Camera::LEFT:      return (xPos - xDelta) > 0;
            case Camera::UPWARD:    return (yPos + yDelta) < (float)numOfRows;
            case Camera::DOWNWARD:  return (yPos - yDelta) > 0;
            default: return false;
            }
            // clang-format on
        }();

        if (shouldMove) {
            m_camera.moveCamera(movement, (float)m_window.getDeltaTime());
        }
    }
};

#endif /* end of include guard: RENDERER_HPP_23RYFJ3HF3 */
