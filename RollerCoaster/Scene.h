#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// inicijalizacija svih OpenGL resura i scene
void InitScene(GLFWwindow* window, int screenWidth, int screenHeight);

// azuriranje logike (input, kretanje, toggle pojaseva...)
void UpdateScene(GLFWwindow* window, double deltaTime);

// iscrtavanje svega (pruga, stubovi, vagon, putnici, pojasevi, nameplate)
void RenderScene();

// oslobadjanje OpenGL resursa
void CleanupScene();
