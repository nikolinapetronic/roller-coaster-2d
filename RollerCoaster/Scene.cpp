#define _USE_MATH_DEFINES
#include "Scene.h"
#include "Util.h"
#include "RideMotion.h"
#include "TrackRenderer.h"
#include "Passengers.h"
#include "CartRenderer.h"
#include "PassengerRenderer.h"

#include <iostream>
#include <cmath>

// dimenzije ekrana i aspect (potrebni za pretvaranje misa u NDC)
static int screenWidthGlobal = 800;
static int screenHeightGlobal = 800;
// faktor potreban za dobijanje kvadrata na pravouaonom ekranu (fullscreen mod)
static float aspect = 1.0f;

// teksture
static unsigned int wagonTexture;
static unsigned int nameplateTexture;
static unsigned int passengerTexture;
static unsigned int passengerSickTexture;
static unsigned int beltTexture;

// sejderi
static unsigned int texturedAlphaShader;

// uniforme
static int uOffsetLocation = -1;
static int uTexLocation = -1;
static int uAlphaLocation = -1;
static int uAngleLocation = -1;

// geometrija vagona
// visina vagona na ekranu
static float cartHeight = 0.21f;
// faktor koliko je sirina veca od visine (npr. 3x)
static float cartShapeRatio = 2.7f;
// sirina korigovana aspect-om i oblikom
static float cartWidth = 0.0f;

// ------------------ PARAMETRI PRUGE U NDC ------------------

static float trackXMin = -0.9f;
static float trackXMax = 0.9f;

// ------------------ POMOCNE FUNKCIJE ------------------

// ucitavanje teksture i podesavanje parametara
void preprocessTexture(unsigned& texture, const char* filepath) {
    texture = loadImageToTexture(filepath); // ucitavanje teksture
    glBindTexture(GL_TEXTURE_2D, texture);  // vezujemo se za teksturu kako bismo je podesili

    // generisanje mipmapa - predefinisani razliciti formati za lakse skaliranje po potrebi
    glGenerateMipmap(GL_TEXTURE_2D);

    // podesavanje strategija za wrap-ovanje
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S - tekseli po x-osi
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T - tekseli po y-osi

    // podesavanje algoritma za smanjivanje i povecavanje rezolucije: nearest - bira najblizi piksel, linear - usrednjava okolne piksele
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}


// callback funkcije (prototipi)
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

// ------------------  initScene ------------------

void initScene(GLFWwindow* window, int screenWidth, int screenHeight)
{
    // cuvamo dimenzije ekrana globalno (za mis u NDC)
    screenWidthGlobal = screenWidth;
    screenHeightGlobal = screenHeight;

    // faktor potreban za dobijanje kvadrata na pravouaonom ekranu (fullscreen mod)
    aspect = static_cast<float>(screenHeightGlobal) / static_cast<float>(screenWidthGlobal);

    // vezivanje callback funkcija za tastaturu i mis
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    // ucitavanje custom kursora
    GLFWcursor* customCursor = loadImageToCursor("res/rail-road.png");
    if (customCursor != nullptr) {
        glfwSetCursor(window, customCursor);
        std::cout << "Custom kursor uspjesno postavljen." << std::endl;
    }
    else {
        std::cout << "Custom kursor NIJE postavljen." << std::endl;
    }

    // ucitavanje tekstura
    preprocessTexture(wagonTexture, "res/pink_cart.png");
    preprocessTexture(nameplateTexture, "res/nameplate1.png");
    preprocessTexture(passengerTexture, "res/passenger1.png");
    preprocessTexture(passengerSickTexture, "res/passenger_green1.png");
    preprocessTexture(beltTexture, "res/seatbelt.png");

    // kreiranje shadera
    texturedAlphaShader = createShader("textured_alpha.vert", "textured_alpha.frag");

    // pronalazimo lokacije uniformi u textured shaderu
    uOffsetLocation = glGetUniformLocation(texturedAlphaShader, "uOffset");
    if (uOffsetLocation == -1) {
        std::cout << "uOffset nije pronadjen u shaderu!" << std::endl;
    }

    uTexLocation = glGetUniformLocation(texturedAlphaShader, "uTex");
    if (uTexLocation == -1) {
        std::cout << "uTex nije pronadjen u shaderu!" << std::endl;
    }

    glUseProgram(texturedAlphaShader);
    glUniform1i(uTexLocation, 0);  // GL_TEXTURE0

    uAlphaLocation = glGetUniformLocation(texturedAlphaShader, "uAlpha");
    if (uAlphaLocation == -1) {
        std::cout << "uAlpha nije pronadjen u shaderu!" << std::endl;
    }

    uAngleLocation = glGetUniformLocation(texturedAlphaShader, "uAngle");
    if (uAngleLocation == -1)
        std::cout << "uAngle nije pronadjen u shaderu!\n";

    // ----------------- Geometrija vozila + 8 sjedista -----------------

    // visina vagona na ekranu
    cartHeight = 0.21f;
    // faktor koliko je sirina veca od visine
    cartShapeRatio = 1.7f;
    // sirina korigovana aspect-om i oblikom
    cartWidth = cartHeight * cartShapeRatio * aspect;

    // ----------------- Inicijalizacija 8 sjedista (1 red) -----------------
    float innerMargin = 0.15f;

    // lijeva i desna granica unutrasnjosti vagona
    float seatsLeftX = -cartWidth * (1.0f - innerMargin);
    float seatsRightX = cartWidth * (1.0f - innerMargin);

    // rucno pomjeranje cijelog reda sjedista malo ulijevo
    float seatShiftX = -0.01f;
    seatsLeftX += seatShiftX;
    seatsRightX += seatShiftX;

    // ukupni span za sjedista
    float seatSpan = seatsRightX - seatsLeftX;

    // razmak izmedju centara sjedista
    // dijeljenje sa SEAT_COUNT i pomjeranje za pola koraka da prvi/poslednji nisu skroz uz ivicu
    float seatStep = seatSpan / SEAT_COUNT;

    // ----------------- Parametri kompozicije po stazi -----------------
    initRideMotion(trackXMin, trackXMax, seatStep, SEAT_COUNT);

    // visina sjedista unutar vagona (po y-osi)
    float seatsY = cartHeight * 0.3f;

    // inicijalizacija sjedista (u passengers modu)
    initSeats(seatsLeftX, seatsY, seatStep);

    // renderer za prugu
    initTrackRenderer();

    // renderer za vagon i nameplate
    initCartRenderer(cartHeight,
        cartShapeRatio,
        aspect,
        seatStep,
        wagonTexture,
        nameplateTexture,
        texturedAlphaShader,
        uOffsetLocation,
        uAngleLocation,
        uAlphaLocation);

    // renderer za putnike i pojaseve
    initPassengerRenderer(passengerTexture,
        passengerSickTexture,
        beltTexture,
        texturedAlphaShader,
        uOffsetLocation,
        uAngleLocation,
        uAlphaLocation);

    // vracamo na neki default VAO
    glBindVertexArray(0);
}

