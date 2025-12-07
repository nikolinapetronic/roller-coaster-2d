#define _USE_MATH_DEFINES
#include "CartRenderer.h"
#include "Passengers.h"
#include "RideMotion.h"

#include <cmath>

// VAO/VBO za vagon i nameplate
static GLuint g_vaoCart = 0;
static GLuint g_vboCart = 0;

static GLuint g_vaoNameplate = 0;
static GLuint g_vboNameplate = 0;

// teksture
static GLuint g_wagonTexture = 0;
static GLuint g_nameplateTexture = 0;

// shader i uniforme (teksturisani alpha shader)
static GLuint g_texturedAlphaShader = 0;
static int g_uOffsetLocation = -1;
static int g_uAngleLocation = -1;
static int g_uAlphaLocation = -1;

// geometrija vagona
static float g_cartHeight = 0.0f;
static float g_cartShapeRatio = 0.0f;
static float g_cartWidth = 0.0f;

static float g_wagonSegmentWidth = 0.0f;
static float g_wagonSegmentHeight = 0.0f;

void initCartRenderer(float cartHeight,
    float cartShapeRatio,
    float aspect,
    float seatStep,
    GLuint wagonTexture,
    GLuint nameplateTexture,
    GLuint texturedAlphaShader,
    int uOffsetLocation,
    int uAngleLocation,
    int uAlphaLocation)
{
    g_cartHeight = cartHeight;
    g_cartShapeRatio = cartShapeRatio;
    g_cartWidth = g_cartHeight * g_cartShapeRatio * aspect;

    g_wagonTexture = wagonTexture;
    g_nameplateTexture = nameplateTexture;

    g_texturedAlphaShader = texturedAlphaShader;
    g_uOffsetLocation = uOffsetLocation;
    g_uAngleLocation = uAngleLocation;
    g_uAlphaLocation = uAlphaLocation;

    // geometrija 8 malih kvadrata vozila (segment ispod jednog sjedista)

    // visina kvadrata (po y), u NDC
    g_wagonSegmentHeight = g_cartHeight * 0.25f;

    // sirina vezana za razmak izmedju sjedista
    g_wagonSegmentWidth = seatStep * 0.65f;

    // x, y, u, v  
    float wagonVertices[] = {
        -g_wagonSegmentWidth,  2.0f * g_wagonSegmentHeight, 0.0f, 1.0f, // gornje lijevo
        -g_wagonSegmentWidth,  0.0f,                        0.0f, 0.0f, // donje lijevo
         g_wagonSegmentWidth,  0.0f,                        1.0f, 0.0f, // donje desno
         g_wagonSegmentWidth,  2.0f * g_wagonSegmentHeight, 1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &g_vaoCart);
    glGenBuffers(1, &g_vboCart);

    glBindVertexArray(g_vaoCart);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboCart);
    glBufferData(GL_ARRAY_BUFFER, sizeof(wagonVertices), wagonVertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ------------------ VAO/VBO za nameplate (ime u gornjem lijevom uglu) ------------------

    float nameplateVertices[] = {
        -1.0f,  1.0f,    0.0f, 1.0f, // gornje lijevo
        -1.0f,  0.65f,   0.0f, 0.0f, // donje lijevo
        -0.625f,0.65f,   1.0f, 0.0f, // donje desno
        -0.625f,1.0f,    1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &g_vaoNameplate);
    glGenBuffers(1, &g_vboNameplate);

    glBindVertexArray(g_vaoNameplate);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboNameplate);
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
    glBindTexture(GL_TEXTURE_2D, g_wagonTexture);

    glUseProgram(g_texturedAlphaShader);
    glUniform1f(g_uAlphaLocation, 1.0f);

    glBindVertexArray(g_vaoCart);

    for (int i = 0; i < SEAT_COUNT; ++i) {
        float sx, sy, angle;
        getSeatBasePositionAndAngle(i, sx, sy, angle);

        glUniform1f(g_uAngleLocation, angle);
        glUniform2f(g_uOffsetLocation, sx, sy);  // pivot na pruzi, dno vagona
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    glBindVertexArray(0);
}

void renderNameplate()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_nameplateTexture);

    glUseProgram(g_texturedAlphaShader);

    // nameplate poluprovidan
    glUniform1f(g_uAlphaLocation, 0.5f);
    // nameplate statican u uglu, bez pomjeranja
    glUniform2f(g_uOffsetLocation, 0.0f, 0.0f);
    glUniform1f(g_uAngleLocation, 0.0f);

    glBindVertexArray(g_vaoNameplate);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
}

void cleanupCartRenderer()
{
    if (g_vboCart) {
        glDeleteBuffers(1, &g_vboCart);
        g_vboCart = 0;
    }
    if (g_vaoCart) {
        glDeleteVertexArrays(1, &g_vaoCart);
        g_vaoCart = 0;
    }

    if (g_vboNameplate) {
        glDeleteBuffers(1, &g_vboNameplate);
        g_vboNameplate = 0;
    }
    if (g_vaoNameplate) {
        glDeleteVertexArrays(1, &g_vaoNameplate);
        g_vaoNameplate = 0;
    }
}

float getWagonSegmentHeight()
{
    return g_wagonSegmentHeight;
}

float getWagonSegmentWidth()
{
    return g_wagonSegmentWidth;
}
