#ifndef GAME_HPP
#define GAME_HPP

#include "threadpool.hpp"
#include "unrolled_matrix.hpp"

#include <PerlinNoise.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <random>
#include <utility>

class Grid
{
public:
    using Cell       = std::uint8_t;    // i'm planning on using all the range a byte offer
    using Coord_type = int;
    using Grid_type  = UnrolledMatrix<Cell>;    // X number of Cells inside Y number of vectors

    enum class BufferType
    {
        FRONT,
        BACK
    };

    static constexpr Cell LIVE_STATE = 0xff;
    static constexpr Cell DEAD_STATE = 0x00;

    Grid(const Coord_type width, const Coord_type height)
        : m_width{ width }
        , m_height{ height }
        , m_front{ width, height }
        , m_back{ width, height }
        , m_threadPool{ std::thread::hardware_concurrency() }
    {
    }

    Grid(const Grid& other)            = delete;
    Grid& operator=(const Grid& other) = delete;
    Grid(Grid&& other)                 = delete;
    Grid& operator=(Grid&& other)      = delete;

    template <typename T>
    static T getRandomNumber(T min, T max)
    {
        thread_local static std::mt19937 mt{ static_cast<std::mt19937::result_type>(std::time(nullptr)) };
        if constexpr (std::integral<T>) {
            return std::uniform_int_distribution<T>{ min, max }(mt);
        } else if constexpr (std::floating_point<T>) {
            return std::uniform_real_distribution<T>{ min, max }(mt);
        } else if constexpr (std::is_arithmetic_v<T>) {
            auto real = std::uniform_real_distribution<>{ 0.0, 1.0 }(mt);
            return min + real * (max - min);
        } else {
            static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");
        }
    }

    // 0 < probability < 1
    static bool getRandomBool(float probability)
    {
        thread_local static std::mt19937 mt{ static_cast<std::mt19937::result_type>(std::time(nullptr)) };
        return std::uniform_real_distribution<float>{}(mt) < probability;
    }

    // 0.0f < return < 1.0f
    static float getRandomProbability()
    {
        return getRandomNumber(0.0f, 1.0f);
    }

    void populate(const float density = getRandomProbability())
    {
        for (auto y : std::views::iota(0l, m_height)) {
            for (auto x : std::views::iota(0l, m_width)) {
                auto spawn   = shouldSpawn((int)x, (int)y, density) && getRandomBool(density);
                m_back(x, y) = m_front(x, y) = spawn ? LIVE_STATE : DEAD_STATE;
            }
        }
    }

    void updateState()
    {
        const auto concurrencyLevel = m_threadPool.size();
        const auto chunkSize        = m_height / (long)concurrencyLevel;

        std::vector<std::future<void>> futures;
        futures.reserve(concurrencyLevel);

        // interleaved update
        for (auto i : std::views::iota(0l, (long)concurrencyLevel)) {
            const auto numSteps = [&] {
                const auto maxSize = chunkSize * (long)concurrencyLevel + i;
                if (maxSize < m_height) {
                    return chunkSize + 1;
                }
                return chunkSize;
            }();

            futures.emplace_back(m_threadPool.enqueue([=, this] {
                for (auto count : std::views::iota(0l, numSteps)) {
                    const auto y = count * (int)concurrencyLevel + i;
                    for (auto x : std::views::iota(0, m_width)) {
                        auto cell    = m_front(x, y);
                        auto neigbor = checkNeighbors((int)x, (int)y);

                        // clang-format off
                        if (cell == LIVE_STATE) {
                            if      (neigbor <  2) { m_back(x, y) -= 1; }
                            else if (neigbor <= 3) { m_back(x, y) = LIVE_STATE; }
                            else                   { m_back(x, y) -= 1; }
                        } else {
                            if      (neigbor == 3) { m_back(x, y) = LIVE_STATE; }
                            else                   { m_back(x, y) = (cell == 0 || cell - 1 == 0) ? DEAD_STATE : cell - 1; }
                        }
                        // clang-format on
                    }
                }
            }));
        }

        for (auto& future : futures) {
            future.get();
        }

        m_front.swap(m_back);
    }

