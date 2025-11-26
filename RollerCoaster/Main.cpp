#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Util.h"
#include <iostream>

int endProgram(const char* message) {
    std::cout << message << std::endl;
    glfwTerminate();
    return -1;
}

int main()
{
    // pomjeraj kvadrata (vagona) po x i y osi
    float offsetX = 0.0f;
    float offsetY = 0.0f;

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
        monitor,    
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

    // kreiranje shadera
    unsigned int basicShader = createShader("basic.vert", "basic.frag");

    // pronalazimo lokaciju uniforme uOffset u shaderu
    int uOffsetLocation = glGetUniformLocation(basicShader, "uOffset");
    if (uOffsetLocation == -1) {
        std::cout << "uOffset nije pronadjen u shaderu!" << std::endl;
    }

    // kreiranje VAO i VBO
    float vertices[] = {
     -0.2f, 0.2f, 0.0f, 0.0f, 1.0f, // gornje lijevo tjeme
     -0.2f, -0.2f, 0.0f, 1.0f, 0.0f, // donje lijevo tjeme
      0.2f, -0.2f, 1.0f, 0.0f, 0.0f, // donje desno tjeme
      0.2f, 0.2f, 0.0f, 1.0f, 1.0f  // gornje desno tjeme
    };

    unsigned int VAO;
    unsigned int VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // pozicija
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // boja
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);


    // postavljanje boje pozadine
    glClearColor(1.0f, 0.8f, 0.9f, 1.0f);

    // glavna petlja 
    while (!glfwWindowShouldClose(window))
    {
        // esc za iskljucivanje
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // pomjeranje kvadrata tastaturom (WASD)
        float speed = 0.01f; // korak pomjeranja u jednom frejmu

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            offsetX -= speed; // lijevo
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            offsetX += speed; // desno
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            offsetY += speed; // gore
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            offsetY -= speed; // dole
        }

        glClear(GL_COLOR_BUFFER_BIT);

        // crtanje kvadrata za provjeru
        glUseProgram(basicShader); // koristi shader 
        
        glUniform2f(uOffsetLocation, offsetX, offsetY); // slanje offseta u shader

        glBindVertexArray(VAO);    // koristi VAO sa kvadratom

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // 4 verteksa kao kvadrat

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // terminacija
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
