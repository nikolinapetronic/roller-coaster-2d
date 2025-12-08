#define _USE_MATH_DEFINES
#include "PassengerRenderer.h"
#include "Passengers.h"
#include "RideMotion.h"
#include "CartRenderer.h"

#include <cmath>

// VAO/VBO za putnika/pojas
static GLuint vaoPassenger = 0;
static GLuint vboPassenger = 0;

// teksture
static GLuint passengerTexture = 0;
static GLuint passengerSickTexture = 0;
static GLuint beltTexture = 0;

// shader i uniforme
static GLuint texturedAlphaShader = 0;
static int uOffsetLocation = -1;
static int uAngleLocation = -1;
static int uAlphaLocation = -1;

// geometrija putnika
// putnik malo uzi od vagona
static float passengerHalfWidth = 0.0f;
static float passengerHalfHeight = 0.0f;

// koliko iznad dna vagona je centar putnika 
static float passengerHeightFromBase = 0.0f;

// lokalno pomjeranje putnika u odnosu na centar sjedista (NDC)
static const float passengerOffsetX = -0.01f;   // malo ulijevo

// pomocna funkcija: racuna centar putnika i ugao za zadato sjediste
static void getPassengerCenterForSeat(int seatIndex,
    float& centerX,
    float& centerY,
    float& angleOut)
{
    float sx, sy, angle;
    getSeatBasePositionAndAngle(seatIndex, sx, sy, angle);

    // lokalni pomjeraj putnika (blago ulijevo u odnosu na sjediste)
    float localX = passengerOffsetX;          // malo ulijevo
    float localY = passengerHeightFromBase;   // iznad dna vagona

    float c = std::cos(angle);
    float s = std::sin(angle);

    centerX = sx + localX * c - localY * s;
    centerY = sy + localX * s + localY * c;
    angleOut = angle;
}

void initPassengerRenderer(GLuint inPassengerTexture,
    GLuint inPassengerSickTexture,
    GLuint inBeltTexture,
    GLuint inTexturedAlphaShader,
    int inUOffsetLocation,
    int inUAngleLocation,
    int inUAlphaLocation)
{
    passengerTexture = inPassengerTexture;
    passengerSickTexture = inPassengerSickTexture;
    beltTexture = inBeltTexture;

    texturedAlphaShader = inTexturedAlphaShader;
    uOffsetLocation = inUOffsetLocation;
    uAngleLocation = inUAngleLocation;
    uAlphaLocation = inUAlphaLocation;

    // geometrija putnika zasnovana na segmentu cart-a
    float cartSegmentHeight = getCartSegmentHeight();
    float cartSegmentWidth = getCartSegmentWidth();

    passengerHalfWidth = cartSegmentWidth * 0.7f;
    passengerHalfHeight = cartSegmentHeight * 0.6f;
    passengerHeightFromBase = cartSegmentHeight * 1.2f;

    float passengerVertices[] = {
        -passengerHalfWidth,  passengerHalfHeight, 0.0f, 1.0f, // gornje lijevo
        -passengerHalfWidth, -passengerHalfHeight, 0.0f, 0.0f, // donje lijevo
         passengerHalfWidth, -passengerHalfHeight, 1.0f, 0.0f, // donje desno
         passengerHalfWidth,  passengerHalfHeight, 1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &vaoPassenger);
    glGenBuffers(1, &vboPassenger);

    glBindVertexArray(vaoPassenger);
    glBindBuffer(GL_ARRAY_BUFFER, vboPassenger);
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
    glUseProgram(texturedAlphaShader);
    glBindVertexArray(vaoPassenger);

    for (int i = 0; i < SEAT_COUNT; ++i) {
        if (!seats[i].occupied) continue;

        // izracunaj centar putnika i ugao za zadato sjediste
        float px, py, angle;
        getPassengerCenterForSeat(i, px, py, angle);

        // prvo crtamo putnika
        glActiveTexture(GL_TEXTURE0);
        GLuint tex = seats[i].sick ? passengerSickTexture : passengerTexture;
        glBindTexture(GL_TEXTURE_2D, tex);

        glUniform1f(uAlphaLocation, 1.0f);
        glUniform1f(uAngleLocation, angle);
        glUniform2f(uOffsetLocation, px, py);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // ako je pojas zakacen, crtamo pojas preko istog centra
        if (seats[i].beltOn) {
            glBindTexture(GL_TEXTURE_2D, beltTexture);
            glUniform1f(uAlphaLocation, 1.0f);
            glUniform1f(uAngleLocation, angle);
            glUniform2f(uOffsetLocation, px, py);
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

        if (mouseNdcX >= cx - passengerHalfWidth && mouseNdcX <= cx + passengerHalfWidth &&
            mouseNdcY >= cy - passengerHalfHeight && mouseNdcY <= cy + passengerHalfHeight) {

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
    if (vboPassenger) {
        glDeleteBuffers(1, &vboPassenger);
        vboPassenger = 0;
    }
    if (vaoPassenger) {
        glDeleteVertexArrays(1, &vaoPassenger);
        vaoPassenger = 0;
    }
}
