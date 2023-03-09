#include <iostream>
#include <vector>
#include <random>
#include <ctime>
#include <utility>      // for std::pair

#ifndef GAME_H
#define GAME_H


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
    constexpr auto setLive() { m_state_next = true; }       // won't be affected before update() has been called
    constexpr auto setDead() { m_state_next = false; }      // won't be affected before update() has been called
    
    void update() { m_state = m_state_next; }
};


class Grid
{
public:
    using Coord_type = int;
    using Grid_type = std::vector<std::vector<Cell>>;       // X number of Cells inside Y number of vectors


private:
    Coord_type m_width{};
    Coord_type m_length{};
    Grid_type m_grid{};

    static int getRandomNumber(int min, int max)
    {
        static std::mt19937 mt{ static_cast<std::mt19937::result_type>(std::time(nullptr)) };
        return std::uniform_int_distribution{ min, max }(mt);
    }

    // 0 < probability < 1
    static bool getRandomBool(float probability)
    {
        int num{ getRandomNumber(0,std::numeric_limits<int>::max()) };
        float numNormalized{ static_cast<float>(num) / std::numeric_limits<int>::max() };
        return numNormalized < probability;
    }


public:
    Grid(const Coord_type length, const Coord_type width)
        : m_length{ length }
        , m_width{ width }
        , m_grid{ std::vector<std::vector<Cell>>(width, std::vector<Cell>(length)) }
    {
    }

    ~Grid() = default;

    void populate(const float density)
    {
        for (auto& row : m_grid)
        {
            for (auto& cell : row)
            {
                switch (getRandomBool(density))
                {
                case true:  cell.setLive(); break;
                case false: cell.setDead(); break;
                }
            }
        }

        for (auto& row : m_grid)
            for (auto& cell : row)
                cell.update();
    }

    Grid& updateState()
    {
        for (Coord_type i{ 0 }; i < m_length; ++i)
        {
            for (Coord_type j{ 0 }; j < m_width; ++j)
            {
                auto& cell{ (*this)(i, j) };
                
                auto numberOfNeighbor{ checkNeighbors(i, j) };
                
                if (cell.isLive())
                {
                    if      (numberOfNeighbor <  2) cell.setDead();
                    else if (numberOfNeighbor <= 3) cell.setLive();
                    else                            cell.setDead();
                }
                else
                    if      (numberOfNeighbor == 3) cell.setLive();
            }
        }

        for (auto& row : m_grid)
            for (auto& cell : row)
                cell.update();

        return *this;
    }

    void clear()
    {
        // zeros
        m_grid = std::vector<std::vector<Cell>>(m_width, std::vector<Cell>(m_length));
    }

    // return the number of live neighbors
    int checkNeighbors(const Coord_type xPos, const Coord_type yPos)
    {
        auto& x{ xPos };
        auto& y{ yPos };

        /**
         * check 8 neighbors of a cell
         *      [x-1, y-1] [x, y-1] [x+1, y-1]
         *      [x-1, y  ] [x, y  ] [x+1, y  ]
         *      [x-1, y+1] [x, y+1] [x+1, y+1]
        */

        return {
              checkState(x-1, y-1) + checkState(x, y-1) + checkState(x+1, y-1) 
            + checkState(x-1, y)                        + checkState(x+1, y)
            + checkState(x-1, y+1) + checkState(x, y+1) + checkState(x+1, y+1)
        };
    }

    constexpr bool isInBound(const Coord_type xPos, const Coord_type yPos) const
    {
        return {
            (xPos >= 0 && xPos < m_length)
            && (yPos >= 0 && yPos < m_width)
        };
    }

    constexpr bool checkState(const Coord_type xPos, const Coord_type yPos) const
    {
        if (!isInBound(xPos, yPos))             // boundary condition: return dead state
            return Cell::s_dead_state;

        return (*this)(xPos, yPos).isLive();
    }

    constexpr bool checkState(const Cell& cell) const
    {
        return cell.isLive();
    }

    const auto& data() const { return m_grid; }
    auto& getCell(int col, int row) { return (*this)(col, row); }
    const auto& getCell(int col, int row) const { return (*this)(col, row); }
    const Coord_type getWidth() const { return m_width; }
    const Coord_type getLength() const { return m_length; }

    // return length, width
    const std::pair<int, int> getDimension() const { return { m_length, m_width }; }

    Cell& operator() (const Coord_type xPos, const Coord_type yPos)
    {
        if (!isInBound(xPos, yPos))
            throw std::range_error{ "out of range" };

        return m_grid[yPos][xPos];
    }
    const Cell& operator() (const Coord_type xPos, const Coord_type yPos) const
    {
        if (!isInBound(xPos, yPos))
            throw std::range_error{ "out of range" };

        return m_grid[yPos][xPos];
    }

    friend std::ostream& operator<<(std::ostream& out, const Grid& grid)
    {
        for (auto& row : grid.m_grid)
        {
            for (auto& cell : row)
            {
                switch (cell.isLive())
                {
                case true:  std::cout << "█"; break;
                case false: std::cout << " "; break;
                }
            }
            std::cout << '\n';
        }
        return out;
    }
};


#endif