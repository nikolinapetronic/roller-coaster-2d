#define _USE_MATH_DEFINES
#include "TrackRenderer.h"
#include "RideMotion.h"
#include "Util.h"

#include <iostream>

// VAO/VBO za prugu i stubove
static unsigned int g_vaoTrack = 0;
static unsigned int g_vboTrack = 0;
static unsigned int g_vaoSupports = 0;
static unsigned int g_vboSupports = 0;

// shader za prugu (samo boja)
static unsigned int g_trackShader = 0;
static int g_uTrackColorLocation = -1;

// broj stubova
static const int SUPPORT_COUNT = 35;
static float supportVertices[SUPPORT_COUNT * 4];
// svaki stub ima 2 verteksa: (x_top, y_top), (x_top, y_bottom) => 4 floats

void initTrackRenderer()
{
    // kreiranje shadera
    g_trackShader = createShader("solid_color.vert", "solid_color.frag");
    g_uTrackColorLocation = glGetUniformLocation(g_trackShader, "uColor");
    if (g_uTrackColorLocation == -1) {
        std::cout << "uColor (track) nije pronadjen u shaderu!" << std::endl;
    }

    // ------------------  VAO/VBO za prugu rolerkostera ------------------

    const float* trackVertices = getTrackVertices();

    glGenVertexArrays(1, &g_vaoTrack);
    glBindVertexArray(g_vaoTrack);

    glGenBuffers(1, &g_vboTrack);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboTrack);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(float) * TRACK_POINT_COUNT * 2,
        trackVertices,
        GL_STATIC_DRAW);

    // atribut 0: pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // deblja linija da izgleda kao sina
    glLineWidth(3.0f);

    // ------------------ STUBOVI ISPOD PRUGE ------------------

    float bottomY = -0.98f;
    const float* tv = trackVertices;

    for (int i = 0; i < SUPPORT_COUNT; ++i) {
        // uzmi tacku sa pruge na odredjenom mjestu
        int   trackIndex = i * (TRACK_POINT_COUNT - 1) / (SUPPORT_COUNT - 1);
        float x_top = tv[trackIndex * 2];
        float y_top = tv[trackIndex * 2 + 1];

        // gornja tacka (na pruzi)
        supportVertices[i * 4 + 0] = x_top;
        supportVertices[i * 4 + 1] = y_top;

        // donja tacka (na dnu ekrana)
        supportVertices[i * 4 + 2] = x_top;
        supportVertices[i * 4 + 3] = bottomY;
    }

    glGenVertexArrays(1, &g_vaoSupports);
    glBindVertexArray(g_vaoSupports);

    glGenBuffers(1, &g_vboSupports);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboSupports);
    glBufferData(GL_ARRAY_BUFFER, sizeof(supportVertices), supportVertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void renderTrack()
{
    // crtanje pruge - jedna glatka kriva sa ravnim dijelovima
    glUseProgram(g_trackShader);
    // skoro bijela
    glUniform3f(g_uTrackColorLocation, 0.8f, 1.0f, 1.0f);

    glBindVertexArray(g_vaoTrack);
    glDrawArrays(GL_LINE_STRIP, 0, TRACK_POINT_COUNT);

    // crtanje stubova ispod pruge (isti shader i boja kao za prugu)
    glBindVertexArray(g_vaoSupports);
    glDrawArrays(GL_LINES, 0, SUPPORT_COUNT * 2);

    glBindVertexArray(0);
}

void cleanupTrackRenderer()
{
    if (g_vboTrack) {
        glDeleteBuffers(1, &g_vboTrack);
        g_vboTrack = 0;
    }
    if (g_vaoTrack) {
        glDeleteVertexArrays(1, &g_vaoTrack);
        g_vaoTrack = 0;
    }

    if (g_vboSupports) {
        glDeleteBuffers(1, &g_vboSupports);
        g_vboSupports = 0;
    }
    if (g_vaoSupports) {
        glDeleteVertexArrays(1, &g_vaoSupports);
        g_vaoSupports = 0;
    }

    if (g_trackShader) {
        glDeleteProgram(g_trackShader);
        g_trackShader = 0;
    }
}
