#ifndef GAME_HPP
#define GAME_HPP

#include <algorithm>    // std::for_each
#include <ctime>        // std::time
#include <execution>    // std::execution
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>    // std::pair
#include <vector>

#include <perlin_noise/PerlinNoise.hpp>

class Cell
{
public:
    inline static const bool s_dead_state = false;

private:
    bool m_state{};
    bool m_state_next{};

public:
    Cell() = default;

    constexpr auto isLive() const { return m_state; }
    constexpr auto isDead() const { return !m_state; }
    constexpr void setLive() { m_state_next = true; }     // won't be affected before update() has been called
    constexpr void setDead() { m_state_next = false; }    // won't be affected before update() has been called

    void update() { m_state = m_state_next; }

    operator bool() const { return m_state; }
};

class Grid
{
public:
    using Coord_type     = int;
    using Grid_type      = std::vector<std::vector<Cell>>;    // X number of Cells inside Y number of vectors
    using Grid_flat_type = std::vector<Cell>;

private:
    Coord_type m_width{};     // Y
    Coord_type m_length{};    // X
    Grid_type  m_grid{};      // states

    // threading
    static constexpr int     MAX_NUM_OF_THREAD{ 64 };
    std::vector<std::thread> threadPool{};

    static int getRandomNumber(int min, int max)
    {
        static std::mt19937 mt{ static_cast<std::mt19937::result_type>(std::time(nullptr)) };
        return std::uniform_int_distribution{ min, max }(mt);
    }

    // 0 < probability < 1
    static bool getRandomBool(float probability)
    {
        static std::mt19937 mt{ static_cast<std::mt19937::result_type>(std::time(nullptr)) };
        return std::uniform_real_distribution<float>{}(mt) < probability;
    }

    bool shouldSpawn(int x, int y, float probability)
    {
        static const siv::BasicPerlinNoise<float> perlin{ static_cast<siv::PerlinNoise::seed_type>(std::time(nullptr)) };
        static const float                        freq{ 8.0f };
        static const int                          octave{ 8 };

        const float fx{ freq / m_length };
        const float fy{ freq / m_width };

        return perlin.octave2D_01(fx * x, fy * y, octave) < probability;
    }

public:
    // 0.0f < return < 1.0f
    static float getRandomProbability()
    {
        return (getRandomNumber(0, std::numeric_limits<int>::max()) / static_cast<float>(std::numeric_limits<int>::max()));
    }

    Grid(const Coord_type length, const Coord_type width)
        : m_length{ length }
        , m_width{ width }
        , m_grid{ std::vector<std::vector<Cell>>(width, std::vector<Cell>(length)) }
    {
    }

    ~Grid() = default;

    void populate(const float density)
    {
        // for (auto& row : m_grid) {
        for (Coord_type i{ 0 }; i < m_length; ++i) {
            auto& row{ m_grid[i] };
            // for (auto& cell : row) {
            for (Coord_type j{ 0 }; j < m_width; ++j) {
                auto& cell{ (*this)(i, j) };
                // if (getRandomBool(density)) {
                if (shouldSpawn(i, j, density) && getRandomBool(density)) {
                    cell.setLive();
                } else {
                    cell.setDead();
                }
            }
        }

        for (auto& row : m_grid) {
            for (auto& cell : row) {
                cell.update();
            }
        }
    }

    Grid& updateState()
    {
        for (Coord_type i{ 0 }; i < m_length; ++i) {
            if (threadPool.size() >= MAX_NUM_OF_THREAD) {
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
                for (Coord_type j{ 0 }; j < m_width; ++j) {
                    auto& cell{ (*this)(i_cur, j) };

                    auto numberOfNeighbor{ checkNeighbors(i_cur, j) };

                    if (cell.isLive()) {
                        if (numberOfNeighbor < 2) {
                            cell.setDead();
                        } else if (numberOfNeighbor <= 3) {
                            cell.setLive();
                        } else {
                            cell.setDead();
                        }
                    } else if (numberOfNeighbor == 3) {
                        cell.setLive();
                    }
                }
            },
                i);
        }

        for (auto& thread : threadPool) {
            thread.join();
        }
        threadPool.clear();

        // for (auto& row : m_grid) {
        //     for (auto& cell : row) {
        //         cell.update();
        //     }
        // }

        std::for_each(std::execution::par_unseq, m_grid.begin(), m_grid.end(), [&](std::vector<Cell>& row) {
            std::for_each(std::execution::par_unseq, row.begin(), row.end(), [&](Cell& cell) { cell.update(); });
        });

