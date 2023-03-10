#ifndef PLANE_HPP
#define PLANE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <ios>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <ranges>
#include <vector>
#include <optional>

#include <glad/glad.h>

// #define ENABLE_NORMAL

template <typename T = float>
class Plane
{
public:
    using Value_type = T;
    using Vec_type   = std::array<Value_type, 3>;

    Plane(
        int  xSides          = 1,
        int  ySides          = 1,
        bool adjustTexture   = false,
        bool generateBuffers = true)
        : m_subdivideX{ xSides }
        , m_subdivideY{ ySides }
    {
        if (adjustTexture) {
            m_texCoordsMultiplierX = static_cast<Value_type>(xSides);
            m_texCoordsMultiplierY = static_cast<Value_type>(ySides);
        }
        std::cout << "Generating plane...\n";
        generateVerticesAndIndices();
        buildInterleavedVertices();

        if (generateBuffers) {
            setBuffers();
        }
        std::cout << "Done\n";
    }

    ~Plane() = default;

    void draw() const
    {
        // bind buffer
        glBindVertexArray(VAO);

        // draw
        // glDrawArrays(GL_TRIANGLES, 0, std::size(m_interleavedVertices));
        const auto& indices{ m_indices_custom.has_value() ? m_indices_custom.value() : m_indices };
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // unbind buffer
        glBindVertexArray(0);
    }

    void deleteBuffers()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void multiplyTexCoords(Value_type width, Value_type height)
    {
        // width
        m_texCoordsMultiplierX = width;
        m_texCoordsMultiplierY = height;

        buildInterleavedVertices();
        deleteBuffers();
        setBuffers();
    }

    // exclusive: [xStart, xEnd), [yStart, yEnd), return new indices,
    // __bool_like: can convert implicitly to bool
    template <typename __bool_like = bool>
    void customizeIndices(const std::vector<std::vector<__bool_like>>& condition, int xStart, int xEnd, int yStart, int yEnd)
    {
        std::vector<unsigned int> indices;

        // convert 2D coordinate to 1D indices with each 2D point corresponds to 6 points in 1D indices
        // std::cout << "\nIDX: ";
        for (int x{ xStart }; x < xEnd; ++x) {
            for (int y{ yStart }; y < yEnd; ++y) {
                if (!condition[y][x]) {
                    continue;
                }

                auto idx{ (x * m_subdivideY + y) * 6 };
                // std::cout << idx <<  ';

                indices.push_back(m_indices[idx++]);
                indices.push_back(m_indices[idx++]);
                indices.push_back(m_indices[idx++]);
                indices.push_back(m_indices[idx++]);
                indices.push_back(m_indices[idx++]);
                indices.push_back(m_indices[idx++]);
            }
        }
        // std::cout << '\n';
        // print(&indices);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices.front()), &indices.front(), GL_STATIC_DRAW);

        m_indices_custom = indices;
    }

    void resetIndices()
    {
        m_indices_custom.reset();
        deleteBuffers();
        setBuffers();
    }

    const std::pair<int, int> getSubdivision() const
    {
        return { m_subdivideX, m_subdivideY };
    }

    void print(const std::vector<unsigned int>* indices = nullptr) const
    {
        auto coutStateOld{ std::cout.flags() };

        std::cout << "\nInterleavedVertices:\n";
        auto& v{ m_interleavedVertices };
        for (std::size_t i{ 0 }; i < std::size(m_interleavedVertices); i += 8) {
            std::cout << std::fixed << std::setprecision(2) << std::showpos
                      << v[i] << '\t' << v[i + 1] << '\t' << v[i + 2] << "\t\t"
                      << v[i + 3] << '\t' << v[i + 4] << '\t' << v[i + 5] << "\t\t"
                      << v[i + 6] << '\t' << v[i + 7] << '\n';
        }

        std::cout << "\nIndices:\n";
        const auto& ind{ indices ? *indices : m_indices };
        for (std::size_t i{ 0 }; i < ind.size(); ++i) {
            std::cout << ind[i] << '\t';
            if ((i + 1) % 6 == 0) {
                std::cout << '\n';
            }
        }

        std::cout.flags(coutStateOld);
    }

private:
    // buffers
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;

    static constexpr Value_type START_FROM{ -0.5 };
    static constexpr Value_type END_TO{ 0.5 };

#ifdef ENABLE_NORMAL
    inline static std::size_t m_interleavedVerticesStrideSize{ 8 * sizeof(Value_type) };
#else
    inline static std::size_t m_interleavedVerticesStrideSize{ 5 * sizeof(Value_type) };
#endif

    int m_subdivideX{};
    int m_subdivideY{};

    std::vector<Vec_type>                    m_vertices;
    std::vector<unsigned int>                m_indices;
    std::optional<std::vector<unsigned int>> m_indices_custom{ std::nullopt };
    std::vector<Value_type>                  m_interleavedVertices;

    Value_type m_texCoordsMultiplierX{ 1.0 };
    Value_type m_texCoordsMultiplierY{ 1.0 };

    struct __PointsGenerator
    {
        Value_type m_delta{};
        Value_type s_current{ START_FROM };

        __PointsGenerator(Value_type delta)
            : m_delta{ delta }
        {
        }

        Value_type operator()()
        {
            auto tmp{ s_current };
            s_current += m_delta;
            return tmp;
        }
    };

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
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(m_indices.front()), &m_indices.front(), GL_STATIC_DRAW);

        // vertex attribute
        //-----------------
        // position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(0));

        // texcoords
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(3 * sizeof(Value_type)));

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
        std::vector<Value_type>            xVertices(m_subdivideX + 1);
        std::vector<Value_type>            yVertices(m_subdivideY + 1);
        std::vector<std::vector<Vec_type>> vertices2D(xVertices.size(), std::vector<Vec_type>(yVertices.size()));

        Value_type xDelta{ (END_TO - START_FROM) / static_cast<Value_type>(m_subdivideX) };
        Value_type yDelta{ (END_TO - START_FROM) / static_cast<Value_type>(m_subdivideY) };

        std::generate(xVertices.begin(), xVertices.end(), __PointsGenerator{ xDelta });
        std::generate(yVertices.begin(), yVertices.end(), __PointsGenerator{ yDelta });

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
                m_indices.insert(m_indices.end(), {
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
        // this->print();
    }
};

#endif /* ifndef PLANE_HPP */
