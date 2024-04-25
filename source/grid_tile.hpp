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
public:
    struct PlaneInfo
    {
        glm::vec<2, int> m_subdivision;
        glm::vec2        m_textureScaling;
        glm::vec3        m_position;
        glm::vec3        m_color;
        glm::vec3        m_scale;
    };

    struct ShaderInfo
    {
        std::filesystem::path m_vertexShaderDir;
        std::filesystem::path m_fragmentShaderDir;
    };

    struct TextureInfo
    {
        std::filesystem::path       m_textureDir;
        ImageTexture::Specification m_textureSpec;
    };

    Plane        m_plane;
    ImageTexture m_texture;
    glm::vec3    m_position{ 0.0f, 0.0f, 0.0f };
    glm::vec3    m_scale{ 1.0f, 1.0f, 1.0f };
    glm::vec3    m_color{ 1.0f, 1.0f, 1.0f };
    Shader       m_shader{};

    GridTile()                           = delete;
    GridTile(const GridTile&)            = delete;
    GridTile& operator=(const GridTile&) = delete;
    GridTile(GridTile&&)                 = default;
    GridTile& operator=(GridTile&&)      = default;

    GridTile(
        PlaneInfo   planeInfo,
        ShaderInfo  shaderInfo,
        TextureInfo textureInfo
    )
        : m_plane{ 1.0f, planeInfo.m_subdivision, planeInfo.m_textureScaling }
        , m_texture{ [&] {
            auto [textureDir, spec] = std::move(textureInfo);
            return ImageTexture::from(textureDir, "u_tex", 0, std::move(spec)).value();    // throws on failure
        }() }
        , m_position{ planeInfo.m_position }
        , m_scale{ planeInfo.m_scale }
        , m_color{ planeInfo.m_color }
        , m_shader{ shaderInfo.m_vertexShaderDir, shaderInfo.m_fragmentShaderDir }
    {
        m_shader.use();
        m_shader.setUniform("u_color", m_color);    // color
        m_texture.activate(m_shader);
    }

    void draw(Plane::DrawMode mode)
    {
        m_shader.use();
        m_texture.activate(m_shader);
        m_plane.draw(mode);
    }
};

#endif /* ifndef GRID_TILE_H */
