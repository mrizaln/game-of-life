#ifndef RENDER_HPP
#define RENDER_HPP

// #define DEBUG

#include <algorithm>    // std::min, std::max
#include <atomic>
#include <cmath>    // std::*math_functions*
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <camera_header/camera.hpp>
#include <tile/tile.hpp>
#include <tile/grid_tile.hpp>

#include <type_name/type_name.hpp>

#include "./game.hpp"

namespace RenderEngine
{
    GLFWwindow* initializeWindow(const std::string&, bool);
    void        reset();

    void framebuffer_size_callback(GLFWwindow*, int, int);
    void cursor_position_callback(GLFWwindow*, double, double);
    void scroll_callback(GLFWwindow*, double, double);
    void mouse_button_callback(GLFWwindow*, int, int, int);
    void key_callback(GLFWwindow*, int, int, int, int);

    int  shouldClose();
    void resetCamera(bool = false);
    void updateCamera();
    void processMouseButton();
    void processInput(GLFWwindow*);
    bool updateStates(int, int, int, int);
    void updateDeltaTime();

    //=================================================================================================

    namespace configuration
    {
        int   width{ 800 };
        int   height{ 600 };
        float aspectRatio{ 800 / static_cast<float>(600) };

        std::string title{ "Game of Life" };
    }

    namespace timing
    {
        float lastFrame{};
        float deltaTime{};
        float sumTime{};

        float delay{};
    }

    namespace mouse
    {
        float lastX{};
        float lastY{};
        bool  firstMouse{ true };
        bool  captureMouse{ false };

        bool leftButtonPressed{};
        bool rightButtonPressed{};
    }

    namespace camera
    {
        Camera* camera{};
    }

    namespace simulation
    {
        bool pause{ false };
        bool stop{ false };

        enum GridMode
        {
            OFF,
            ON,
            AUTO,

            numOfGridModes,
        };
        GridMode gridMode{ AUTO };
    }

    namespace data
    {
        Grid*       gridPtr{};
        GLFWwindow* window{};
        Tile*       gridBorderTile{};
        GridTile*   gridTile{};

        namespace thread
        {
            std::optional<std::thread> workerThread{ std::nullopt };
            std::atomic<bool>          done{ false };
            bool                       block{ true };
            bool                       doUpdate{ false };

            void blockHere()
            {
                if (!workerThread.has_value()) {
                    return;
                }

                try {
                    data::thread::workerThread->join();
                } catch (std::exception& e) {
                    std::cerr << "\nThread Error: " << e.what() << " (" << GET_TYPE_NAME_FROM_VAR(e) << ")\n\n";
                }
            }
        }
    }

    namespace fps_counter
    {
        int         counter{ 0 };
        const float timeInterval{ 1.0f };    // print every this time interval
        float       sum{ 0 };

        void updateFps()
        {
            counter++;
            if (sum >= timeInterval) {
                // std::cout << "\n\033[1A\033[2KFPS: " << 1 / (sum / interval);
                auto title{ configuration::title + " [FPS: " + std::to_string(1 / (sum / counter)).substr(0, 5) + "]" };
                glfwSetWindowTitle(data::window, title.c_str());
                sum     = 0.0f;
                counter = 0;
            }
            sum += timing::deltaTime;
        }
    }

    //=================================================================================================

    int initialize(Grid& grid, float delay = 1e-5f, bool vsync = true)
    {
        data::window = initializeWindow("Game of Life", vsync);
        if (!data::window) {
            std::cout << "There's an error when creating window.\n";
            glfwTerminate();
            return -1;
        }

        data::gridPtr = &grid;

        data::gridBorderTile = new Tile{
            1.0f,
            "./resources/shaders/shader.vs",
            "./resources/shaders/shader.fs",
            "./resources/textures/grid.png",
            GL_LINEAR,
            GL_LINEAR,
            GL_REPEAT,
            { data::gridPtr->getLength() / 2.0f, data::gridPtr->getWidth() / 2.0f, 0.0f },    // pos
            { 1.0f, 1.0f, 1.0f },                                                             // color
            { data::gridPtr->getLength(), data::gridPtr->getWidth(), 0.0f }                   // scale
        };

        data::gridBorderTile->getPlane().multiplyTexCoords(data::gridPtr->getLength(), data::gridPtr->getWidth());

        data::gridTile = new GridTile{
            data::gridPtr->getLength(),
            data::gridPtr->getWidth(),
            "./resources/shaders/gridShader.vert",
            "./resources/shaders/gridShader.frag",
            "./resources/textures/cell.png",
            GL_LINEAR,
            GL_LINEAR,
            GL_REPEAT,
            { data::gridPtr->getLength() / 2.0f, data::gridPtr->getWidth() / 2.0f, 0.0f },    // pos
            { 1.0f, 1.0f, 1.0f },                                                             // color
            { data::gridPtr->getLength(), data::gridPtr->getWidth(), 0.0f }                   // scale
        };

        camera::camera        = new Camera{};
        camera::camera->speed = 100.0f;    // 10 tiles per movement
        resetCamera(true);

        timing::delay = delay;

        return 0;
    }

