#ifndef IMAGE_TEXTURE_HPP_23VAW8H39
#define IMAGE_TEXTURE_HPP_23VAW8H39

#include "texture.hpp"

#include <glbinding/gl/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <vector>

class ImageData
{
public:
    const int            m_width{};
    const int            m_height{};
    const int            m_nrChannels{};
    const unsigned char* m_data{};

public:
    ImageData(const ImageData&)            = delete;
    ImageData& operator=(const ImageData&) = delete;
    ImageData(ImageData&& other)
        : m_width{ other.m_width }
        , m_height{ other.m_height }
        , m_nrChannels{ other.m_nrChannels }
        , m_data{ other.m_data }
    {
        other.m_data = nullptr;
    }

    ~ImageData()
    {
        if (m_data) {
            stbi_image_free(const_cast<unsigned char*>(m_data));
        }
    }

private:
    ImageData(int w, int h, int c, unsigned char* d)
        : m_width{ w }
        , m_height{ h }
        , m_nrChannels{ c }
        , m_data{ d }
    {
    }

public:
    static std::optional<ImageData> from(std::filesystem::path imagePath, bool flipVertically = true)
    {
        stbi_set_flip_vertically_on_load(flipVertically);

        int            width, height, nrChannels;
        unsigned char* data{ stbi_load(imagePath.c_str(), &width, &height, &nrChannels, 0) };
        if (!data) {
            std::cerr << std::format("Failed to load image at {}\n", imagePath.string());
            return {};
        }
        return ImageData{ width, height, nrChannels, data };
    }

    // pad data to 4 channels
    static std::vector<std::array<unsigned char, 4>> addPadding(const ImageData& data)
    {
        std::vector<std::array<unsigned char, 4>> newData(std::size_t(data.m_width * data.m_height));    // default initialize
        for (std::size_t i{ 0 }; i < newData.size(); ++i) {
            auto& byte{ newData[i] };
            for (int channel{ 0 }; channel < data.m_nrChannels; ++channel) {
                auto idx{ i * (std::size_t)data.m_nrChannels + (std::size_t)channel };
                byte[(std::size_t)channel] = data.m_data[idx];
            }
            byte[3] = 0xff;    // set alpha to max value
        }
        return newData;
    }
};

class ImageTexture final : public Texture
{
public:
    struct Specification
    {
        gl::GLenum m_minFilter;
        gl::GLenum m_magFilter;
        gl::GLenum m_wrapFilter;
    };

    static std::optional<ImageTexture> from(
        std::filesystem::path imagePath,
        std::string           uniformName,
        gl::GLint             textureUnitNum,
        Specification&&       spec
    )
    {
        auto maybeImageData{ ImageData::from(imagePath) };
        if (!maybeImageData) {
            return {};
        }
        return ImageTexture{
            std::move(*maybeImageData),
            imagePath,
            std::move(uniformName),
            textureUnitNum,
            std::move(spec)
        };
    }

    const std::filesystem::path& imagePath() const
    {
        return m_imagePath;
    }

private:
    std::filesystem::path m_imagePath;
    Specification         m_spec;

    ImageTexture(
        ImageData&&           imageData,
        std::filesystem::path imagePath,
        std::string           uniformName,
        gl::GLint             textureUnitNum,
        Specification         spec
    )
        : Texture{ gl::GL_TEXTURE_2D, textureUnitNum, std::move(uniformName) }
        , m_imagePath{ std::move(imagePath) }
        , m_spec{ std::move(spec) }
    {
        gl::glGenTextures(1, &m_id);
        gl::glBindTexture(m_target, m_id);

        gl::glTexParameteri(m_target, gl::GL_TEXTURE_WRAP_S, m_spec.m_wrapFilter);
        gl::glTexParameteri(m_target, gl::GL_TEXTURE_WRAP_T, m_spec.m_wrapFilter);
        gl::glTexParameteri(m_target, gl::GL_TEXTURE_MIN_FILTER, m_spec.m_minFilter);
        gl::glTexParameteri(m_target, gl::GL_TEXTURE_MAG_FILTER, m_spec.m_magFilter);

        if (imageData.m_nrChannels == 4) {
            gl::glTexImage2D(m_target, 0, gl::GL_RGBA, imageData.m_width, imageData.m_height, 0, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, imageData.m_data);
        } else if (imageData.m_nrChannels == 3) {
            gl::glTexImage2D(m_target, 0, gl::GL_RGB, imageData.m_width, imageData.m_height, 0, gl::GL_RGB, gl::GL_UNSIGNED_BYTE, imageData.m_data);
        } else {
            // pad data if not rgb or rgba
            auto newData{ ImageData::addPadding(imageData) };
            gl::glTexImage2D(m_target, 0, gl::GL_RGBA, imageData.m_width, imageData.m_height, 0, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, &newData.front());
        }
        gl::glGenerateMipmap(m_target);

        gl::glBindTexture(m_target, 0);
    }
};

#endif /* end of include guard: IMAGE_TEXTURE_HPP_23VAW8H39 */
