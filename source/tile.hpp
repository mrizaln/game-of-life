#ifndef TILE_H
#define TILE_H

#include "shader.hpp"
#include "texture.hpp"
#include "simple_plane.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Tile
{
    SimplePlane m_plane{};
    Texture     m_texture{};

public:
    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 m_scale{ 1.0f, 1.0f, 1.0f };
    glm::vec3 m_color{ 1.0f, 1.0f, 1.0f };
    Shader    m_shader{};

    Tile()                       = delete;
    Tile& operator=(const Tile&) = delete;

    Tile(const Tile& tile)
        : m_plane{ tile.m_plane }          // copy plane VAO, VBO, EBO : points to the same plane as copied
        , m_texture{ tile.m_texture }      // copy shader ID           : points to the same shader as copied
        , m_position{ tile.m_position }    // copy texture ID          : points to the same texture as copied
        , m_scale{ tile.m_scale }
        , m_color{ tile.m_color }
        , m_shader{ tile.m_shader }

    {
    }

    // Tile(
    //     float width,
    //     const char* vShaderDir,
    //     const char* fShaderDir,
    //     Texture tex,
    //     const glm::vec3& position={0.0f, 0.0f, 0.0f},
    //     const glm::vec3& color={1.0f, 1.0f, 1.0f},
    //     const glm::vec3& scale={1.0f, 1.0f, 1.0f}
    // )
    //     : m_plane{ width }
    //     , m_shader{ vShaderDir, fShaderDir }
    //     , m_texture{ tex }
    //     , m_position{ position }
    //     , m_color{ color }
    //     , m_scale{ scale }
    // {
    // }

    Tile(
        float            width,
        const char*      vShaderDir,
        const char*      fShaderDir,
        const char*      textureDir,
        int              texMinFilter = GL_LINEAR,
        int              texMagFilter = GL_LINEAR,
        int              wrapFilter   = GL_REPEAT,
        const glm::vec3& position     = { 0.0f, 0.0f, 0.0f },
        const glm::vec3& color        = { 1.0f, 1.0f, 1.0f },
        const glm::vec3& scale        = { 1.0f, 1.0f, 1.0f }
    )
        : m_plane{ width }
        , m_shader{ vShaderDir, fShaderDir }
        , m_texture{ textureDir, texMagFilter, texMinFilter, wrapFilter }
        , m_position{ position }
        , m_color{ color }
        , m_scale{ scale }
    {
    }

    SimplePlane& getPlane() { return m_plane; }

    void draw()
    {
        m_shader.use();

        // color
        m_shader.setVec3("color", m_color);

        // set texture
        m_shader.setInt("tex", m_texture.textureUnitNum);
        glActiveTexture(GL_TEXTURE0 + m_texture.textureUnitNum);
        glBindTexture(GL_TEXTURE_2D, m_texture.textureID);

        // draw
        m_plane.draw();
    }
};

#endif