    void reset()
    {
        data::thread::blockHere();

        delete data::gridTile;
        delete data::gridBorderTile;
        delete camera::camera;
    }

    void drawBorder(const glm::mat4& projMat, const glm::mat4& viewMat)
    {
        bool shouldDraw{};
        switch (simulation::gridMode) {
        case simulation::GridMode::ON:
            shouldDraw = true;
            break;

        case simulation::GridMode::AUTO: {
            const float   xDelta{ configuration::width / camera::camera->zoom };    // (number of cells per width of screen)/2
            constexpr int minNumberOfCellsPerScreenHeight{ 100 };                   // change accordingly
            if (xDelta <= minNumberOfCellsPerScreenHeight / 2.0f) {
                shouldDraw = true;
            }
        } break;

        default:
            shouldDraw = false;
        }

        if (!shouldDraw) {
            return;
        }

        // use shader
        data::gridBorderTile->m_shader.use();

        // upload view and projection matrix
        data::gridBorderTile->m_shader.setMat4("view", viewMat);
        data::gridBorderTile->m_shader.setMat4("projection", projMat);

        // model matrix
        glm::mat4 model{ 1.0f };
        model = glm::translate(model, data::gridBorderTile->m_position);
        model = glm::scale(model, data::gridBorderTile->m_scale);
        data::gridBorderTile->m_shader.setMat4("model", model);

        // change color if simulation::pause
        if (!simulation::pause) {
            data::gridBorderTile->m_color = { 1.0f, 1.0f, 1.0f };
        } else {
            data::gridBorderTile->m_color = { 0.7f, 1.0f, 0.7f };
        }

        // draw
        data::gridBorderTile->draw();
    }

    void drawGrid(const glm::mat4& projMat, const glm::mat4& viewMat, int xStart, int xEnd, int yStart, int yEnd, bool update = false)
    {
        // idk
        data::gridTile->m_shader.use();
        data::gridTile->m_shader.setMat4("view", viewMat);
        data::gridTile->m_shader.setMat4("projection", projMat);

        glm::mat4 model{ 1.0f };
        model = glm::translate(model, data::gridTile->m_position);
        model = glm::scale(model, data::gridTile->m_scale);

        data::gridTile->m_shader.setMat4("model", model);

        // data::gridTile->setState(data::gridPtr->data(), xStart, xEnd, yStart, yEnd);
        // data::gridTile->draw();

        // data::gridTile->getPlane().customizeIndices(data::gridPtr->data(), xStart, xEnd, yStart, yEnd);
        // data::gridTile->getPlane().swapIndices();
        data::gridTile->draw();
    }

