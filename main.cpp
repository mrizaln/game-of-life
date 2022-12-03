#include <string>
#include <thread>
#include <chrono>
#include <sstream>

#include "./game.h"
#include "./render.h"


int main(int argc, char** argv)
{
    // help
    if (argc > 1)
    {
        std::string arg{ argv[1] };
        if (arg == "-h")
        {
            std::cout << "Usage: <delay> <startDensity> <length/width>\n";
            return 0;
        }
    }

    float delay{ 0.001 };
    if (argc > 1)
    {
        std::string arg{ argv[1] };
        std::stringstream ss { arg };
        ss >> delay;
    }

    float density{ 0.3 };
    if (argc > 2)
    {
        std::string arg{ argv[2] };
        std::stringstream ss { arg };
        ss >> density;
    }

    int length{ 50 }, width{ 20 };
    if (argc > 3)
    {
        char tmp{};
        std::string arg{ argv[3] };
        std::stringstream ss { arg };
        ss >> length;
        ss >> tmp;
        ss >> width;
    }

    Grid grid{ length, width };
    grid.populate(density);

    // std::cout << "\033[H";          // go to 0,0
    // std::cout << "\033[0J";         // clear all
    // while(true)
    // {
    //     std::cout << "\033[H";      // to to 0,0
    //     // std::cout << "\033[0J";
    //     std::cout << grid << '\n';
    //     grid.updateState();

    //     using namespace std::chrono_literals;
    //     std::this_thread::sleep_for(1s * delay);
    // }

    RenderEngine::initialize(grid, delay);
    while (!RenderEngine::shouldClose())
    {
        RenderEngine::render();
    }
}