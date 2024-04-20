#ifndef TEXTURE_HPP_QDZVR1QU
#define TEXTURE_HPP_QDZVR1QU

#include "shader.hpp"

#include <glbinding/gl/gl.h>

#include <type_traits>
#include <string>
#include <string_view>

// an abstract base class for all textures
class Texture
{

public:
    Texture()                          = delete;
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept
        : m_target{ std::exchange(other.m_target, {}) }
        , m_id{ std::exchange(other.m_id, 0) }
        , m_unitNum{ std::exchange(other.m_unitNum, -1) }
        , m_uniformName{ std::move(other.m_uniformName) }
    {
    }

    Texture& operator=(Texture&& other)
    {
        if (this == &other) {
            return *this;
        }

        if (m_id != 0 && m_unitNum != -1) {
            gl::glDeleteTextures(1, &m_id);
        }

        m_target      = std::exchange(other.m_target, {});
        m_id          = std::exchange(other.m_id, 0);
        m_unitNum     = std::exchange(other.m_unitNum, -1);
        m_uniformName = std::move(other.m_uniformName);

        return *this;
    }

    virtual ~Texture() = 0;    // so that the class can't be instantiated

    gl::GLuint id() const
    {
        return m_id;
    }

    gl::GLint unitNum() const
    {
        return m_unitNum;
    }

    std::string_view uniformName() const
    {
        return m_uniformName;
    }

    void setUniformName(std::string name)
    {
        m_uniformName = std::move(name);
    }

    void activate(Shader& shader) const
    {
        shader.setUniform(m_uniformName, m_unitNum);
        gl::glActiveTexture(gl::GL_TEXTURE0 + std::underlying_type_t<gl::GLenum>(m_unitNum));
        gl::glBindTexture(m_target, m_id);
    }

protected:
    gl::GLenum  m_target;
    gl::GLuint  m_id;
    gl::GLint   m_unitNum;
    std::string m_uniformName;

    Texture(gl::GLenum target, gl::GLint unitNum, std::string uniformName)
        : m_target{ target }
        , m_id{ 0 }
        , m_unitNum{ unitNum }
        , m_uniformName{ std::move(uniformName) }
    {
    }

    Texture(gl::GLenum target, gl::GLuint id, gl::GLint unitNum, std::string uniformName)
        : m_target{ target }
        , m_id{ id }
        , m_unitNum{ unitNum }
        , m_uniformName{ std::move(uniformName) }
    {
    }
};

// handle the deletion of the texture object, the derived class doesn't need to worry about it
inline Texture::~Texture()
{
    if (m_id != 0 && m_unitNum != -1) {
        gl::glDeleteTextures(1, &m_id);
    }
}

#endif /* end of include guard: TEXTURE_HPP_QDZVR1QU */
