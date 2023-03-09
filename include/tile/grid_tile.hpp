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

class GridTile
{
private:
    Plane<float>               m_plane;
    Texture                    m_texture{ 0xff, 0xff, 0xff };
    std::optional<Texture>     m_texture_state{ std::nullopt };
    std::vector<unsigned char> m_texture_state_color;

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

    // GridTile(
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
        , m_texture_state{ std::nullopt }
        , m_position{ position }
        , m_color{ color }
        , m_scale{ scale }
    {
        m_shader.use();
        m_shader.setInt("uTex", m_texture.textureUnitNum);
    }

    auto& getPlane() { return m_plane; }

    // __bool_like type needs to be able to convert itself to bool value
    template <typename __bool_like>
    void setState(const std::vector<std::vector<__bool_like>>& state, float xPos, float yPos, int left, int right, int top, int bottom)
    {
        static bool              once{ false };
        const auto               height{ state.size() };
        const auto               width{ state[0].size() };
        constexpr int            NUM_OF_CHANNELS{ 4 };
        constexpr int            MAX_NUM_OF_THREAD{ 64 };
        std::vector<std::thread> threadPool;

        // culling
        constexpr float offset{ 1.5f };
        // clang-format off
        const int rowLeftBorder  { static_cast<int>(  xPos + left       - offset  > 0      ?   xPos + left   - offset  : 0) };
        const int rowRightBorder { static_cast<int>(  xPos + right + 1  + offset  < width  ?   xPos + right  + offset  : width) };
        const int colTopBorder   { static_cast<int>(-(yPos + top        + offset) > 0      ? -(yPos + top    + offset) : 0) };
        const int colBottomBorder{ static_cast<int>(-(yPos + bottom + 1 - offset) < height ? -(yPos + bottom - offset) : height) };    // y up-down inverted
        // clang-format on

        int modification_num{ 0 };
        // flatten
        if (!once) {
            m_texture_state_color = std::vector<unsigned char>(width * height * NUM_OF_CHANNELS);
        }
        auto& flatState{ m_texture_state_color };
        // for (const auto& row : state | std::views::reverse) {
        // for (int i{ 0 }; i < height; ++i) {
        for (int i{ colTopBorder }; i < colBottomBorder; ++i) {
            const auto& row{ state[i] };
            if (threadPool.size() >= MAX_NUM_OF_THREAD || threadPool.size() == height) {
                for (auto& thread : threadPool) {
                    if (!thread.joinable()) {
                        std::cout << "stuck\n";
                    }
                    thread.join();
                }
                threadPool.clear();
            }

            auto i_cur{ i };
            threadPool.emplace_back([&](int i_cur) {
                // for (int j{ 0 }; j < row.size(); ++j) {
                for (int j{ rowLeftBorder }; j < rowRightBorder; ++j) {
                    const auto& c{ row[j] };
                    const auto  idx{ ((height - i_cur - 1) * width + j) * 4 };
                    const auto  max{ std::numeric_limits<unsigned char>::max() };
                    flatState[idx + 0] = bool(c) * max;    // r
                    flatState[idx + 1] = bool(c) * max;    // g
                    flatState[idx + 2] = bool(c) * max;    // b
                    flatState[idx + 3] = bool(c) * max;    // a

                    // modification_num++;

                    // std::cout << "size: [" << i_cur << ", " << j  << ", " << idx  << ", " << state.size() << ", " << row.size() << "]\n";
                }
            }, i);
        }
        // printf("[%i, %i, %i, %i] (%i, %u)\n", rowLeftBorder, rowRightBorder, colTopBorder, colBottomBorder, modification_num, m_texture_state_color.size());

        // cleanup
        for (auto& thread : threadPool) {
            thread.join();
        }

        // initialize once
        if (!once) {
            m_texture_state.emplace(&flatState.front(), width, height, NUM_OF_CHANNELS);
            m_shader.use();
            m_shader.setInt("uState", m_texture_state->textureUnitNum);

            once = true;
        }

        m_texture_state->updateTexture(&flatState.front(), width, height, NUM_OF_CHANNELS);
    }

    void draw()
    {
        m_shader.use();

        // color
        m_shader.setVec3("uColor", m_color);

        // set texture
        glActiveTexture(GL_TEXTURE0 + m_texture.textureUnitNum);
        glBindTexture(GL_TEXTURE_2D, m_texture.textureID);

        // set state
        glActiveTexture(GL_TEXTURE0 + m_texture_state->textureUnitNum);
        glBindTexture(GL_TEXTURE_2D, m_texture_state->textureID);

        // set length and width
        const auto& [l, w]{ m_plane.getSubdivision() };
        m_shader.setInt("uLength", l);
        m_shader.setInt("uWidth", w);

        // draw
        m_plane.draw();
    }
};

#endif /* ifndef GRID_TILE_H */
