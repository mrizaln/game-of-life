#ifndef SIMPLE_PLANE_HPP
#define SIMPLE_PLANE_HPP

#include <glad/glad.h>

#include <cstddef>
#include <iterator>

class SimplePlane
{
    // clang-format off
    static inline float s_planeVertices[]{
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
    };

    static inline float s_planeNormals[]{
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };

    static inline float s_planeTexCoords[]{
        0.0f, 0.0f,     // bottom-left
        1.0f, 0.0f,     // bottom-right
        1.0f, 1.0f,     // top-right
        0.0f, 1.0f,     // top-left
    };

    static inline unsigned int s_planeIndices[]{
        0, 1, 2,
        0, 2, 3,
    };
    // clang-format on

    // vertices data
    float m_vertices[12]{};
    float m_normals[12]{};
    float m_texCoords[8]{};

    // interleaved vertices data
    float m_interleavedVertices[32]{};
    int   m_interleavedVerticesStrideSize{};

    // buffers
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;

public:
    SimplePlane(float sideLength = 1.0f)
        : m_interleavedVerticesStrideSize{ 8 * sizeof(float) }
    {
        // copy vertices multiplied by sidelength
        for (std::size_t i{ 0 }; i < std::size(m_vertices); i++) {
            m_vertices[i] = s_planeVertices[i] * sideLength;
        }
        // std::copy(std::begin(s_planeVertices), std::end(s_planeVertices), std::begin(m_vertices));
        // std::for_each(std::begin(m_vertices), std::end(m_vertices), [sideLength](float& a){ a *= sideLength; });

        // copy normals
        std::copy(std::begin(s_planeNormals), std::end(s_planeNormals), std::begin(m_normals));

        // copy texCoords
        std::copy(std::begin(s_planeTexCoords), std::end(s_planeTexCoords), std::begin(m_texCoords));

        buildInterleavedVertices();
        setBuffers();
    }

    ~SimplePlane()
    {
        // deleteBuffers();     // segmentation fault???
    }

    void draw() const
    {
        // bind buffer
        glBindVertexArray(VAO);

        // draw
        // glDrawArrays(GL_TRIANGLES, 0, std::size(m_interleavedVertices));
        glDrawElements(GL_TRIANGLES, std::size(s_planeIndices), GL_UNSIGNED_INT, 0);

        // unbind buffer
        glBindVertexArray(0);
    }

    void deleteBuffers()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void multiplyTexCoords(float width, float height)
    {
        // width
        m_texCoords[2] *= width;     // x of bottom-right
        m_texCoords[4] *= width;     // x of top-right
        m_texCoords[5] *= height;    // y of top-right
        m_texCoords[7] *= height;    // y of top-left

        buildInterleavedVertices();
        deleteBuffers();
        setBuffers();
    }

private:
    void buildInterleavedVertices()
    {
        for (std::size_t i{ 0 }, j{ 0 }, k{ 0 }, l{ 0 }; i < std::size(m_interleavedVertices); i += 8, j += 3, k += 3, l += 2) {
            m_interleavedVertices[i]     = m_vertices[j];
            m_interleavedVertices[i + 1] = m_vertices[j + 1];
            m_interleavedVertices[i + 2] = m_vertices[j + 2];

            m_interleavedVertices[i + 3] = m_normals[k];
            m_interleavedVertices[i + 4] = m_normals[k + 1];
            m_interleavedVertices[i + 5] = m_normals[k + 2];

            m_interleavedVertices[i + 6] = m_texCoords[l];
            m_interleavedVertices[i + 7] = m_texCoords[l + 1];
        }
    }

    void setBuffers()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // bind
        //----
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(m_interleavedVertices), m_interleavedVertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(s_planeIndices), s_planeIndices, GL_STATIC_DRAW);

        // vertex attribute
        //-----------------
        // position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(0));

        // normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(3 * sizeof(float)));

        // texcoords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, m_interleavedVerticesStrideSize, (void*)(6 * sizeof(float)));

        // unbind
        //----
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
};

#endif
