#ifndef GRID_TILE_H
#define GRID_TILE_H

#include "image_texture.hpp"
#include "plane.hpp"
#include "shader.hpp"

#include <glbinding/gl/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

class GridTile
{
private:
    Plane        m_plane;
    ImageTexture m_texture;

public:
    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 m_scale{ 1.0f, 1.0f, 1.0f };
    glm::vec3 m_color{ 1.0f, 1.0f, 1.0f };
    Shader    m_shader{};

    GridTile()                           = delete;
    GridTile(const GridTile&)            = delete;
    GridTile& operator=(const GridTile&) = delete;
    GridTile(GridTile&&)                 = default;
    GridTile& operator=(GridTile&&)      = default;

    GridTile(
        glm::vec<2, int>      subdivision,
        glm::vec2             textureScaling,
        std::filesystem::path vShaderDir,
        std::filesystem::path fShaderDir,
        std::filesystem::path textureDir,
        gl::GLenum            texMinFilter,
        gl::GLenum            texMagFilter,
        gl::GLenum            wrapFilter,
        const glm::vec3&      position = { 0.0f, 0.0f, 0.0f },
        const glm::vec3&      color    = { 1.0f, 1.0f, 1.0f },
        const glm::vec3&      scale    = { 1.0f, 1.0f, 1.0f }
    )
        : m_plane{ 1.0f, subdivision, textureScaling }
        , m_texture{ [&] {
            ImageTexture::Specification spec{
                .m_minFilter  = texMinFilter,
                .m_magFilter  = texMagFilter,
                .m_wrapFilter = wrapFilter,
            };
            return ImageTexture::from(textureDir, "uTex", 0, std::move(spec)).value();    // throws on failure
        }() }
        , m_position{ position }
        , m_scale{ scale }
        , m_color{ color }
        , m_shader{ vShaderDir, fShaderDir }
    {
        m_shader.use();
        m_shader.setUniform("uColor", m_color);    // color
        m_texture.activate(m_shader);
    }

    auto& getPlane()
    {
        return m_plane;
    }

    void draw(Plane::DrawMode mode)
    {
        m_shader.use();
        m_texture.activate(m_shader);
        m_plane.draw(mode);
    }
};

#endif /* ifndef GRID_TILE_H */
