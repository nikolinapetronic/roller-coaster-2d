#define _USE_MATH_DEFINES
#include "PassengerRenderer.h"
#include "Passengers.h"
#include "RideMotion.h"
#include "CartRenderer.h"

#include <cmath>

// VAO/VBO za putnika/pojas
static GLuint g_vaoPassenger = 0;
static GLuint g_vboPassenger = 0;

// teksture
static GLuint g_passengerTexture = 0;
static GLuint g_passengerSickTexture = 0;
static GLuint g_beltTexture = 0;

// shader i uniforme
static GLuint g_texturedAlphaShader = 0;
static int g_uOffsetLocation = -1;
static int g_uAngleLocation = -1;
static int g_uAlphaLocation = -1;

// geometrija putnika
// putnik malo uzi od vagona
static float g_passengerHalfWidth = 0.0f;
static float g_passengerHalfHeight = 0.0f;

// koliko iznad dna vagona je centar putnika 
static float g_passengerHeightFromBase = 0.0f;

// lokalno pomjeranje putnika u odnosu na centar sjedista (NDC)
static const float g_passengerOffsetX = -0.01f;   // malo ulijevo

// pomocna funkcija: racuna centar putnika i ugao za zadato sjediste
static void getPassengerCenterForSeat(int seatIndex,
    float& centerX,
    float& centerY,
    float& angleOut)
{
    float sx, sy, angle;
    getSeatBasePositionAndAngle(seatIndex, sx, sy, angle);

    // lokalni pomjeraj putnika (blago ulijevo u odnosu na sjediste)
    float localX = g_passengerOffsetX;          // malo ulijevo
    float localY = g_passengerHeightFromBase;   // iznad dna vagona

    float c = std::cos(angle);
    float s = std::sin(angle);

    centerX = sx + localX * c - localY * s;
    centerY = sy + localX * s + localY * c;
    angleOut = angle;
}

void initPassengerRenderer(GLuint passengerTexture,
    GLuint passengerSickTexture,
    GLuint beltTexture,
    GLuint texturedAlphaShader,
    int uOffsetLocation,
    int uAngleLocation,
    int uAlphaLocation)
{
    g_passengerTexture = passengerTexture;
    g_passengerSickTexture = passengerSickTexture;
    g_beltTexture = beltTexture;

    g_texturedAlphaShader = texturedAlphaShader;
    g_uOffsetLocation = uOffsetLocation;
    g_uAngleLocation = uAngleLocation;
    g_uAlphaLocation = uAlphaLocation;

    // geometrija putnika zasnovana na segmentu vagona
    float wagonSegmentHeight = getWagonSegmentHeight();
    float wagonSegmentWidth = getWagonSegmentWidth();

    g_passengerHalfWidth = wagonSegmentWidth * 0.7f;
    g_passengerHalfHeight = wagonSegmentHeight * 0.6f;
    g_passengerHeightFromBase = wagonSegmentHeight * 1.2f;

    float passengerVertices[] = {
        -g_passengerHalfWidth,  g_passengerHalfHeight, 0.0f, 1.0f, // gornje lijevo
        -g_passengerHalfWidth, -g_passengerHalfHeight, 0.0f, 0.0f, // donje lijevo
         g_passengerHalfWidth, -g_passengerHalfHeight, 1.0f, 0.0f, // donje desno
         g_passengerHalfWidth,  g_passengerHalfHeight, 1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &g_vaoPassenger);
    glGenBuffers(1, &g_vboPassenger);

    glBindVertexArray(g_vaoPassenger);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboPassenger);
    glBufferData(GL_ARRAY_BUFFER, sizeof(passengerVertices), passengerVertices, GL_STATIC_DRAW);

    // pozicija
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void renderPassengers()
{
    glUseProgram(g_texturedAlphaShader);
    glBindVertexArray(g_vaoPassenger);

    for (int i = 0; i < SEAT_COUNT; ++i) {
        if (!seats[i].occupied) continue;

        // izracunaj centar putnika i ugao za zadato sjediste
        float px, py, angle;
        getPassengerCenterForSeat(i, px, py, angle);

        // prvo crtamo putnika
        glActiveTexture(GL_TEXTURE0);
        GLuint tex = seats[i].sick ? g_passengerSickTexture : g_passengerTexture;
        glBindTexture(GL_TEXTURE_2D, tex);

        glUniform1f(g_uAlphaLocation, 1.0f);
        glUniform1f(g_uAngleLocation, angle);
        glUniform2f(g_uOffsetLocation, px, py);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // ako je pojas zakacen, crtamo pojas preko istog centra
        if (seats[i].beltOn) {
            glBindTexture(GL_TEXTURE_2D, g_beltTexture);
            glUniform1f(g_uAlphaLocation, 1.0f);
            glUniform1f(g_uAngleLocation, angle);
            glUniform2f(g_uOffsetLocation, px, py);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }

    glBindVertexArray(0);
}

void handlePassengerClick(float mouseNdcX, float mouseNdcY)
{
    if (isRideRunning())    // nije dozvoljeno dodavanje/uklanjanje pojaseva u toku voznje
        return;

    // prodji kroz sva sjedista i vidi da li je klik unutar "kvadrata putnika"
    for (int i = 0; i < SEAT_COUNT; ++i) {
        if (!seats[i].occupied) continue; // ako nema putnika, nista

        // centar putnika
        float cx, cy, angle;
        getPassengerCenterForSeat(i, cx, cy, angle);

        if (mouseNdcX >= cx - g_passengerHalfWidth && mouseNdcX <= cx + g_passengerHalfWidth &&
            mouseNdcY >= cy - g_passengerHalfHeight && mouseNdcY <= cy + g_passengerHalfHeight) {

            if (!unloadingPhase) {
                // normalni rezim -> vezi / odvezi pojas
                seats[i].beltOn = !seats[i].beltOn;
            }
            else {
                // u fazi iskrcavanja -> klik ga skida iz voza
                seats[i].occupied = false;
                seats[i].beltOn = false;
                seats[i].sick = false;

                // provjeri da li su svi otisli -> zavrsavamo unloading
                bool anyOccupied = false;
                for (int sIdx = 0; sIdx < SEAT_COUNT; ++sIdx) {
                    if (seats[sIdx].occupied) {
                        anyOccupied = true;
                        break;
                    }
                }
                if (!anyOccupied) {
                    unloadingPhase = false;
                }
            }

            break; // samo jedan putnik po kliku
        }
    }
}

void cleanupPassengerRenderer()
{
    if (g_vboPassenger) {
        glDeleteBuffers(1, &g_vboPassenger);
        g_vboPassenger = 0;
    }
    if (g_vaoPassenger) {
        glDeleteVertexArrays(1, &g_vaoPassenger);
        g_vaoPassenger = 0;
    }
}
