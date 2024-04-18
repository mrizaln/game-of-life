#ifndef PLANE_HPP
#define PLANE_HPP

#include "unrolled_matrix.hpp"

#include <glad/glad.h>

#include <algorithm>    // std::generate
#include <array>
#include <concepts>
#include <cstddef>    // std::size_t
#include <vector>

// #define ENABLE_NORMAL

template <typename T = float>
class Plane
{
public:
    using value_type = T;
    using vec_type   = std::array<value_type, 3>;

    Plane(
        int  xSides          = 1,
        int  ySides          = 1,
        bool adjustTexture   = false,
        bool generateBuffers = true
    )
        : m_subdivideX{ xSides }
        , m_subdivideY{ ySides }
    {
        if (adjustTexture) {
            m_texCoordsMultiplierX = static_cast<value_type>(xSides);
            m_texCoordsMultiplierY = static_cast<value_type>(ySides);
        }
        generateVerticesAndIndices();
        buildInterleavedVertices();

        if (generateBuffers) {
            setBuffers();
        }
    }

    ~Plane() = default;

    void draw()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_shownIndices.size() * sizeof(m_shownIndices.front()), m_shownIndices.data(), GL_DYNAMIC_DRAW);

        // bind buffer
        glBindVertexArray(VAO);

        // draw
        // glDrawArrays(GL_TRIANGLES, 0, std::size(m_interleavedVertices));
        std::size_t indicesSize{ m_shownIndices.size() };
        glDrawElements(GL_TRIANGLES, indicesSize, GL_UNSIGNED_INT, 0);

        // unbind buffer
        glBindVertexArray(0);
    }

    void deleteBuffers()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void multiplyTexCoords(value_type width, value_type height)
    {
        // width
        m_texCoordsMultiplierX = width;
        m_texCoordsMultiplierY = height;

        buildInterleavedVertices();
        deleteBuffers();
        setBuffers();
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

                auto idx{ (x * m_subdivideY + y) * 6 };

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

    const std::pair<int, int> getSubdivision() const
    {
        return { m_subdivideX, m_subdivideY };
    }

private:
    static constexpr value_type START_FROM{ -0.5 };
    static constexpr value_type END_TO{ 0.5 };

#ifdef ENABLE_NORMAL
    inline static std::size_t m_interleavedVerticesStrideSize{ 8 * sizeof(Value_type) };
#else
    inline static std::size_t m_interleavedVerticesStrideSize{ 5 * sizeof(value_type) };
#endif

    // buffers
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;

    int m_subdivideX{};
    int m_subdivideY{};

    std::vector<vec_type>     m_vertices;
    std::vector<unsigned int> m_fullIndices;
    std::vector<unsigned int> m_shownIndices{};
    std::vector<value_type>   m_interleavedVertices;

    value_type m_texCoordsMultiplierX{ 1.0 };
    value_type m_texCoordsMultiplierY{ 1.0 };

    void setBuffers()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // bind
        //----
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, m_interleavedVertices.size() * sizeof(m_interleavedVertices.front()), &m_interleavedVertices.front(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_fullIndices.size() * sizeof(m_fullIndices.front()), &m_fullIndices.front(), GL_DYNAMIC_DRAW);

        // vertex attribute
        //-----------------
        // position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(0));

        // texcoords
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(3 * sizeof(value_type)));

#ifdef ENABLE_NORMAL
        // normal
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(5 * sizeof(Value_type)));
#endif

        // unbind
        //----
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void generateVerticesAndIndices()
    {
        std::vector<value_type>            xVertices(m_subdivideX + 1);
        std::vector<value_type>            yVertices(m_subdivideY + 1);
        std::vector<std::vector<vec_type>> vertices2D(xVertices.size(), std::vector<vec_type>(yVertices.size()));

        value_type xDelta{ (END_TO - START_FROM) / static_cast<value_type>(m_subdivideX) };
        value_type yDelta{ (END_TO - START_FROM) / static_cast<value_type>(m_subdivideY) };

        struct PointsGenerator
        {
            value_type m_delta{};
            value_type m_current{ START_FROM };

            PointsGenerator(value_type delta)
                : m_delta{ delta }
            {
            }

            value_type operator()()
            {
                auto tmp{ m_current };
                m_current += m_delta;
                return tmp;
            }
        };

        std::generate(xVertices.begin(), xVertices.end(), PointsGenerator{ xDelta });
        std::generate(yVertices.begin(), yVertices.end(), PointsGenerator{ yDelta });

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

        const auto toFlatIndex = [&](unsigned int col, unsigned int row) -> unsigned int {
            return row * yVertices.size() + col;
        };

        // indices
        for (std::size_t x{ 0 }; x < m_subdivideX; ++x) {
            for (std::size_t y{ 0 }; y < m_subdivideY; ++y) {
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
        constexpr std::array normal{ 0.0f, 0.0f, 1.0f };
        for (const auto& [x, y, z] : m_vertices) {
            // vertices
            m_interleavedVertices.push_back(x);
            m_interleavedVertices.push_back(y);
            m_interleavedVertices.push_back(z);

            // texCoords
            m_interleavedVertices.push_back((x + 0.5f) * m_texCoordsMultiplierX);
            m_interleavedVertices.push_back((y + 0.5f) * m_texCoordsMultiplierY);

#ifdef ENABLE_NORMAL
            // normals
            m_interleavedVertices.push_back(normal[0]);
            m_interleavedVertices.push_back(normal[1]);
            m_interleavedVertices.push_back(normal[2]);
#endif
        }
    }
};

#endif /* ifndef PLANE_HPP */