// ------------------  KeyCallback ------------------

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)window;
    (void)scancode;
    (void)mods;

    // reagujemo samo na pritisak (ne i na pustanje)
    if (action != GLFW_PRESS)
        return;

    switch (key)
    {
    case GLFW_KEY_SPACE:
        // logika dodavanja putnika izmjestena u Passengers modul
        tryAddPassengerFromBack();
        break;

    case GLFW_KEY_ENTER:
    {
        // pokusaj da pokrenes voznju
        bool canStart = areAllOccupiedSeatsBelted();
        tryStartRide(canStart);
        break;
    }

    // tasteri 1-8 simuliraju signal da se nekom putniku slosilo
    case GLFW_KEY_1:
    case GLFW_KEY_2:
    case GLFW_KEY_3:
    case GLFW_KEY_4:
    case GLFW_KEY_5:
    case GLFW_KEY_6:
    case GLFW_KEY_7:
    case GLFW_KEY_8:
    {
        int keyIndex = key - GLFW_KEY_1;
        handleSickKeyFromFrontIndex(keyIndex);
        break;
    }
    }
}

// ------------------  MouseButtonCallback ------------------

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    (void)window;
    (void)mods;

    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    // pozicija misa u trenutku klika
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // pretvaranje koordinata misa u NDC (-1,1)
    float mouseNdcX = (float)((mouseX / screenWidthGlobal) * 2.0 - 1.0);
    float mouseNdcY = (float)(1.0 - (mouseY / screenHeightGlobal) * 2.0);

    // prepustamo PassengerRenderer-u da obavi logiku
    handlePassengerClick(mouseNdcX, mouseNdcY);
}

// ------------------  updateScene ------------------

void updateScene(GLFWwindow* window, double deltaTime)
{
    (void)window;
    updateRide(deltaTime);

    // ako se voz upravo vratio na pocetak (bilo normalno, bilo poslije emergency)
    if (didJustReturnToStart()) {
        handleRideReturned();
    }
}


// ------------------  renderScene ------------------

void renderScene()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // crtanje pruge 
    renderTrack();

    // crtanje vozila
    renderCart();

    // crtanje putnika i pojaseva
    renderPassengers();

    // crtanje nameplate-a
    renderNameplate();
}

// ------------------  cleanupScene ------------------

void cleanupScene()
{
    cleanupTrackRenderer();
    cleanupCartRenderer();
    cleanupPassengerRenderer();

    glDeleteProgram(texturedAlphaShader);
}
