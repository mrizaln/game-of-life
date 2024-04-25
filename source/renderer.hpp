#ifndef RENDERER_HPP_23RYFJ3HF3
#define RENDERER_HPP_23RYFJ3HF3

#include "camera.hpp"
#include "game.hpp"
#include "grid_tile.hpp"
#include "plane.hpp"
#include "window.hpp"
#include "window_manager.hpp"

#include <glbinding/gl/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <type_traits>

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

    template <typename T>
    struct Dimension
    {
        T m_width  = 0;
        T m_height = 0;
    };

    struct Cache
    {
        Dimension<int> m_gridDimension   = {};
        Dimension<int> m_windowDimension = {};
    };

    Renderer()                           = delete;
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    Renderer(const Window& window, const Grid& grid)
        : m_borderTile{
            GridTile::PlaneInfo{
                .m_subdivision    = { 1, 1 },
                .m_textureScaling = { grid.width(), grid.height() },
                .m_position       = { (float)grid.width() / 2.0f, (float)grid.height() / 2.0f, 0.0f },
                .m_color          = { 1.0f, 1.0f, 1.0f },
                .m_scale          = { grid.width(), grid.height(), 0.0f },
            },
            GridTile::ShaderInfo{
                .m_vertexShaderDir   = "./resources/shaders/grid_shader.vert",
                .m_fragmentShaderDir = "./resources/shaders/grid_shader.frag",
            },
            GridTile::TextureInfo{
                .m_textureDir  = "./resources/textures/grid.png",
                .m_textureSpec = {
                    .m_minFilter  = gl::GL_LINEAR,
                    .m_magFilter  = gl::GL_LINEAR,
                    .m_wrapFilter = gl::GL_REPEAT,
                },
            },
        }
        , m_gridTile{
            GridTile::PlaneInfo{
                .m_subdivision    = { grid.width(), grid.height() },
                .m_textureScaling = { grid.width(), grid.height() },
                .m_position       = { (float)grid.width() / 2.0f, (float)grid.height() / 2.0f, 0.0f },
                .m_color          = { 1.0f, 1.0f, 1.0f },
                .m_scale          = { grid.width(), grid.height(), 0.0f },
            },
            GridTile::ShaderInfo{
                .m_vertexShaderDir   = "./resources/shaders/grid_shader.vert",
                .m_fragmentShaderDir = "./resources/shaders/grid_shader.frag",
            },
            GridTile::TextureInfo{
                .m_textureDir  = "./resources/textures/cell.png",
                .m_textureSpec = {
                    .m_minFilter  = gl::GL_LINEAR,
                    .m_magFilter  = gl::GL_LINEAR,
                    .m_wrapFilter = gl::GL_REPEAT,
                },
            },
        }
        , m_camera{}
        , m_gridMode{ GridMode::AUTO }
    {
        m_camera.speed = 100.0f;

        const auto winProp        = window.getProperties();
        m_cache.m_windowDimension = {
            .m_width  = winProp.m_width,
            .m_height = winProp.m_height,
        };
        m_cache.m_gridDimension = {
            .m_width  = (int)grid.width(),    // might throw if first element not exist
            .m_height = (int)grid.height(),
        };

        resetCamera(true);
    }

    void render(const Window& window, const Grid::Grid_type& gridData, bool isPaused)
    {
        const auto winProp        = window.getProperties();
        m_cache.m_windowDimension = {
            .m_width  = winProp.m_width,
            .m_height = winProp.m_height,
        };
        m_cache.m_gridDimension = {
            .m_width  = (int)gridData.width(),    // might throw if first element not exist
            .m_height = (int)gridData.height(),
        };

        if (isPaused) {
            gl::glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        } else {
            gl::glClearColor(0.1f, 0.1f, 0.11f, 1.0f);
        }
        gl::glClear(gl::GL_COLOR_BUFFER_BIT | gl::GL_DEPTH_BUFFER_BIT);

        gl::glViewport(0, 0, winProp.m_width, winProp.m_height);

        // orthogonal frustum
        // clang-format off
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
        const auto  width  = (float)m_cache.m_gridDimension.m_width;
        const auto  height = (float)m_cache.m_gridDimension.m_height;

        constexpr float offset{ 1.5f };

        // culling
        // clang-format off
        const int rowLeftBorder  { static_cast<int>( xPos + left       - offset > 0      ? xPos + left   - offset : 0) };
        const int rowRightBorder { static_cast<int>( xPos + right + 1  + offset < width  ? xPos + right  + offset : width) };
        const int colTopBorder   { static_cast<int>( yPos + top        + offset < height ? yPos + top    + offset : height) };
        const int colBottomBorder{ static_cast<int>( yPos + bottom + 1 - offset > 0      ? yPos + bottom - offset : 0) };
        // clang-format on

        updateGrid({ rowLeftBorder, rowRightBorder, colBottomBorder, colTopBorder }, gridData);

        // draw
        drawBorder(projMat, viewMat, isPaused);
        drawGrid(projMat, viewMat);
    }

    void processCameraMovement(Camera::CameraMovement movement, float deltaTime)
    {
        float xPos{ m_camera.position.x };
        float yPos{ m_camera.position.y };
        float xDelta{ m_cache.m_windowDimension.m_width / m_camera.zoom };
        float yDelta{ m_cache.m_windowDimension.m_height / m_camera.zoom };
        auto& [numOfColumns, numOfRows]{ m_cache.m_gridDimension };    // potential data race

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
            m_camera.moveCamera(movement, deltaTime);
        }
    }

    void fitToWindow()
    {
        resetCamera(false);

        const auto gridWidth = (float)m_cache.m_gridDimension.m_width;
        const auto winWidth  = (float)m_cache.m_windowDimension.m_width;
        const auto zoom      = 2 / 1.0f * winWidth / gridWidth;
        setCameraZoom(zoom);

        constexpr int maxCol{ 75 };

        const auto& Z{ zoom };
        auto        z{ std::max({
            2.0f * winWidth / gridWidth,    // all grid visible
            4.0f * winWidth / gridWidth,    // half column
            // 4/1.1f * winHeight / gridHeight,     // half row

            2.0f * winWidth / float(maxCol)    // show only number of maxCol
        }) };

        auto       n{ -std::log(Z / z) / std::log(1.1f) };
        const auto speed = 100.0f * std::pow(1.1f, n / 2);
        setCameraSpeed(speed);
    }

    glm::vec3 getCameraPosition()
    {
        return m_camera.position;
    }

    float getCameraZoom()
    {
        return m_camera.zoom;
    }

    void setCameraPosition(float xPos, float yPos)
    {
        m_camera.position.x = xPos;
        m_camera.position.y = yPos;
    }

    void setCameraZoom(float zoom)
    {
        m_camera.zoom = zoom;
    }

    void setCameraSpeed(float speed)
    {
        m_camera.speed = speed;
    }

    void offsetCameraPosition(float xOffset, float yOffset, float multiplier = 1.0f)
    {
        m_camera.position.x += xOffset * m_camera.speed / multiplier;
        m_camera.position.y += yOffset * m_camera.speed / multiplier;
    }

    void offsetCameraZoom(float yOffset)
    {
        m_camera.processMouseScroll(yOffset, Camera::ZOOM);
    }

    void resetCamera(bool resetZoom)
    {
        auto& [numOfColumns, numOfRows]{ m_cache.m_gridDimension };

        // center camera
        m_camera.position.x = (float)numOfColumns / 2.0f;
        m_camera.position.y = (float)numOfRows / 2.0f;

        constexpr int maxCol{ 75 };

        if (resetZoom) {
            auto width    = (float)m_cache.m_windowDimension.m_width;
            m_camera.zoom = std::max({
                2.0f * width / float(numOfColumns),    // all grid visible
                4.0f * width / float(numOfColumns),    // half column
                // 4/1.1f * height/float(numOfRows),     // half row

                2.0f * width / float(maxCol)    // show only number of maxCol
            });
        }
    }

    void multiplyCameraSpeed(float multiplier)
    {
        m_camera.speed *= multiplier;
    }

    void cycleGridMode()
    {
        using Int  = std::underlying_type_t<GridMode>;
        m_gridMode = static_cast<GridMode>(
            (static_cast<Int>(m_gridMode) + 1) % static_cast<Int>(GridMode::numOfGridModes)
        );
    }

