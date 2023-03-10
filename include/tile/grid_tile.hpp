#ifndef GRID_TILE_H
#define GRID_TILE_H

#include <limits>
#include <ranges>
#include <optional>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shader_header/shader.hpp>
#include <camera_header/camera.hpp>
#include <texture_header/texture.hpp>
#include <plane/plane.hpp>
#include <thread>

#include <timer/timer.h>

class GridTile
{
private:
    Plane<float>           m_plane;
    Texture                m_texture{ 0xff, 0xff, 0xff };

public:
    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 m_scale{ 1.0f, 1.0f, 1.0f };
    glm::vec3 m_color{ 1.0f, 1.0f, 1.0f };
    Shader    m_shader{};

    GridTile()                           = delete;
    GridTile& operator=(const GridTile&) = delete;

    GridTile(const GridTile& tile)
        : m_plane{ tile.m_plane }        // copy plane VAO, VBO, EBO : points to the same plane as copied
        , m_shader{ tile.m_shader }      // copy shader ID           : points to the same shader as copied
        , m_texture{ tile.m_texture }    // copy texture ID          : points to the same texture as copied
        , m_position{ tile.m_position }
        , m_color{ tile.m_color }
        , m_scale{ tile.m_scale }

    {
    }

    GridTile(
        int              length,
        int              width,
        const char*      vShaderDir,
        const char*      fShaderDir,
        const char*      textureDir,
        int              texMinFilter = GL_LINEAR,
        int              texMagFilter = GL_LINEAR,
        int              wrapFilter   = GL_REPEAT,
        const glm::vec3& position     = { 0.0f, 0.0f, 0.0f },
        const glm::vec3& color        = { 1.0f, 1.0f, 1.0f },
        const glm::vec3& scale        = { 1.0f, 1.0f, 1.0f })
        : m_plane{ length, width, true }
        , m_shader{ vShaderDir, fShaderDir }
        , m_texture{ textureDir, texMagFilter, texMinFilter, wrapFilter }
        , m_position{ position }
        , m_color{ color }
        , m_scale{ scale }
    {
        m_shader.use();
        m_shader.setInt("uTex", m_texture.textureUnitNum);    // texture
        m_shader.setVec3("uColor", m_color);                  // color
    }

    auto& getPlane() { return m_plane; }

    // __bool_like type needs to be able to convert itself to bool value
    template <typename __bool_like>
    void setState(const std::vector<std::vector<__bool_like>>& state, float xPos, float yPos, int left, int right, int top, int bottom)
    {
        const auto height{ state.size() };
        const auto width{ state[0].size() };

        // culling
        constexpr float offset{ 1.5f };
        // clang-format off
        const int rowLeftBorder  { static_cast<int>(  xPos + left       - offset  > 0      ?   xPos + left   - offset  : 0) };
        const int rowRightBorder { static_cast<int>(  xPos + right + 1  + offset  < width  ?   xPos + right  + offset  : width) };
        const int colTopBorder   { static_cast<int>(-(yPos + top        + offset) > 0      ? -(yPos + top    + offset) : 0) };
        const int colBottomBorder{ static_cast<int>(-(yPos + bottom + 1 - offset) < height ? -(yPos + bottom - offset) : height) };    // y up-down inverted
        // clang-format on

        m_plane.customizeIndices<__bool_like>(state, rowLeftBorder, rowRightBorder, height - colBottomBorder, height - colTopBorder);
    }

    void draw(bool update = false)
    {
        m_shader.use();

        // set texture
        glActiveTexture(GL_TEXTURE0 + m_texture.textureUnitNum);
        glBindTexture(GL_TEXTURE_2D, m_texture.textureID);

        // draw
        m_plane.draw();
    }
};

#endif /* ifndef GRID_TILE_H */
