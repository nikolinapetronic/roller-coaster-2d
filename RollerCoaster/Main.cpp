#define _USE_MATH_DEFINES
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Util.h"
#include "Scene.h"
#include <iostream>
#include <cmath>

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

    // GLEW inicijalizacija
    if (glewInit() != GLEW_OK) {
        return endProgram("GLEW nije uspio da se inicijalizuje.");
    }

    std::cout << "GLEW uspjesno inicijalizovan." << std::endl;

    // ukljucivanje alfa kanala za providnost
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // postavljanje boje pozadine
    glClearColor(0.68f, 0.85f, 0.90f, 1.0f);

    // inicijalizacija scene
    InitScene(window, screenWidth, screenHeight);

    // FPS limiter i delta time
    const double TARGET_FPS = 75.0;
    const double FRAME_DURATION = 1.0 / TARGET_FPS; // trajanje jednog frejma u sekundama ( priblizno 0.0133s)

    // vrijeme posljednjeg iscrtanog frejma
    double lastFrameTime = glfwGetTime();

    // glavna petlja 
    while (!glfwWindowShouldClose(window))
    {
        // vrijeme od posljednjeg frejma
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastFrameTime;

        // FPS limiter - ako frejm traje krace od FRAME_DURATION, sacekaj
        if (deltaTime < FRAME_DURATION) {
            continue; // preskoci ostatak petlje, jos je rano za sljedeci frejm
        }

        // azuriranje vremena posljednjeg frejma
        lastFrameTime = currentTime;

        // ESC za izlaz 
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // logika: unos, pomjeranje, pojasevi, putnici...
        UpdateScene(window, deltaTime);

        // iscrtavanje svega
        RenderScene();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // terminacija
    CleanupScene();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
