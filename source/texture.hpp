#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <glbinding/gl/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include <spdlog/spdlog.h>
#include <stb/stb_image.h>

#include <cstddef>    // std::size_t
#include <limits>     // std::numeric_limits
#include <unordered_map>

class Texture
{
private:
    static inline unsigned int s_textureUnitCount{ 0 };

    unsigned char* imageData{};

    gl::GLenum magFilter{};
    gl::GLenum minFilter{};
    gl::GLenum wrapFilter{};

    inline static std::unordered_map<std::size_t, gl::GLenum> numChannelsToGLenum{
        { 1, gl::GL_RED },
        { 2, gl::GL_RG },
        { 3, gl::GL_RGB },
        { 4, gl::GL_RGBA }
    };

public:
    static inline constexpr unsigned int maxUnitNum{ std::numeric_limits<unsigned int>::max() };

    unsigned int textureUnitNum{ maxUnitNum };    // means no texture loaded
    unsigned int textureID;                       // don't change this value outside of Texture class

    int imageWidth{};
    int imageHeight{};
    int nrChannels{};

    Texture(const Texture&) = default;

    // texture but basic material actually
    Texture(
        const unsigned char red   = 0x0,
        const unsigned char green = 0x0,
        const unsigned char blue  = 0x0
    )
    {
        textureUnitNum = s_textureUnitCount++;

        nrChannels = 3;

        imageData = new unsigned char[3];

        imageData[0] = red;
        imageData[1] = green;
        imageData[2] = blue;

        imageWidth  = 1;
        imageHeight = 1;

        generateTexture();

        delete[] imageData;
        imageData = nullptr;
    }

    Texture(
        unsigned char* colorData,
        int            width,
        int            height,
        int            channels  = 3,
        gl::GLenum     minFilter = gl::GL_LINEAR,
        gl::GLenum     magFilter = gl::GL_NEAREST,
        gl::GLenum     wrap      = gl::GL_REPEAT
    )
        : imageData{ colorData }
        , imageWidth{ width }
        , imageHeight{ height }
        , nrChannels{ channels }
    {
        textureUnitNum = s_textureUnitCount++;

        if (!imageData) {
            spdlog::error("(Texture) failed to load texture from colorData");
        } else {
            spdlog::info("(Shader) Successfully loaded texture from colorData ({})", textureUnitNum);
            generateTexture(minFilter, magFilter, wrap);
        }
    }

    Texture(
        const char* texFilePath,
        gl::GLenum  minFilter      = gl::GL_LINEAR,
        gl::GLenum  magFilter      = gl::GL_NEAREST,
        gl::GLenum  wrap           = gl::GL_REPEAT,
        bool        flipVertically = true
    )
    {
        textureUnitNum = s_textureUnitCount++;

        stbi_set_flip_vertically_on_load(flipVertically);
        imageData = stbi_load(texFilePath, &imageWidth, &imageHeight, &nrChannels, 0);

        if (!imageData) {
            spdlog::error("(Texture) failed to load texture from '{}'", texFilePath);
        } else {
            spdlog::info("(Shader) Successfully loaded texture from '{}' ({})", texFilePath, textureUnitNum);
            generateTexture(minFilter, magFilter, wrap);
        }

        stbi_image_free(imageData);
        imageData = nullptr;
    }

    void updateTexture(void* data, int width, int height, int numChannels = 3, gl::GLenum dataType = gl::GL_UNSIGNED_BYTE)
    {
        gl::glBindTexture(gl::GL_TEXTURE_2D, textureID);
        // gl::glTexImage2DGL_TEXTURE_2D, 0, numChannelsToGLenum[numChannels], width, height, 0, numChannelsToGLenum[numChannels], dataType, data);
        gl::glTexSubImage2D(gl::GL_TEXTURE_2D, 0, 0, 0, width, height, numChannelsToGLenum[numChannels], dataType, data);
        // gl::glGenerateMipmap(GL_TEXTURE_2D);
    }

    void updateMagFilter(gl::GLenum magFilter)
    {
        gl::glBindTexture(gl::GL_TEXTURE_2D, textureID);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, magFilter);
    }

    void updateMinFilter(gl::GLenum minFilter)
    {
        gl::glBindTexture(gl::GL_TEXTURE_2D, textureID);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, minFilter);
    }

    void updateWrapFilter(gl::GLenum wrap)
    {
        gl::glBindTexture(gl::GL_TEXTURE_2D, textureID);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, wrap);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, wrap);
    }

    void updateFilters(gl::GLenum magFilter, gl::GLenum minFilter, gl::GLenum wrap)
    {
        gl::glBindTexture(gl::GL_TEXTURE_2D, textureID);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, magFilter);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, minFilter);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, wrap);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, wrap);
    }

private:
    void generateTexture(gl::GLenum minFilter = gl::GL_LINEAR, gl::GLenum magFilter = gl::GL_NEAREST, gl::GLenum wrap = gl::GL_REPEAT)
    {
        // generate texture
        gl::glGenTextures(1, &textureID);

        // bind texture
        gl::glBindTexture(gl::GL_TEXTURE_2D, textureID);

        // set texture parameters
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, wrap);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, wrap);

        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, minFilter);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, magFilter);

        // now generate texture from image
        gl::glTexImage2D(gl::GL_TEXTURE_2D, 0, numChannelsToGLenum[nrChannels], imageWidth, imageHeight, 0, numChannelsToGLenum[nrChannels], gl::GL_UNSIGNED_BYTE, imageData);
        gl::glGenerateMipmap(gl::GL_TEXTURE_2D);
    }
};

#endif
