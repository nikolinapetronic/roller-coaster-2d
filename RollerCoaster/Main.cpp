#define _USE_MATH_DEFINES
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Util.h"
#include "Scene.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>

int main()
{
    // GLFW inicijalizacija
    if (!glfwInit()) {
        return endProgram("GLFW nije uspio da se inicijalizuje.");
    }

    std::cout << "GLFW uspjesno inicijalizovan." << std::endl;

    // postavljanje OpenGL 3.3 Core Profile 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // dimenzije ekrana - fullscreen
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    int screenWidth = mode->width;
    int screenHeight = mode->height;

    // kreiranje fullscreen prozora
    GLFWwindow* window = glfwCreateWindow(
        screenWidth,
        screenHeight,
        "Roller Coaster 2D",
        monitor,    // fullscreen
        NULL
    );
    if (window == NULL) {
        return endProgram("Prozor nije uspio da se kreira.");
    }

    // povezivanje OpenGL konteksta sa prozorom
    glfwMakeContextCurrent(window);

    glfwSwapInterval(0);  // iskljucen Vsync, da u potpunosti kontrolisemo fps

    // GLEW inicijalizacija
    if (glewInit() != GLEW_OK) {
        return endProgram("GLEW nije uspio da se inicijalizuje.");
    }

    std::cout << "GLEW uspjesno inicijalizovan." << std::endl;

    // ukljucivanje alfa kanala za providnost
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // postavljanje boje pozadine
    glClearColor(0.39f, 0.74f, 0.97f, 1.0f);

    // inicijalizacija scene
    initScene(window, screenWidth, screenHeight);

    // FPS limiter i delta time
    const double TARGET_FPS = 75.0;
    const double FRAME_DURATION = 1.0 / TARGET_FPS;

    double lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double frameStart = glfwGetTime();
        double deltaTime = frameStart - lastFrameTime;
        lastFrameTime = frameStart;

        // ESC za izlaz
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // logika
        updateScene(window, deltaTime);

        // iscrtavanje
        renderScene();

        glfwSwapBuffers(window);
        glfwPollEvents();

        // FPS limiter - spavaj ako je frejm bio prebrz
        double frameEnd = glfwGetTime();
        double frameTime = frameEnd - frameStart;

        if (frameTime < FRAME_DURATION) {
            double sleepTime = FRAME_DURATION - frameTime;
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
        }
    }

    // terminacija
    cleanupScene();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
