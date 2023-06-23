#include <string>
#include <sstream>
#include <iostream>

#include <util/timer.hpp>

#include "./game.hpp"
#include "./render.hpp"

int main(int argc, char** argv)
{
    // help
    if (argc > 1) {
        std::string arg{ argv[1] };
        if (arg == "-h") {
            std::cout << "Usage: <delay> <startDensity> <length/width>\n";
            return 0;
        }
    }

    float delay{ 0.001 };
    if (argc > 1) {
        std::stringstream ss{ std::string{ argv[1] } };
        ss >> delay;
    }

    float density{ 0.3 };
    if (argc > 2) {
        std::stringstream ss{ std::string{ argv[2] } };
        ss >> density;
    }

    int length{ 50 }, width{ 20 };
    if (argc > 3) {
        char              tmp{};
        std::stringstream ss{ std::string{ argv[3] } };
        ss >> length;
        ss >> tmp;
        ss >> width;
    }

    bool pause{ false };
    if (argc > 4) {
        std::stringstream ss{ std::string{ argv[4] } };
        ss >> pause;
    }

    bool vsync{ true };
    if (argc > 5) {
        std::stringstream ss{ std::string{ argv[4] } };
        ss >> vsync;
    }

    std::cout << "Creating and populating grid... ";
    Grid grid{ length, width };
    grid.populate(density);
    std::cout << "Done\n";

    // enable timer print
    util::Timer::s_doPrint = false;

    RenderEngine::simulation::pause = pause;

    if (RenderEngine::initialize(grid, delay, vsync)) {
        return -1;
    }

    while (!RenderEngine::shouldClose()) {
        RenderEngine::render();
    }
    RenderEngine::reset();
}
