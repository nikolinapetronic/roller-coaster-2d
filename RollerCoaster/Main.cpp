#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Util.h"
#include <iostream>

int endProgram(const char* message) {
    std::cout << message << std::endl;
    glfwTerminate();
    return -1;
}

void preprocessTexture(unsigned& texture, const char* filepath) {
    texture = loadImageToTexture(filepath); // ucitavanje teksture
    glBindTexture(GL_TEXTURE_2D, texture); // vezujemo se za teksturu kako bismo je podesili

    // generisanje mipmapa - predefinisani razliciti formati za lakše skaliranje po potrebi
    glGenerateMipmap(GL_TEXTURE_2D);

    // podesavanje strategija za wrap-ovanje
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S - tekseli po x-osi
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T - tekseli po y-osi

    // podesavanje algoritma za smanjivanje i povecavanje rezolucije: nearest - bira najblizi piksel, linear - usrednjava okolne piksele
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

    // ukljucivanje alfa kanala za providnost
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ucitavanje teksture vagona
    unsigned int wagonTexture;
    preprocessTexture(wagonTexture, "res/wagon.jpg");

    // FPS limiter i delta time
    const double TARGET_FPS = 75.0;
    const double FRAME_DURATION = 1.0 / TARGET_FPS; // trajanje jednog frejma u sekundama ( priblizno 0.0133s)

    // vrijeme posljednjeg iscrtanog frejma
    double lastFrameTime = glfwGetTime();

    // kreiranje shadera
    unsigned int basicShader = createShader("basic.vert", "basic.frag");

    // pronalazimo lokaciju uniforme uOffset u shaderu
    int uOffsetLocation = glGetUniformLocation(basicShader, "uOffset");
    if (uOffsetLocation == -1) {
        std::cout << "uOffset nije pronadjen u shaderu!" << std::endl;
    }

    // uniform za teksturu (sampler2D)
    int uTexLocation = glGetUniformLocation(basicShader, "uTex");
    if (uTexLocation == -1) {
        std::cout << "uTex nije pronadjen u shaderu!" << std::endl;
    }

    // podesimo da uTex koristi teksturnu jedinicu 0
    glUseProgram(basicShader);
    glUniform1i(uTexLocation, 0);  // GL_TEXTURE0

    // kreiranje VAO i VBO
    // x, y, u, v
    float vertices[] = {
        -0.2f,  0.2f, 0.0f, 1.0f, // gornje lijevo tjeme  (u=0, v=1)
        -0.2f, -0.2f, 0.0f, 0.0f, // donje lijevo tjeme   (u=0, v=0)
         0.2f, -0.2f, 1.0f, 0.0f, // donje desno tjeme    (u=1, v=0)
         0.2f,  0.2f, 1.0f, 1.0f  // gornje desno tjeme   (u=1, v=1)
    };

    unsigned int VAO;
    unsigned int VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);


    // postavljanje boje pozadine
    glClearColor(1.0f, 0.8f, 0.9f, 1.0f);

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

        // pomjeranje kvadrata - skalirano deltaTime-om 
        float speed = 0.5f; // jedinica u sekundi
        float velocity = speed * static_cast<float>(deltaTime);

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            offsetX -= velocity; // lijevo
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            offsetX += velocity; // desno
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            offsetY += velocity; // gore
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            offsetY -= velocity; // dole
        }

        glClear(GL_COLOR_BUFFER_BIT);

        // biramo teksturnu jedinicu 0 i vezujemo teksturu vagona
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, wagonTexture);

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
