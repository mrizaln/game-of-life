#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <iostream>
#include <string>

// #define TIMER_FORCE_PRINT

namespace util
{
    class Timer
    {
    public:
        // type aliases
        using clock_type  = std::chrono::steady_clock;
        using second_type = std::chrono::duration<double, std::ratio<1, 1000>>;

        inline static bool s_doPrint{ true };

    public:
        Timer(const std::string& name, bool doAutoPrint = true)
            : m_name{ name }
            , m_doAutoPrint{ doAutoPrint }
        {
        }

        // #if (!defined(NDEBUG) or defined(TIMER_FORCE_PRINT)) and !defined(TIMER_SUPPRESS_PRINT)
        ~Timer()
        {
            if (s_doPrint && m_doAutoPrint)
                print();
        }
        // #endif

        void reset()
        {
            m_beginning = clock_type::now();
        }

        double elapsed() const
        {
            return std::chrono::duration_cast<second_type>(clock_type::now() - m_beginning).count();
        }

        void print()
        {
            std::cout << m_name << ": " << elapsed() << " ms\n";
        }

    private:
        const std::string                   m_name;
        const bool                          m_doAutoPrint;
        std::chrono::time_point<clock_type> m_beginning{ clock_type::now() };
    };
}

#endif /* ifndef TIMER_HPP */
