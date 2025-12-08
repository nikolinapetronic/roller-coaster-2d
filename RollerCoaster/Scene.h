#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// inicijalizacija svih OpenGL resura i scene
void initScene(GLFWwindow* window, int screenWidth, int screenHeight);

// azuriranje logike (input, kretanje, toggle pojaseva...)
void updateScene(GLFWwindow* window, double deltaTime);

// iscrtavanje svega (pruga, stubovi, vagon, putnici, pojasevi, nameplate)
void renderScene();

// oslobadjanje OpenGL resursa
void cleanupScene();