private:
    GridTile m_borderTile;
    GridTile m_gridTile;
    Camera   m_camera;
    GridMode m_gridMode;
    Cache    m_cache;

    void updateGrid(const Border& border, const Grid::Grid_type& gridData)
    {
        const auto& [x1, x2, y1, y2] = border;
        m_gridTile.m_plane.customizeIndices(x1, x2, y1, y2, gridData, [](const Grid::Cell& cell) {
            return cell == Grid::LIVE_STATE;
        });
    }

    void drawBorder(const glm::mat4& projMat, const glm::mat4& viewMat, bool isPaused)
    {
        bool shouldDraw{};

        switch (m_gridMode) {
        case GridMode::ON:
        {
            shouldDraw = true;
            break;
        }
        case GridMode::AUTO:
        {
            const auto    winWidth{ m_cache.m_windowDimension.m_width };
            const float   xDelta{ winWidth / m_camera.zoom };        // (number of cells per width of screen)/2
            constexpr int minNumberOfCellsPerScreenHeight{ 100 };    // change accordingly
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
        m_borderTile.m_shader.setUniform("u_view", viewMat);
        m_borderTile.m_shader.setUniform("u_projection", projMat);

        // model matrix
        glm::mat4 model{ 1.0f };
        model = glm::translate(model, m_borderTile.m_position);
        model = glm::scale(model, m_borderTile.m_scale);
        m_borderTile.m_shader.setUniform("u_model", model);

        // change color if simulation::pause
        if (isPaused) {
            m_borderTile.m_color = { 0.7f, 1.0f, 0.7f };
        } else {
            m_borderTile.m_color = { 1.0f, 1.0f, 1.0f };
        }

        // draw
        m_borderTile.draw(Plane::DrawMode::FULL);
    }

    void drawGrid(const glm::mat4& projMat, const glm::mat4& viewMat)
    {
        m_gridTile.m_shader.use();
        m_gridTile.m_shader.setUniform("u_view", viewMat);
        m_gridTile.m_shader.setUniform("u_projection", projMat);

        glm::mat4 model{ 1.0f };
        model = glm::translate(model, m_gridTile.m_position);
        model = glm::scale(model, m_gridTile.m_scale);

        m_gridTile.m_shader.setUniform("u_model", model);

        m_gridTile.draw(Plane::DrawMode::PARTIAL);
    }
};

#endif /* end of include guard: RENDERER_HPP_23RYFJ3HF3 */