        return *this;
    }

    void clear()
    {
        // zeros
        m_grid = std::vector<std::vector<Cell>>(m_width, std::vector<Cell>(m_length));
    }

    // return the number of live neighbors
    constexpr int checkNeighbors(const Coord_type xPos, const Coord_type yPos)
    {
        auto& x{ xPos };
        auto& y{ yPos };

        /**
         * check 8 neighbors of a cell
         *      [x-1, y-1] [x, y-1] [x+1, y-1]
         *      [x-1, y  ] [x, y  ] [x+1, y  ]
         *      [x-1, y+1] [x, y+1] [x+1, y+1]
         */

        return (
            checkState(x - 1, y - 1) + checkState(x, y - 1) + checkState(x + 1, y - 1)
            + checkState(x - 1, y) + checkState(x + 1, y)
            + checkState(x - 1, y + 1) + checkState(x, y + 1) + checkState(x + 1, y + 1));
    }

    constexpr bool isInBound(const Coord_type xPos, const Coord_type yPos) const
    {
        return (
            (xPos >= 0 && xPos < m_length)
            && (yPos >= 0 && yPos < m_width));
    }

    constexpr bool checkState(const Coord_type xPos, const Coord_type yPos) const
    {
        if (!isInBound(xPos, yPos)) {    // boundary condition: return dead state
            // int effX{ xPos % m_length + 2 * (xPos < 0) - 1 };
            // int effY{ yPos % m_width + 2 * (yPos < 0) - 1 };
            // std::cout << m_length << '/' << m_width << ": ";
            // std::cout << xPos << '/' << yPos << " -> ";
            int effX{ (m_length + xPos % m_length) % m_length };    // following python behavior (surprisingly operator% behaves differently between python and cpp
            int effY{ (m_width + yPos % m_width) % m_width };
            // std::cout << effX << '/' << effY << '\n';

            return (*this)(effX, effY).isLive();
            // return Cell::s_dead_state;
        }

        return (*this)(xPos, yPos).isLive();
    }

    constexpr bool checkState(const Cell& cell) const
    {
        return cell.isLive();
    }

    const auto& data() const { return m_grid; }

    // __return_cell_type can be anything as long as you provided the conversion function in pred.
    // if __return_cell_type == bool, you can use data_flat_bool() instead
    // if __return_cell_type == Cell, you can use data_flat_Cell() instead
    template <typename __return_cell_type>
    const auto data_flat(std::function<__return_cell_type(const Cell&)> pred) const
    {
        std::vector<__return_cell_type> vec;
        for (std::size_t y{ 0 }; y < m_width; ++y) {
            for (std::size_t x{ 0 }; x < m_length; ++x) {
                vec.push_back(pred((*this)(x, y)));
            }
        };
        return vec;
    }

    const auto data_flat_bool() const
    {
        std::vector<bool> vec;
        for (std::size_t y{ 0 }; y < m_width; ++y) {
            for (std::size_t x{ 0 }; x < m_length; ++x) {
                vec.push_back(bool((*this)(x, y)));
            }
        }
        return vec;
    }

    const auto data_flat_Cell() const
    {
        std::vector<bool> vec;
        for (std::size_t y{ 0 }; y < m_width; ++y) {
            for (std::size_t x{ 0 }; x < m_length; ++x) {
                vec.push_back((*this)(x, y));
            }
        }
        return vec;
    }

    auto&            getCell(int col, int row) { return (*this)(col, row); }
    const auto&      getCell(int col, int row) const { return (*this)(col, row); }
    const Coord_type getWidth() const { return m_width; }
    const Coord_type getLength() const { return m_length; }

    // return length, width
    const std::pair<int, int> getDimension() const { return { m_length, m_width }; }

    constexpr Cell& operator()(const Coord_type xPos, const Coord_type yPos)
    {
        if (!isInBound(xPos, yPos)) {
            throw std::out_of_range{ "out of range" };
        }

        return m_grid[yPos][xPos];
    }
    constexpr const Cell& operator()(const Coord_type xPos, const Coord_type yPos) const
    {
        if (!isInBound(xPos, yPos)) {
            throw std::range_error{ "out of range" };
        }

        return m_grid[yPos][xPos];
    }

    friend std::ostream& operator<<(std::ostream& out, const Grid& grid)
    {
        for (auto& row : grid.m_grid) {
            for (auto& cell : row) {
                switch (cell.isLive()) {
                case true:
                    std::cout << "█";
                    break;
                case false:
                    std::cout << " ";
                    break;
                }
            }
            std::cout << '\n';
        }
        return out;
    }
};

#endif
