#ifndef GAME_HPP
#define GAME_HPP

#include "threadpool.hpp"
#include "unrolled_matrix.hpp"

#include <PerlinNoise.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <ctime>
#include <map>
#include <random>
#include <ranges>
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

    enum class UpdateStrategy
    {
        INTERLEAVED,
        CHUNKED,
    };

    static constexpr Cell LIVE_STATE = 0xff;
    static constexpr Cell DEAD_STATE = 0x00;

    static inline const std::map<std::string, UpdateStrategy> s_updateStrategyMap{
        { "interleaved", UpdateStrategy::INTERLEAVED },
        { "chunked", UpdateStrategy::CHUNKED }
    };

    Grid(const Coord_type width, const Coord_type height, UpdateStrategy updateStrategy)
        : m_front{ width, height }
        , m_back{ width, height }
        , m_threadPool{ std::thread::hardware_concurrency() }
        , m_width{ width }
        , m_height{ height }
        , m_updateStrategy{ updateStrategy }
    {
        spdlog::info("(Grid) Created with width: [{}], height: [{}]", width, height);
        spdlog::info("(Grid) Using update strategy: [{}]", [&] {
            for (const auto& [key, value] : s_updateStrategyMap) {
                if (value == updateStrategy) {
                    return key;
                }
            }
            return std::string{ "unknown" };    // this should never happen
        }());
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
        process_multi([&](long x, long y) {
            auto spawn   = shouldSpawn((int)x, (int)y, density) && getRandomBool(density);
            m_back(x, y) = m_front(x, y) = spawn ? LIVE_STATE : DEAD_STATE;
        });
    }

    void updateState()
    {
        process_multi([this](long x, long y) {
            auto cell    = m_front(x, y);
            auto neigbor = checkNeighbors((int)x, (int)y);

            // clang-format off
            if (cell == LIVE_STATE) {
                if      (neigbor <  2) { m_back(x, y) = (cell == 0 || cell - 1 == 0) ? DEAD_STATE : cell - 1; }
                else if (neigbor <= 3) { m_back(x, y) = LIVE_STATE; }
                else                   { m_back(x, y) = (cell == 0 || cell - 1 == 0) ? DEAD_STATE : cell - 1; }
            } else {
                if      (neigbor == 3) { m_back(x, y) = LIVE_STATE; }
                else                   { m_back(x, y) = (cell == 0 || cell - 1 == 0) ? DEAD_STATE : cell - 1; }
            }

            // // incorrect but interesting result
            // if (cell == LIVE_STATE) {
            //     if      (neigbor <  2) { m_back(x, y) -= 1; }
            //     else if (neigbor <= 3) { m_back(x, y) = LIVE_STATE; }
            //     else                   { m_back(x, y) -= 1; }
            // } else {
            //     if      (neigbor == 3) { m_back(x, y) = LIVE_STATE; }
            //     else                   { m_back(x, y) = (cell == 0 || cell - 1 == 0) ? DEAD_STATE : cell - 1; }
            // }
            // clang-format on
        });

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
        switch (type) {
        case BufferType::FRONT:
            return m_front(xPos, yPos);
        case BufferType::BACK:
            return m_back(xPos, yPos);
        }
    }

    Cell& get(const Coord_type xPos, const Coord_type yPos, BufferType type = BufferType::FRONT)
    {
        switch (type) {
        case BufferType::FRONT:
            return m_front(xPos, yPos);
        case BufferType::BACK:
            return m_back(xPos, yPos);
        }
    }

    Cell& operator()(const Coord_type xPos, const Coord_type yPos)
    {
        // boundary condition: wrap-around
        if (!isInBound(xPos, yPos)) {
            int effX{ (m_width + xPos % m_width) % m_width };    // following python behavior of operator% (modulo)
            int effY{ (m_height + yPos % m_height) % m_height };
            return get(effX, effY);
        }
        return get(xPos, yPos);
    }

    const Cell& operator()(const Coord_type xPos, const Coord_type yPos) const
    {
        // boundary condition: wrap-around
        if (!isInBound(xPos, yPos)) {
            int effX{ (m_width + xPos % m_width) % m_width };    // following python behavior of operator% (modulo)
            int effY{ (m_height + yPos % m_height) % m_height };
            return get(effX, effY);
        }
        return get(xPos, yPos);
    }

    Grid_type& data(BufferType type = BufferType::FRONT)
    {
        switch (type) {
        case BufferType::FRONT:
            return m_front;
        case BufferType::BACK:
            return m_back;
        }
    }

    const Grid_type& data(BufferType type = BufferType::FRONT) const
    {
        switch (type) {
        case BufferType::FRONT:
            return m_front;
        case BufferType::BACK:
            return m_back;
        }
    }

    Coord_type width() const { return m_width; }
    Coord_type height() const { return m_height; }

    // return length, width
    const std::pair<int, int> dimension() const { return { m_width, m_height }; }

private:
    Grid_type      m_front;    // this one is to be shown to the outside
    Grid_type      m_back;     // updates done here
    ThreadPool     m_threadPool;
    Coord_type     m_width          = 0;
    Coord_type     m_height         = 0;
    UpdateStrategy m_updateStrategy = UpdateStrategy::INTERLEAVED;

    siv::BasicPerlinNoise<float> m_perlin{ static_cast<siv::PerlinNoise::seed_type>(std::time(nullptr)) };
    float                        m_perlinFreq   = 8.0f;
    int                          m_perlinOctave = 8;

    bool shouldSpawn(int x, int y, float probability) const
    {
        const float fx{ m_perlinFreq / m_width };
        const float fy{ m_perlinFreq / m_height };

        return m_perlin.octave2D_01(fx * (float)x, fy * (float)y, m_perlinOctave) < probability;
    }

    // process the grid in parallel
    void process_multi(std::invocable<long, long> auto&& func)
    {
        switch (m_updateStrategy) {
        case UpdateStrategy::INTERLEAVED:
            processInterleaved(std::forward<decltype(func)>(func));
            break;
        case UpdateStrategy::CHUNKED:
            processChunked(std::forward<decltype(func)>(func));
            break;
        }
    }

    void processInterleaved(std::invocable<long, long> auto&& func)
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
                    for (auto x : std::views::iota(0l, m_width)) {
                        func(x, y);
                    }
                }
            }));
        }

        for (auto& future : futures) {
            future.get();
        }
    }

    void processChunked(std::invocable<long, long> auto&& func)
    {
        const auto concurrencyLevel = std::min((long)m_threadPool.size(), (long)m_height);    // make sure at least 1
        const auto chunkSize        = m_height / concurrencyLevel;

        std::vector<std::future<void>> futures;
        futures.reserve((std::size_t)concurrencyLevel);

        // chunked update
        for (auto i : std::views::iota(0l, concurrencyLevel)) {
            const auto chunkBegin = i * chunkSize;
            const auto chunkEnd   = std::min(chunkBegin + chunkSize, (long)m_height);

            futures.emplace_back(m_threadPool.enqueue([=, this] {
                for (auto y : std::views::iota(chunkBegin, chunkEnd)) {
                    for (auto x : std::views::iota(0l, m_width)) {
                        func(x, y);
                    }
                }
            }));
        }

        for (auto& future : futures) {
            future.get();
        }
    }
};

#endif
