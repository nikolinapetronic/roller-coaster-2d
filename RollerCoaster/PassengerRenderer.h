#pragma once

#include <GL/glew.h>

// inicijalizacija VAO/VBO za putnika, tekstura i veza na shader
void initPassengerRenderer(GLuint passengerTexture,
    GLuint passengerSickTexture,
    GLuint beltTexture,
    GLuint texturedAlphaShader,
    int uOffsetLocation,
    int uAngleLocation,
    int uAlphaLocation);

// crtanje svih putnika i pojaseva
void renderPassengers();

// logika za klik misa nad putnicima
void handlePassengerClick(float mouseNdcX, float mouseNdcY);

// oslobadjanje VAO/VBO
void cleanupPassengerRenderer();
