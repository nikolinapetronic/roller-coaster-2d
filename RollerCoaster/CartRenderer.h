#pragma once

#include <GL/glew.h>

// inicijalizacija geometrije vagona i nameplate-a
void initCartRenderer(float cartHeight,
    float cartShapeRatio,
    float aspect,
    float seatStep,
    GLuint wagonTexture,
    GLuint nameplateTexture,
    GLuint texturedAlphaShader,
    int uOffsetLocation,
    int uAngleLocation,
    int uAlphaLocation);

// crtanje svih segmenata vagona (po sjedistima)
void renderCart();

// crtanje nameplate-a u gornjem lijevom uglu
void renderNameplate();

// oslobadjanje VAO/VBO resursa
void cleanupCartRenderer();

// pomocne funkcije 
float getCartSegmentHeight();
float getCartSegmentWidth();