    // zeroes-out the grid
    void clear()
    {
        m_back = m_front = { m_width, m_height, DEAD_STATE };
    }

    // return the number of live neighbors
    int checkNeighbors(const Coord_type xPos, const Coord_type yPos) const
    {
        auto checkState = [this](const Coord_type x, const Coord_type y) {
            return (*this)(x, y) == LIVE_STATE;
        };

        auto& x{ xPos };
        auto& y{ yPos };

        /**
         * check 8 neighbors of a cell
         *      [x-1, y-1] [x, y-1] [x+1, y-1]
         *      [x-1, y  ] [x, y  ] [x+1, y  ]
         *      [x-1, y+1] [x, y+1] [x+1, y+1]
         */

        return (
            // clang-format off
              checkState(x - 1, y - 1) + checkState(x, y - 1) + checkState(x + 1, y - 1)
            + checkState(x - 1, y    )                        + checkState(x + 1, y    )
            + checkState(x - 1, y + 1) + checkState(x, y + 1) + checkState(x + 1, y + 1)
            // clang-format on
        );
    }

    bool isInBound(const Coord_type xPos, const Coord_type yPos) const
    {
        return xPos >= 0 && xPos < m_width && yPos >= 0 && yPos < m_height;
    }

    const Cell& get(const Coord_type xPos, const Coord_type yPos, BufferType type = BufferType::FRONT) const
    {
        // clang-format off
        switch (type) {
        case BufferType::FRONT: return m_front(xPos, yPos);
        case BufferType::BACK:  return m_back(xPos, yPos);
        }
        // clang-format on
    }

    // unchecked access
    Cell& get(const Coord_type xPos, const Coord_type yPos, BufferType type = BufferType::FRONT)
    {
        return data(type)(xPos, yPos);
    }

    Cell& operator()(const Coord_type xPos, const Coord_type yPos)
    {
        // boundary condition: wrap-around
        if (!isInBound(xPos, yPos)) {
            int effX{ (m_width + yPos % m_width) % m_width };    // following python behavior of operator% (modulo)
            int effY{ (m_height + xPos % m_height) % m_height };
            return get(effX, effY);
        }
        return get(xPos, yPos);
    }

    const Cell& operator()(const Coord_type xPos, const Coord_type yPos) const
    {
        // boundary condition: wrap-around
        if (!isInBound(xPos, yPos)) {
            int effX{ (m_width + yPos % m_width) % m_width };    // following python behavior of operator% (modulo)
            int effY{ (m_height + xPos % m_height) % m_height };
            return get(effX, effY);
        }
        return get(xPos, yPos);
    }

    Grid_type& data(BufferType type = BufferType::FRONT)
    {
        // clang-format off
        switch (type) {
        case BufferType::FRONT: return m_front;
        case BufferType::BACK:  return m_back;
        }
        // clang-format on
    }

    const Grid_type& data(BufferType type = BufferType::FRONT) const
    {
        // clang-format off
        switch (type) {
        case BufferType::FRONT: return m_front;
        case BufferType::BACK: return m_back;
        }
        // clang-format on
    }

    Coord_type width() const { return m_width; }
    Coord_type height() const { return m_height; }

    // return length, width
    const std::pair<int, int> dimension() const { return { m_width, m_height }; }

private:
    Coord_type m_width  = 0;
    Coord_type m_height = 0;
    Grid_type  m_front;    // this one is to be shown to the outside
    Grid_type  m_back;     // updates done here
    ThreadPool m_threadPool;

    bool shouldSpawn(int x, int y, float probability)
    {
        static const siv::BasicPerlinNoise<float> perlin{ static_cast<siv::PerlinNoise::seed_type>(std::time(nullptr)) };
        static const float                        freq{ 8.0f };
        static const int                          octave{ 8 };

        const float fx{ freq / m_width };
        const float fy{ freq / m_height };

        return perlin.octave2D_01(fx * (float)x, fy * (float)y, octave) < probability;
    }
};

#endif
