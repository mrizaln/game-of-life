#include "application.hpp"
#include "game.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <string>

int main(int argc, char** argv)
{
    CLI::App app{ "Conway's game of life simulation renderer" };

    int         length   = 360;
    int         width    = 480;
    std::size_t delay    = 0;
    float       density  = 0.3f;
    bool        pause    = false;
    bool        noVsync  = false;
    bool        debug    = false;
    auto        strategy = Grid::UpdateStrategy::INTERLEAVED;

    app.add_option("-l,--length", length, "The length of the world grid")->required(true);
    app.add_option("-w,--width", width, "The width of the world grid")->required(true);
    app.add_option("-t,--delay", delay, "Delay for each update (in seconds)");
    app.add_option("-d,--density", density, "Start density")->check(CLI::Range(0.0f, 1.0f));
    app.add_flag("--paused", pause, "Start the simulation on a paused state");
    app.add_flag("--no-vsync", noVsync, "Turn off vsync");
    app.add_flag("--debug", debug, "Print debugging info");

    app.add_option("--update-strategy", strategy, "The strategy to be used on updates (multithreaded)")
        ->transform(CLI::CheckedTransformer(Grid::s_updateStrategyMap, CLI::ignore_case));

    CLI11_PARSE(app, argc, argv);

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
    }

    Application application{ {
        .m_windowWidth    = 800,
        .m_windowHeight   = 600,
        .m_gridWidth      = length,
        .m_gridHeight     = width,
        .m_startDensity   = density,
        .m_delay          = delay,    // s to ms
        .m_vsync          = !noVsync,
        .m_updateStrategy = strategy,
    } };
    application.run();
}
