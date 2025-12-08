#define _USE_MATH_DEFINES
#include "CartRenderer.h"
#include "Passengers.h"
#include "RideMotion.h"

#include <cmath>

// VAO/VBO za cart i nameplate
static GLuint vaoCart = 0;
static GLuint vboCart = 0;

static GLuint vaoNameplate = 0;
static GLuint vboNameplate = 0;

// teksture
static GLuint cartTexture = 0;
static GLuint nameplateTexture = 0;

// shader i uniforme (teksturisani alpha shader)
static GLuint texturedAlphaShader = 0;
static int uOffsetLocation = -1;
static int uAngleLocation = -1;
static int uAlphaLocation = -1;

// geometrija cart-a
static float cartHeight = 0.0f;
static float cartShapeRatio = 0.0f;
static float cartWidth = 0.0f;

static float cartSegmentWidth = 0.0f;
static float cartSegmentHeight = 0.0f;

void initCartRenderer(float inCartHeight,
    float inCartShapeRatio,
    float aspect,
    float seatStep,
    GLuint inCartTexture,
    GLuint inNameplateTexture,
    GLuint inTexturedAlphaShader,
    int inUOffsetLocation,
    int inUAngleLocation,
    int inUAlphaLocation)
{
    // sacuvaj parametre u staticke promenljive
    cartHeight = inCartHeight;
    cartShapeRatio = inCartShapeRatio;
    cartWidth = cartHeight * cartShapeRatio * aspect;

    cartTexture = inCartTexture;
    nameplateTexture = inNameplateTexture;

    texturedAlphaShader = inTexturedAlphaShader;
    uOffsetLocation = inUOffsetLocation;
    uAngleLocation = inUAngleLocation;
    uAlphaLocation = inUAlphaLocation;

    // geometrija 8 malih kvadrata vozila (segment ispod jednog sjedista)

    // visina kvadrata (po y), u NDC
    cartSegmentHeight = cartHeight * 0.25f;

    // sirina vezana za razmak izmedju sjedista
    cartSegmentWidth = seatStep * 0.65f;

    // x, y, u, v  
    float cartVertices[] = {
        -cartSegmentWidth,  2.0f * cartSegmentHeight, 0.0f, 1.0f, // gornje lijevo
        -cartSegmentWidth,  0.0f,                      0.0f, 0.0f, // donje lijevo
         cartSegmentWidth,  0.0f,                      1.0f, 0.0f, // donje desno
         cartSegmentWidth,  2.0f * cartSegmentHeight, 1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &vaoCart);
    glGenBuffers(1, &vboCart);

    glBindVertexArray(vaoCart);
    glBindBuffer(GL_ARRAY_BUFFER, vboCart);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cartVertices), cartVertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ------------------ VAO/VBO za nameplate (ime u gornjem lijevom uglu) ------------------

    float nameplateVertices[] = {
        -1.0f,   1.0f,   0.0f, 1.0f, // gornje lijevo
        -1.0f,   0.65f,  0.0f, 0.0f, // donje lijevo
        -0.625f, 0.65f,  1.0f, 0.0f, // donje desno
        -0.625f, 1.0f,   1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &vaoNameplate);
    glGenBuffers(1, &vboNameplate);

    glBindVertexArray(vaoNameplate);
    glBindBuffer(GL_ARRAY_BUFFER, vboNameplate);
    glBufferData(GL_ARRAY_BUFFER, sizeof(nameplateVertices), nameplateVertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void renderCart()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cartTexture);

    glUseProgram(texturedAlphaShader);
    glUniform1f(uAlphaLocation, 1.0f);

    glBindVertexArray(vaoCart);

    for (int i = 0; i < SEAT_COUNT; ++i) {
        float sx, sy, angle;
        getSeatBasePositionAndAngle(i, sx, sy, angle);

        glUniform1f(uAngleLocation, angle);
        glUniform2f(uOffsetLocation, sx, sy);  // pivot na pruzi, dno cart-a
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    glBindVertexArray(0);
}

void renderNameplate()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, nameplateTexture);

    glUseProgram(texturedAlphaShader);

    // nameplate poluprovidan
    glUniform1f(uAlphaLocation, 0.5f);
    // nameplate statican u uglu, bez pomjeranja
    glUniform2f(uOffsetLocation, 0.0f, 0.0f);
    glUniform1f(uAngleLocation, 0.0f);

    glBindVertexArray(vaoNameplate);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
}

void cleanupCartRenderer()
{
    if (vboCart) {
        glDeleteBuffers(1, &vboCart);
        vboCart = 0;
    }
    if (vaoCart) {
        glDeleteVertexArrays(1, &vaoCart);
        vaoCart = 0;
    }

    if (vboNameplate) {
        glDeleteBuffers(1, &vboNameplate);
        vboNameplate = 0;
    }
    if (vaoNameplate) {
        glDeleteVertexArrays(1, &vaoNameplate);
        vaoNameplate = 0;
    }
}

float getCartSegmentHeight()
{
    return cartSegmentHeight;
}

float getCartSegmentWidth()
{
    return cartSegmentWidth;
}
