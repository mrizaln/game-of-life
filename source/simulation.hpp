#ifndef SIMULATION_HPP_WHFEDHF3
#define SIMULATION_HPP_WHFEDHF3

#include "game.hpp"

#include <sync_cpp/sync.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

// Enter a lazy state if the simulation is paused: function provide to the Simulation::run() will be called every
// `s_lazyUpdateTime` ms instead of every `m_delay` ms
class Simulation
{
public:
    using Duration = std::chrono::milliseconds;

    Simulation(
        Grid::Coord_type     gridWidth,
        Grid::Coord_type     gridHeight,
        Grid::UpdateStrategy updateStrategy,
        std::size_t          delay
    )
        : m_grid{ m_mutex, gridWidth, gridHeight, updateStrategy }
        , m_delay{ delay }
        , m_ignoreDelay{ false }
        , m_paused{ false }
        , m_wakeFlag{ false }
    {
    }

    Simulation(const Simulation&)            = delete;
    Simulation& operator=(const Simulation&) = delete;
    Simulation(Simulation&&)                 = delete;
    Simulation& operator=(Simulation&&)      = delete;

    ~Simulation() { stop(); }

    // run the simulation and hook the given function to the simulation loop.
    // this function returns immediately.
    template <std::invocable<Grid&> Fn>
    void launch(Fn&& fn)
    {
        m_thread = std::jthread{ [this, fn = std::forward<Fn>(fn)](const std::stop_token& st) {
            while (!st.stop_requested()) {
                auto tpsCounter{ m_tickRateCounter.update() };

                m_grid.write([&](Grid& grid) {
                    if (!m_paused) {
                        grid.updateState();
                    }
                    fn(grid);
                });

                // sleep routine
                if (bool paused = m_paused.load(); paused || !m_ignoreDelay) {
                    Duration         delay{ m_delay.load() };
                    std::unique_lock lock{ m_mutex };
                    delay = paused ? s_lazyUpdateTime : delay;
                    m_cv.wait_until(lock, tpsCounter.m_currTime + delay, [this] { return m_wakeFlag.load(); });
                    m_wakeFlag = false;
                }
            }
        } };
    }

    void forceUpdate()
    {
        wakeUp();
        if (m_paused) {
            m_grid.write(&Grid::updateState);
        }
    }

    // wake up the thread if the thread is sleeping
    void wakeUp()
    {
        m_wakeFlag = true;
        m_cv.notify_one();
    }

    void stop()
    {
        m_thread.request_stop();
        wakeUp();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void setDelay(std::size_t delay)
    {
        if (m_delay > delay) {
            m_delay = delay;
            wakeUp();
        } else if (m_delay != delay) {
            m_delay = delay;
        }
    }

    std::size_t getDelay() const { return m_delay; }
    void        setPause(bool pause) { m_paused = pause; }
    bool        isPaused() const { return m_paused; }
    bool        togglePause() { return m_paused = !m_paused; }
    void        setIgnoreDelay(bool ignore) { m_ignoreDelay = ignore; }
    bool        isIgnoringDelay() const { return m_ignoreDelay; }
    bool        toggleIgnoreDelay() { return m_ignoreDelay = !m_ignoreDelay; }

    decltype(auto) write(auto&&... args) { return m_grid.write(std::forward<decltype(args)>(args)...); }
    decltype(auto) read(auto&&... args) const { return m_grid.read(std::forward<decltype(args)>(args)...); }

    float getTickRate() const { return m_tickRateCounter.get(); }

private:
    static constexpr Duration s_lazyUpdateTime{ 33 };    // about 30 tps

    class TickRateCounter
    {
    private:
        struct [[nodiscard]] Inserter
        {
            Inserter(TickRateCounter& counter)
                : m_currTime{ std::chrono::steady_clock::now() }
                , m_counter{ counter }
            {
            }
            ~Inserter() { m_counter.add((std::chrono::steady_clock::now() - m_currTime).count()); }

            std::chrono::time_point<std::chrono::steady_clock> m_currTime;
            TickRateCounter&                                   m_counter;
        };

    public:
        TickRateCounter() = default;
        Inserter update() { return Inserter{ *this }; }
        float    get() const { return m_tickRate; }

    private:
        void add(long time)
        {
            m_tickRateHistory[m_index] = 1e9F / static_cast<float>(time);
            m_index                    = (m_index + 1) % m_tickRateHistory.size();

            m_tickRate = std::accumulate(m_tickRateHistory.begin(), m_tickRateHistory.end(), 0.0F)
                         / static_cast<float>(m_tickRateHistory.size());
        }

        std::array<float, 8> m_tickRateHistory{ 0 };
        std::atomic<float>   m_tickRate{ 0 };
        std::size_t          m_index{ 0 };
    };

    spp::Sync<Grid, std::mutex, false> m_grid;

    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
    std::jthread            m_thread;

    std::atomic<std::size_t> m_delay;
    std::atomic<bool>        m_ignoreDelay;
    std::atomic<bool>        m_paused;
    std::atomic<bool>        m_wakeFlag;

    TickRateCounter m_tickRateCounter;
};

#endif /* end of include guard: SIMULATION_HPP_WHFEDHF3 */
