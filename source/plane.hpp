#ifndef PLANE_HPP
#define PLANE_HPP

#include "unrolled_matrix.hpp"

#include <glbinding/gl/gl.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <vector>

class Plane
{
public:
    using Vec_type = std::array<float, 3>;

    enum class DrawMode
    {
        FULL,
        PARTIAL    // only draw the indices specified by customizeIndices
    };

    Plane(
        float            sideLength     = 1.0f,
        glm::vec<2, int> subdivision    = { 1, 1 },
        glm::vec2        textureScaling = { 1.0f, 1.0f }
    )
        : m_subdivision{ subdivision }
        , m_textureScaling{ textureScaling }
    {
        generateVerticesAndIndices(sideLength);
        buildInterleavedVertices();
        setBuffers();
    }

    ~Plane() = default;

    void draw(DrawMode mode)
    {
        // clang-format off
        const auto& indices = [&] { switch (mode) {
            case DrawMode::FULL: return m_fullIndices;
            case DrawMode::PARTIAL: return m_shownIndices;
        } }();
        // clang-format on

        gl::glBindBuffer(gl::GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        gl::glBufferData(
            gl::GL_ELEMENT_ARRAY_BUFFER,
            static_cast<gl::GLsizeiptr>(indices.size() * sizeof(std::remove_cvref_t<decltype(indices)>::value_type)),
            indices.data(),
            gl::GL_DYNAMIC_DRAW
        );

        gl::glBindVertexArray(m_vao);
        gl::glDrawElements(gl::GL_TRIANGLES, (gl::GLsizei)indices.size(), gl::GL_UNSIGNED_INT, 0);
        gl::glBindVertexArray(0);
    }

    // exclusive: [xStart, xEnd), [yStart, yEnd), return new indices,
    template <typename TT, std::invocable<const TT&> Func>
        requires std::same_as<bool, std::invoke_result_t<Func, const TT&>>
    void customizeIndices(
        int                       xStart,
        int                       xEnd,
        int                       yStart,
        int                       yEnd,
        const UnrolledMatrix<TT>& reference,
        Func&&                    comp
    )
    {
        std::vector<unsigned int> indices;

        // convert 2D coordinate to 1D indices with each 2D point corresponds to 6 points in 1D indices
        for (int x{ xStart }; x < xEnd; ++x) {
            for (int y{ yStart }; y < yEnd; ++y) {
                if (!comp(reference(x, y))) {
                    continue;
                }

                auto idx{ static_cast<std::size_t>((x * m_subdivision.y + y) * 6) };

                indices.push_back(m_fullIndices[idx++]);
                indices.push_back(m_fullIndices[idx++]);
                indices.push_back(m_fullIndices[idx++]);
                indices.push_back(m_fullIndices[idx++]);
                indices.push_back(m_fullIndices[idx++]);
                indices.push_back(m_fullIndices[idx++]);
            }
        }

        m_shownIndices = indices;
    }

    void resetIndices()
    {
        m_shownIndices.clear();
        deleteBuffers();
        setBuffers();
    }

    auto subdivision() const
    {
        return m_subdivision;
    }

private:
    inline static gl::GLsizei m_interleavedVerticesStrideSize{ 5 * sizeof(float) };

    // buffers
    unsigned int m_vao;
    unsigned int m_vbo;
    unsigned int m_ebo;

    std::vector<Vec_type>     m_vertices;
    std::vector<unsigned int> m_fullIndices;
    std::vector<unsigned int> m_shownIndices{};
    std::vector<float>        m_interleavedVertices;

    glm::vec<2, int> m_subdivision    = { 1, 1 };
    glm::vec2        m_textureScaling = m_subdivision;

    void setBuffers()
    {
        gl::glGenVertexArrays(1, &m_vao);
        gl::glGenBuffers(1, &m_vbo);
        gl::glGenBuffers(1, &m_ebo);

        // bind
        //----
        gl::glBindVertexArray(m_vao);

        gl::glBindBuffer(gl::GL_ARRAY_BUFFER, m_vbo);
        gl::glBufferData(
            gl::GL_ARRAY_BUFFER,
            static_cast<gl::GLsizeiptr>(m_interleavedVertices.size() * sizeof(decltype(m_interleavedVertices)::value_type)),
            m_interleavedVertices.data(),
            gl::GL_STATIC_DRAW
        );

        gl::glBindBuffer(gl::GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        gl::glBufferData(
            gl::GL_ELEMENT_ARRAY_BUFFER,
            static_cast<gl::GLsizeiptr>(m_fullIndices.size() * sizeof(decltype(m_fullIndices)::value_type)),
            m_fullIndices.data(),
            gl::GL_DYNAMIC_DRAW
        );

        // vertex attribute
        //-----------------
        // position
        gl::glEnableVertexAttribArray(0);
        gl::glVertexAttribPointer(0, 3, gl::GL_FLOAT, gl::GL_FALSE, m_interleavedVerticesStrideSize, (void*)(0));

        // texcoords
        gl::glEnableVertexAttribArray(1);
        gl::glVertexAttribPointer(1, 2, gl::GL_FLOAT, gl::GL_FALSE, m_interleavedVerticesStrideSize, (void*)(3 * sizeof(float)));

        // unbind
        //----
        gl::glBindBuffer(gl::GL_ARRAY_BUFFER, 0);
        gl::glBindVertexArray(0);
    }

    void deleteBuffers()
    {
        gl::glDeleteVertexArrays(1, &m_vao);
        gl::glDeleteBuffers(1, &m_vbo);
    }

    void generateVerticesAndIndices(float sideLength)
    {
        std::vector<float> xVertices((std::size_t)m_subdivision.x + 1);
        std::vector<float> yVertices((std::size_t)m_subdivision.y + 1);

        float xDelta{ sideLength / static_cast<float>(m_subdivision.x) };
        float yDelta{ sideLength / static_cast<float>(m_subdivision.y) };

        struct PointsGenerator
        {
            float m_delta;
            float m_current;

            float operator()()
            {
                auto tmp{ m_current };
                m_current += m_delta;
                return tmp;
            }
        };

        std::generate(xVertices.begin(), xVertices.end(), PointsGenerator{ xDelta, -sideLength / 2.0f });
        std::generate(yVertices.begin(), yVertices.end(), PointsGenerator{ yDelta, -sideLength / 2.0f });

        std::vector<std::vector<Vec_type>> vertices2D(xVertices.size(), std::vector<Vec_type>(yVertices.size()));
        for (std::size_t i{ 0 }; i < xVertices.size(); ++i) {
            auto& x{ xVertices[i] };
            auto& row{ vertices2D[i] };
            for (std::size_t j{ 0 }; j < yVertices.size(); ++j) {
                auto& y{ yVertices[j] };
                row[j] = { x, y, 0.0f };
            }
        }

        // flatten vertices
        for (const auto& row : vertices2D) {
            m_vertices.insert(m_vertices.end(), row.begin(), row.end());
        }

        const auto toFlatIndex = [&](std::size_t col, std::size_t row) {
            return static_cast<unsigned int>(row * yVertices.size() + col);
        };

        // indices
        for (std::size_t x{ 0 }; x < (std::size_t)m_subdivision.x; ++x) {
            for (std::size_t y{ 0 }; y < (std::size_t)m_subdivision.y; ++y) {
                // clang-format off
                m_fullIndices.insert(m_fullIndices.end(), {
                    toFlatIndex(y, x),
                    toFlatIndex(y, x+1),
                    toFlatIndex(y+1, x+1),
                    toFlatIndex(y, x),
                    toFlatIndex(y+1, x+1),
                    toFlatIndex(y+1, x)
                });
                // clang-format on
            }
        }
    }

    void buildInterleavedVertices()
    {
        // texture coods multiplied -> much bigger coords -> tiled
        for (const auto& [x, y, z] : m_vertices) {
            // vertices
            m_interleavedVertices.push_back(x);
            m_interleavedVertices.push_back(y);
            m_interleavedVertices.push_back(z);

            // texCoords
            m_interleavedVertices.push_back((x + 0.5f) * m_textureScaling.x);
            m_interleavedVertices.push_back((y + 0.5f) * m_textureScaling.y);
        }
    }
};

#endif /* ifndef PLANE_HPP */
