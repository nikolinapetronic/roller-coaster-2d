#define _USE_MATH_DEFINES
#include "TrackRenderer.h"
#include "RideMotion.h"
#include "Util.h"

#include <iostream>

// VAO/VBO za prugu i stubove
static unsigned int vaoTrack = 0;
static unsigned int vboTrack = 0;
static unsigned int vaoSupports = 0;
static unsigned int vboSupports = 0;

// shader za prugu (samo boja)
static unsigned int trackShader = 0;
static int uTrackColorLocation = -1;

// broj stubova
static const int SUPPORT_COUNT = 35;
static float supportVertices[SUPPORT_COUNT * 4];
// svaki stub ima 2 verteksa: (x_top, y_top), (x_top, y_bottom) => 4 floats

void initTrackRenderer()
{
    // kreiranje shadera
    trackShader = createShader("solid_color.vert", "solid_color.frag");
    uTrackColorLocation = glGetUniformLocation(trackShader, "uColor");
    if (uTrackColorLocation == -1) {
        std::cout << "uColor (track) nije pronadjen u shaderu!" << std::endl;
    }

    // ------------------  VAO/VBO za prugu rolerkostera ------------------

    const float* trackVertices = getTrackVertices();

    glGenVertexArrays(1, &vaoTrack);
    glBindVertexArray(vaoTrack);

    glGenBuffers(1, &vboTrack);
    glBindBuffer(GL_ARRAY_BUFFER, vboTrack);
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

    glGenVertexArrays(1, &vaoSupports);
    glBindVertexArray(vaoSupports);

    glGenBuffers(1, &vboSupports);
    glBindBuffer(GL_ARRAY_BUFFER, vboSupports);
    glBufferData(GL_ARRAY_BUFFER, sizeof(supportVertices), supportVertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void renderTrack()
{
    // crtanje pruge - jedna glatka kriva sa ravnim dijelovima
    glUseProgram(trackShader);
    // skoro bijela
    glUniform3f(uTrackColorLocation, 0.8f, 1.0f, 1.0f);

    glBindVertexArray(vaoTrack);
    glDrawArrays(GL_LINE_STRIP, 0, TRACK_POINT_COUNT);

    // crtanje stubova ispod pruge (isti shader i boja kao za prugu)
    glBindVertexArray(vaoSupports);
    glDrawArrays(GL_LINES, 0, SUPPORT_COUNT * 2);

    glBindVertexArray(0);
}

void cleanupTrackRenderer()
{
    if (vboTrack) {
        glDeleteBuffers(1, &vboTrack);
        vboTrack = 0;
    }
    if (vaoTrack) {
        glDeleteVertexArrays(1, &vaoTrack);
        vaoTrack = 0;
    }

    if (vboSupports) {
        glDeleteBuffers(1, &vboSupports);
        vboSupports = 0;
    }
    if (vaoSupports) {
        glDeleteVertexArrays(1, &vaoSupports);
        vaoSupports = 0;
    }

    if (trackShader) {
        glDeleteProgram(trackShader);
        trackShader = 0;
    }
}