    void render()
    {
        // background color
        if (!simulation::pause) {
            glClearColor(0.1f, 0.1f, 0.11f, 1.0f);
        } else {
            glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // orthogonal frustum
        // clang-format off
        const float left  { -configuration::width  / static_cast<float>(camera::camera->zoom) };
        const float right {  configuration::width  / static_cast<float>(camera::camera->zoom) };
        const float bottom{ -configuration::height / static_cast<float>(camera::camera->zoom) };
        const float top   {  configuration::height / static_cast<float>(camera::camera->zoom) };
        const float near  { -10.0f };
        const float far   {  10.0f };
        // clang-format on

        // projection matrix
        auto projMat{ glm::ortho(left, right, bottom, top, near, far) };

        // view matrix
        auto viewMat{ camera::camera->getViewMatrix() };

        const auto& xPos{ camera::camera->position.x };
        const auto& yPos{ camera::camera->position.y };
        const auto& [width, height]{ data::gridPtr->getDimension() };

        constexpr float offset{ 1.5f };
        // culling
        // clang-format off
        const int rowLeftBorder  { static_cast<int>( xPos + left       - offset > 0      ? xPos + left   - offset : 0) };
        const int rowRightBorder { static_cast<int>( xPos + right + 1  + offset < width  ? xPos + right  + offset : width) };
        const int colTopBorder   { static_cast<int>( yPos + top        + offset < height ? yPos + top    + offset : height) };
        const int colBottomBorder{ static_cast<int>( yPos + bottom + 1 - offset > 0      ? yPos + bottom - offset : 0) };
        // clang-format on

        // update states
        bool shouldUpdateFrame{ updateStates(rowLeftBorder, rowRightBorder, colBottomBorder, colTopBorder) };    // update grid if timing::sumTime > delay
        // if (!simulation::pause) {
        //     data::gridPtr->updateState();
        // }

        // grid
        drawBorder(projMat, viewMat);

        // new grid
        drawGrid(projMat, viewMat, rowLeftBorder, rowRightBorder, colBottomBorder, colTopBorder);
        // drawGrid(projMat, viewMat, left, right, top, bottom);

        glfwSwapBuffers(data::window);
        updateCamera();

        // input
        processInput(data::window);
        processMouseButton();
        glfwPollEvents();

        // delta time
        updateDeltaTime();

        // print deltaTime
#ifdef DEBUG
        std::cout << timing::deltaTime << '\n';
#endif

        // fps
        fps_counter::updateFps();
    }

    //=================================================================================================

    GLFWwindow* initializeWindow(const std::string& windowName, bool vsync)
    {
        // initialize glfw
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // window creation
        GLFWwindow* window{ glfwCreateWindow(configuration::width, configuration::height, windowName.c_str(), NULL, NULL) };
        if (!window) {
            std::cerr << "Failed to create GLFW window\n";
            return nullptr;
        }
        glfwMakeContextCurrent(window);

        // glad: load all OpenGL function pointers
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "Failed to initialize glad\n";
            return nullptr;
        }

        // set callbacks
        //--------------
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetScrollCallback(window, scroll_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetKeyCallback(window, key_callback);

        // enable depth test
        // glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        // turn off vsync
        if (!vsync) {
            glfwSwapInterval(0);
        }

        return window;
    }

    // window resize callback
    void framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
        configuration::aspectRatio = width / static_cast<float>(height);
        configuration::width       = width;
        configuration::height      = height;
        // std::cout << "aspect ratio: " << configuration::aspectRatio << '\n';
    }

    // cursor position callback
    void cursor_position_callback(GLFWwindow* window, double xPos, double yPos)
    {
        if (!mouse::captureMouse) {
            return;
        }

        if (mouse::firstMouse) {
            mouse::lastX      = xPos;
            mouse::lastY      = yPos;
            mouse::firstMouse = false;
        }

        float xOffset{ static_cast<float>(xPos) - mouse::lastX };
        float yOffset{ mouse::lastY - static_cast<float>(yPos) };

        // instead of normal camera (yaw, pitch, etc.), i'm using this as sliding movement
        // camera::camera->processMouseMovement(xOffset, yOffset);
        camera::camera->position.x += xOffset * camera::camera->speed / 200.0f;
        camera::camera->position.y += yOffset * camera::camera->speed / 200.0f;

        mouse::lastX = xPos;
        mouse::lastY = yPos;
    }

    // scroll callback
    void scroll_callback(GLFWwindow* window, double xOffset, double yOffset)
    {
        camera::camera->processMouseScroll(static_cast<float>(yOffset), Camera::ZOOM);
        // std::cout << camera::camera->zoom << '\n';
    }

    // mouse button callback
    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        static bool simulationStateBefore{};

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            mouse::leftButtonPressed = true;
            simulationStateBefore    = simulation::pause;    // save simulation state before (paused or not)
            simulation::pause        = true;                 // pause updating states when button pressed
        } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            mouse::leftButtonPressed = false;
            // simulation::pause = false;                   // resume updating states when button released
            simulation::pause = simulationStateBefore;    // restore simulation state before (paused or not)
        }

        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
            mouse::rightButtonPressed = true;
            simulationStateBefore     = simulation::pause;    // save simulation state before (paused or not)
            simulation::pause         = true;                 // pause updating states when button pressed
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
            mouse::rightButtonPressed = false;
            // simulation::pause = false;                   // resume updating states when button released
            simulation::pause = simulationStateBefore;    // restore simulation state before (paused or not)
        }

        double xPos{};
        double yPos{};
        glfwGetCursorPos(window, &xPos, &yPos);

#ifdef DEBUG
        std::cout << "cursor position in window" << xPos << '/' << yPos << '\n';
