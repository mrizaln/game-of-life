#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>    // include glad to get all the required OpenGL headers
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <cstddef>
#include <iostream>
#include <limits>
#include <unordered_map>

class Texture
{
private:
    static inline unsigned int s_textureUnitCount{ 0 };

    int imageWidth{};
    int imageHeight{};
    int nrChannels{};

    unsigned char* imageData{};

    GLenum magFilter{};
    GLenum minFilter{};
    GLenum wrapFilter{};

    inline static const std::unordered_map<std::size_t, GLenum> numChannelsToGLenum{
        { 1, GL_RED },
        { 2, GL_RG },
        { 3, GL_RGB },
        { 4, GL_RGBA }
    };

public:
    static inline constexpr unsigned int maxUnitNum{ std::numeric_limits<unsigned int>::max() };

    unsigned int textureUnitNum{ maxUnitNum };    // means no texture loaded
    unsigned int textureID;                       // don't change this value outside of Texture class

    Texture(const Texture&) = default;

    // texture but basic material actually
    Texture(
        const unsigned char red   = 0x0,
        const unsigned char green = 0x0,
        const unsigned char blue  = 0x0)
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
        int            minFilter = GL_LINEAR,
        int            magFilter = GL_NEAREST,
        int            wrap      = GL_REPEAT)
        : imageData{ colorData }
        , imageWidth{ width }
        , imageHeight{ height }
        , nrChannels{ channels }
    {
        textureUnitNum = s_textureUnitCount++;

        if (!imageData) {
            std::cerr << "Failed to load texture: " << (void*)imageData << '\n';
        } else {
            std::cerr << "Successfully load texture from colorData (" << textureUnitNum << ")\n";
            generateTexture(minFilter, magFilter, wrap);
        }
    }

    Texture(
        const char* texFilePath,
        int         minFilter      = GL_LINEAR,
        int         magFilter      = GL_NEAREST,
        int         wrap           = GL_REPEAT,
        bool        flipVertically = true)
    {
        textureUnitNum = s_textureUnitCount++;

        stbi_set_flip_vertically_on_load(flipVertically);
        imageData = stbi_load(texFilePath, &imageWidth, &imageHeight, &nrChannels, 0);

        if (!imageData) {
            std::cerr << "Failed to load texture: " << texFilePath << '\n';
        } else {
            std::cerr << "Successfully load texture: " << texFilePath << " (" << textureUnitNum << ")\n";
            generateTexture(minFilter, magFilter, wrap);
        }

        stbi_image_free(imageData);
        imageData = nullptr;
    }

    void updateTexture(void* data, int width, int height, int numChannels = 3, GLenum dataType = GL_UNSIGNED_BYTE)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, numChannelsToGLenum[numChannels], width, height, 0, numChannelsToGLenum[numChannels], dataType, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void updateMagFilter(GLenum magFilter)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    }

    void updateMinFilter(GLenum minFilter)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    }

    void updateWrapFilter(GLenum wrap)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    }

    void updateFilters(GLenum magFilter, GLenum minFilter, GLenum wrap)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    }

private:
    void generateTexture(int minFilter = GL_LINEAR, int magFilter = GL_NEAREST, int wrap = GL_REPEAT)
    {
        // generate texture
        glGenTextures(1, &textureID);

        // bind texture
        glBindTexture(GL_TEXTURE_2D, textureID);

        // set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

        // now generate texture from image
        glTexImage2D(GL_TEXTURE_2D, 0, numChannelsToGLenum[nrChannels], imageWidth, imageHeight, 0, numChannelsToGLenum[nrChannels], GL_UNSIGNED_BYTE, imageData);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
};

#endif
