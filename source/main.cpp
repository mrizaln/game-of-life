#include "application.hpp"
#include "timer.hpp"

#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <spdlog/spdlog.h>
#include <string>

int main(int argc, char** argv)
{
    CLI::App app{ "Conway's game of life simulation renderer" };

    int   length     = 360;
    int   width      = 480;
    float delay      = 1e-5f;
    float density    = 0.3f;
    bool  pause      = false;
    bool  noVsync    = false;
    bool  timerPrint = false;
    bool  debug      = false;

    app.add_option("-l,--length", length, "The length of the world grid")->required(true);
    app.add_option("-w,--width", width, "The width of the world grid")->required(true);
    app.add_option("-t,--delay", delay, "Delay for each update (in seconds)");
    app.add_option("-d,--density", density, "Start density (0.0 - 1.0)");
    app.add_flag("--paused", pause, "Start the simulation on a paused state");
    app.add_flag("--no-vsync", noVsync, "Turn off vsync");
    app.add_flag("--timer", timerPrint, "Output timer results");
    app.add_flag("--debug", debug, "Print debugging info");

    CLI11_PARSE(app, argc, argv);

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
    }

    // enable timer print
    util::Timer::s_doPrint = timerPrint;

    // limit the scope of Renderer so that it's destructor is called before glfwTerminate
    Application application{ {
        .m_windowWidth  = 800,
        .m_windowHeight = 600,
        .m_gridWidth    = length,
        .m_gridHeight   = width,
        .m_startDensity = density,
        .m_delay        = static_cast<std::size_t>(delay * 1000),    // s to ms
        .m_vsync        = !noVsync,
    } };
    application.run();
}