#endif
    }

    // key press callback (for 1 press)
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        // close window
        if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // toggle capture mouse
        if (key == GLFW_KEY_C && action == GLFW_PRESS) {
            // toggle
            mouse::captureMouse = !mouse::captureMouse;

            if (mouse::captureMouse) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                mouse::firstMouse = true;
            }
        }

        // center camera
        if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS) {
            resetCamera();
        }

        // pause-unpause
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
            simulation::pause = !simulation::pause;
        }

        // temporarily unpause
        static bool pauseBefore{};
        if (key == GLFW_KEY_U && action == GLFW_PRESS) {

            data::thread::block = true;

            // data::gridPtr->updateState();
            pauseBefore = simulation::pause;
            if (pauseBefore == true) {
                simulation::pause = false;
            }
        } else if (key == GLFW_KEY_U && action == GLFW_RELEASE) {
            simulation::pause = pauseBefore;
        }

        // fit to screen
        if (key == GLFW_KEY_F && action == GLFW_PRESS) {
            resetCamera();
            camera::camera->zoom = 2 / 1.0f * configuration::width / float(data::gridPtr->getLength());

            constexpr int maxCol{ 75 };

            auto& Z{ camera::camera->zoom };
            auto  z{ std::max({
                2.0f * configuration::width / float(data::gridPtr->getLength()),    // all grid visible
                4.0f * configuration::width / float(data::gridPtr->getLength()),    // half column
                // 4/1.1f * configuration::height/float(numOfRows),     // half row

                2.0f * configuration::width / float(maxCol)    // show only number of maxCol
            }) };
            auto  n{ -std::log(Z / z) / std::log(1.1f) };
            camera::camera->speed = 100.0f * std::pow(1.1f, n / 2);
        }

        // cycle through the grid modes
        if (key == GLFW_KEY_G && action == GLFW_PRESS) {
            simulation::gridMode = static_cast<simulation::GridMode>((simulation::gridMode + 1) % simulation::GridMode::numOfGridModes);
        }

        // reset -> set all cell in grid to dead
        if (key == GLFW_KEY_R && action == GLFW_PRESS) {
            // data::gridPtr->reset();

            // if (camera::follow)         // if camera::follow is set to camera::Mode::FOLLOW_NONE, don't reset
            //     resetCamera();
            data::thread::blockHere();

            data::gridPtr->clear();
        }

        // populate grid with new cells
        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            data::thread::blockHere();

            data::gridPtr->clear();
            auto density{ data::gridPtr->getRandomProbability() * 0.6f + 0.2f };
            data::gridPtr->populate(density);

            std::cout << "populate with density: " << density << '\n';
        }
    }

    int shouldClose()
    {
        if (data::window) {
            return simulation::stop = glfwWindowShouldClose(data::window);
        } else {
            return false;
        }
    }

    void updateCamera()
    {
        float delay{ timing::delay };

        // if lag
        if (timing::delay == 0) {
            delay = 1e-5;
        }
        if (timing::deltaTime > timing::delay) {
            delay = timing::deltaTime;
        }
    }

    void resetCamera(bool resetZoom)
    {
        auto& [numOfColumns, numOfRows]{ data::gridPtr->getDimension() };

        // center camera
        camera::camera->position.x = numOfColumns / 2.0f;
        camera::camera->position.y = numOfRows / 2.0f;

        constexpr int maxCol{ 75 };

        if (resetZoom) {
            camera::camera->zoom = std::max({
                2.0f * configuration::width / float(numOfColumns),    // all grid visible
                4.0f * configuration::width / float(numOfColumns),    // half column
                // 4/1.1f * configuration::height/float(numOfRows),     // half row

                2.0f * configuration::width / float(maxCol)    // show only number of maxCol
            });
        }
    }

    // return true if thread has done
    bool updateStates(int xStart, int xEnd, int yStart, int yEnd)
    {
        using namespace data::thread;

        if (timing::sumTime >= timing::delay) {
            doUpdate        = true;
            timing::sumTime = 0.0f;
        }

        if (!workerThread.has_value()) {
            workerThread.emplace([&](int x1, int x2, int y1, int y2) {
                data::gridTile->getPlane().customizeIndices(data::gridPtr->data(), x1, x2, y1, y2);
                if (!simulation::pause && doUpdate) {
                    data::gridPtr->updateState();
                    doUpdate = false;
                }
                done = true;
            },
                                 xStart, xEnd, yStart, yEnd);
        } else {
            if (done || block) {
                blockHere();
                // workerThread->join();
                workerThread.reset();

                done  = false;
                block = false;
                return true;
            }
        }

        // std::cout << "Not join\n";
        return false;
    }

    void processMouseButton()
    {
        double xPos{};
        double yPos{};
        glfwGetCursorPos(data::window, &xPos, &yPos);    // get position relative to top-left corner (in px)

        yPos = configuration::height - yPos;

        static double xPos_last{ xPos };
        static double yPos_last{ yPos };

        float xDelta{ configuration::width / camera::camera->zoom };
        float yDelta{ configuration::height / camera::camera->zoom };

        constexpr float xOffset{ 0.0f };
        constexpr float yOffset{ 0.0f };
        // current pos
        int x{ static_cast<int>(camera::camera->position.x - (xDelta - 2.0f * xPos / camera::camera->zoom) + xOffset) };    // col
        int y{ static_cast<int>(camera::camera->position.y - (yDelta - 2.0f * yPos / camera::camera->zoom) + yOffset) };    // row

        // map top-left to bottom-left and fix offset
        // x = x + xOffset;
        // y = data::gridPtr->getWidth() - y - yOffset;

        if (mouse::captureMouse) {
            x = camera::camera->position.x;
            y = camera::camera->position.y;
        }

        // last pos
        int x_last{ static_cast<int>(camera::camera->position.x - (xDelta - 2.0f * xPos_last / camera::camera->zoom) + xOffset) };    // last col
        int y_last{ static_cast<int>(camera::camera->position.x - (yDelta - 2.0f * yPos_last / camera::camera->zoom) + yOffset) };    // last row

        // for linear interpolation
        float grad{ static_cast<float>((y - yPos_last) / static_cast<float>(x - xPos_last)) };

        // TODO: implement an interpolation when cursor is moved too fast that it skip some cells

        if (mouse::leftButtonPressed) {
            // linear interpolation [not working]
            // for (int col{ xPos_last }; col < x; ++col)
            // {
            //     int row{ -(grad * (col - xPos_last) + yPos_last) };       // flip back up-down
            //     if (data::gridPtr->isInBound(col, row))
            //     {
            //         auto& cell{ data::gridPtr->getCell(col, row) };
            //         cell.setLive();
            //         cell.update();
            //     }
            // }
            xPos_last = xPos;
            yPos_last = yPos;

            // current click
            if (data::gridPtr->isInBound(x, y)) {
                auto& cell{ data::gridPtr->getCell(x, y) };
                cell.setLive();
                cell.update();

                // block thread
                data::thread::block = true;
            }
        } else if (mouse::rightButtonPressed) {
            // current click
            if (data::gridPtr->isInBound(x, y)) {
                auto& cell{ data::gridPtr->getCell(x, y) };
                cell.setDead();
                cell.update();

                // block thread
                data::thread::block = true;
            }
        }

        // auto coutBefore{ std::cout.flags() };
        // std::cout << "cursor pos: [" << std::cout.precision(2) << x << ", " << y << "]\n";
        // std::cout.flags(coutBefore);
    }

    // for continuous input
    void processInput(GLFWwindow* window)
    {
        float xPos{ camera::camera->position.x };
        float yPos{ camera::camera->position.y };
        float xDelta{ configuration::width / camera::camera->zoom };
        float yDelta{ configuration::height / camera::camera->zoom };
        auto& [numOfColumns, numOfRows]{ data::gridPtr->getDimension() };

        // camera movement
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            if ((yPos + yDelta) < numOfRows) {
                camera::camera->moveCamera(Camera::UPWARD, timing::deltaTime);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            if ((yPos - yDelta) > 0) {
                camera::camera->moveCamera(Camera::DOWNWARD, timing::deltaTime);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            if ((xPos + xDelta) < numOfColumns) {
                camera::camera->moveCamera(Camera::RIGHT, timing::deltaTime);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            if ((xPos - xDelta) > 0) {
                camera::camera->moveCamera(Camera::LEFT, timing::deltaTime);
            }
        }

        // speed
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
            camera::camera->speed *= 1.01;
        } else if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
            camera::camera->speed /= 1.01;
        }

        // delay
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
            timing::delay *= 1.01f;
        } else if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
            timing::delay /= 1.01f;
            if (timing::delay < timing::deltaTime) {
                timing::delay = timing::deltaTime;
            }
        }
    }

    // record frame draw time
    void updateDeltaTime()
    {
        float currentFrame{ static_cast<float>(glfwGetTime()) };
        timing::deltaTime = currentFrame - timing::lastFrame;
        timing::lastFrame = currentFrame;
        timing::sumTime += timing::deltaTime;
    }
}

#endif
