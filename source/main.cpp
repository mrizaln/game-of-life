#include <string>
#include <sstream>
#include <iostream>

#include "renderer.hpp"
#include "timer.hpp"
#include "game.hpp"

int main(int argc, char** argv)
{
    // help
    if (argc > 1) {
        std::string arg{ argv[1] };
        if (arg == "-h") {
            std::cout << "Usage: <delay> <startDensity> <length/width> <pause> <vsync> <print>\n";
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
        std::stringstream ss{ std::string{ argv[5] } };
        ss >> vsync;
    }

    bool print{ false };
    if (argc > 6) {
        std::stringstream ss{ std::string{ argv[6] } };
        ss >> print;
    }
    std::cout << "Creating and populating grid... ";
    Grid grid{ length, width };
    grid.populate(density);
    std::cout << "Done\n";

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    WindowManager::createInstance();
    auto window = WindowManager::createWindow("Game of Life", 800, 600);
    if (!window.has_value()) {
        std::cerr << "Failed to create window";
    }

    // enable timer print
    util::Timer::s_doPrint = print;

    // limit the scope of Renderer so that it's destructor is called before glfwTerminate
    {
        window->setVsync(vsync);
        window->useHere();

        Renderer renderer{ std::move(window).value(), grid, delay };
        renderer.run();
    }

    WindowManager::destroyInstance();
    glfwTerminate();
}
