#define _USE_MATH_DEFINES
#include "Scene.h"
#include "Util.h"
#include "Motion.h"

#include <iostream>
#include <cmath>

// podaci o 8 sjedista u vagonu
struct Seat {
    float localX;    // lokalna pozicija u odnosu na centar vagona (NDC)
    float localY;    // lokalna pozicija u odnosu na centar vagona (NDC)
    bool occupied;   // da li postoji putnik
    bool beltOn;     // da li je pojas zakopcan
    bool  sick;      // da li mu je lose 
};

static const int SEAT_COUNT = 8;
static Seat seats[SEAT_COUNT];

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
static unsigned int basicShader;
static unsigned int trackShader;

// uniforme
static int uTrackColorLocation = -1;
static int uOffsetLocation = -1;
static int uTexLocation = -1;
static int uAlphaLocation = -1;
static int uAngleLocation = -1;

// VAO i VBO
static unsigned int VAO;           // vagon (segment jednog sjedista)
static unsigned int VBO;
static unsigned int VAOTrack;      // pruga
static unsigned int VBOTrack;
static unsigned int VAOSupports;   // stubovi
static unsigned int VBOSupports;
static unsigned int VAONameplate;  // nameplate
static unsigned int VBONameplate;
static unsigned int VAOPassenger;  // putnik/pojas
static unsigned int VBOPassenger;

// geometrija vagona
// visina vagona na ekranu
static float cartHalfHeight = 0.21f;
// faktor koliko je sirina veca od visine (npr. 3x)
static float cartShapeRatio = 1.7f;
// sirina korigovana aspect-om i oblikom
static float cartHalfWidth = 0.0f;

// geometrija malog kvadrata vozila (segment ispod jednog sjedista)
static float wagonSegmentHalfWidth = 0.0f;
static float wagonSegmentHalfHeight = 0.0f;

// geometrija putnika
// putnik malo uzi od vagona
static float passengerHalfWidth = 0.0f;
static float passengerHalfHeight = 0.0f;
static float seatStepGlobal = 0.0f;
static bool unloadingPhase = false;  // true kad se voz vratio na pocetak i "ispraznjavamo" putnike
static bool emergencyInProgress = false;

// ------------------ STUBOVI ISPOD PRUGE ------------------

// broj stubova
static const int SUPPORT_COUNT = 35;
static float supportVertices[SUPPORT_COUNT * 4];
// svaki stub ima 2 verteksa: (x_top, y_top), (x_top, y_bottom) => 4 floats

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


// callback funkcije i pomocne za voznju (prototipi)
static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static bool AreAllOccupiedSeatsBelted();

// ------------------  InitScene ------------------

void InitScene(GLFWwindow* window, int screenWidth, int screenHeight)
{
    // cuvamo dimenzije ekrana globalno (za mis u NDC)
    screenWidthGlobal = screenWidth;
    screenHeightGlobal = screenHeight;

    // faktor potreban za dobijanje kvadrata na pravouaonom ekranu (fullscreen mod)
    aspect = static_cast<float>(screenHeightGlobal) / static_cast<float>(screenWidthGlobal);

    // vezivanje callback funkcija za tastaturu i mis
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);

    // ucitavanje custom kursora
    GLFWcursor* customCursor = loadImageToCursor("res/cursor2.png");
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
    preprocessTexture(passengerTexture, "res/passenger.png");
    preprocessTexture(passengerSickTexture, "res/passenger_green.png");
    preprocessTexture(beltTexture, "res/seatbelt.png");

    // kreiranje shadera
    basicShader = createShader("basic.vert", "basic.frag");

    // shader za prugu (samo boja, bez teksture)
    trackShader = createShader("track.vert", "track.frag");
    uTrackColorLocation = glGetUniformLocation(trackShader, "uColor");
    if (uTrackColorLocation == -1) {
        std::cout << "uColor (track) nije pronadjen u shaderu!" << std::endl;
    }

    // pronalazimo lokacije uniformi u basic shaderu
    uOffsetLocation = glGetUniformLocation(basicShader, "uOffset");
    if (uOffsetLocation == -1) {
        std::cout << "uOffset nije pronadjen u shaderu!" << std::endl;
    }

    uTexLocation = glGetUniformLocation(basicShader, "uTex");
    if (uTexLocation == -1) {
        std::cout << "uTex nije pronadjen u shaderu!" << std::endl;
    }

    glUseProgram(basicShader);
    glUniform1i(uTexLocation, 0);  // GL_TEXTURE0

    uAlphaLocation = glGetUniformLocation(basicShader, "uAlpha");
    if (uAlphaLocation == -1) {
        std::cout << "uAlpha nije pronadjen u shaderu!" << std::endl;
    }

    uAngleLocation = glGetUniformLocation(basicShader, "uAngle");
    if (uAngleLocation == -1)
        std::cout << "uAngle nije pronadjen u shaderu!\n";

    // ----------------- Geometrija vozila + 8 sjedista -----------------

    // visina vagona na ekranu
    cartHalfHeight = 0.21f;
    // faktor koliko je sirina veca od visine
    cartShapeRatio = 1.7f;
    // sirina korigovana aspect-om i oblikom
    cartHalfWidth = cartHalfHeight * cartShapeRatio * aspect;

    // ----------------- Inicijalizacija 8 sjedista (1 red) -----------------
    float innerMargin = 0.25f;

    // lijeva i desna granica unutrasnjosti vagona
    float seatsLeftX = -cartHalfWidth * (1.0f - innerMargin);
    float seatsRightX = cartHalfWidth * (1.0f - innerMargin);

    // rucno pomjeranje cijelog reda sjedista malo ulijevo
    float seatShiftX = -0.01f;
    seatsLeftX += seatShiftX;
    seatsRightX += seatShiftX;

    // ukupni span za sjedista
    float seatSpan = seatsRightX - seatsLeftX;

    // razmak izmedju centara sjedista
    // dijeljenje sa SEAT_COUNT i pomjeranje za pola koraka da prvi/poslednji nisu skroz uz ivicu
    float seatStep = seatSpan / SEAT_COUNT;
    seatStepGlobal = seatStep;

    // ----------------- Parametri kompozicije po stazi -----------------
    RC_InitMotion(trackXMin, trackXMax, seatStepGlobal, SEAT_COUNT);

    // visina sjedista unutar vagona (po y-osi)
    float seatsY = cartHalfHeight * 0.3f;

    for (int i = 0; i < SEAT_COUNT; ++i) {
        seats[i].occupied = false;
        seats[i].beltOn = false;
        seats[i].sick = false;  

        // centri: 0.5, 1.5, 2.5, ... , 7.5
        seats[i].localX = seatsLeftX + seatStep * (0.5f + i);
        seats[i].localY = seatsY;
    }

    // ----------------- Geometrija 8 malih kvadrata vozila -----------------
    // svaki kvadrat ce biti centriran ispod jednog sjedista

    // visina kvadrata (po y), u NDC
    wagonSegmentHalfHeight = cartHalfHeight * 0.45f;

    // sirina vezana za razmak izmedju sjedista
    wagonSegmentHalfWidth = seatStepGlobal * 0.7f;

    // x, y, u, v  
    float wagonVertices[] = {
        -wagonSegmentHalfWidth,  wagonSegmentHalfHeight, 0.0f, 1.0f,
        -wagonSegmentHalfWidth, -wagonSegmentHalfHeight, 0.0f, 0.0f,
         wagonSegmentHalfWidth, -wagonSegmentHalfHeight, 1.0f, 0.0f,
         wagonSegmentHalfWidth,  wagonSegmentHalfHeight, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(wagonVertices), wagonVertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ------------------  VAO/VBO za prugu rolerkostera ------------------

    const float* trackVertices = RC_GetTrackVertices();

    // VAO/VBO za prugu (jedna linija)
    glGenVertexArrays(1, &VAOTrack);
    glGenBuffers(1, &VBOTrack);

    glBindVertexArray(VAOTrack);
    glBindBuffer(GL_ARRAY_BUFFER, VBOTrack);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(float) * RC_TRACK_POINT_COUNT * 2,
        trackVertices,
        GL_STATIC_DRAW);

    // atribut 0: pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // deblja linija da izgleda kao sina
    glLineWidth(3.0f);

    // ------------------ STUBOVI ISPOD PRUGE ------------------

    float bottomY = -0.98f;
    const float* tv = RC_GetTrackVertices();

    for (int i = 0; i < SUPPORT_COUNT; ++i) {
        // uzmi tacku sa pruge na odredjenom mjestu
        int   trackIndex = i * (RC_TRACK_POINT_COUNT - 1) / (SUPPORT_COUNT - 1);
        float x_top = tv[trackIndex * 2];
        float y_top = tv[trackIndex * 2 + 1];

        // gornja tacka (na pruzi)
        supportVertices[i * 4 + 0] = x_top;
        supportVertices[i * 4 + 1] = y_top;

        // donja tacka (na dnu konstrukcije)
        supportVertices[i * 4 + 2] = x_top;
        supportVertices[i * 4 + 3] = bottomY;
    }

    // VAO/VBO za stubove (GL_LINES)
    glGenVertexArrays(1, &VAOSupports);
    glGenBuffers(1, &VBOSupports);

    glBindVertexArray(VAOSupports);
    glBindBuffer(GL_ARRAY_BUFFER, VBOSupports);
    glBufferData(GL_ARRAY_BUFFER, sizeof(supportVertices), supportVertices, GL_STATIC_DRAW);

    // atribut 0: pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // VAO i VBO za nameplate (ime u gornjem lijevom uglu) 

    float nameplateVertices[] = {
        -1.0f,  1.0f,   0.0f, 1.0f, // gornje lijevo
        -1.0f,  0.65f,  0.0f, 0.0f, // donje lijevo
        -0.625f,0.65f,  1.0f, 0.0f, // donje desno
        -0.625f,1.0f,   1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &VAONameplate);
    glGenBuffers(1, &VBONameplate);

    glBindVertexArray(VAONameplate);
    glBindBuffer(GL_ARRAY_BUFFER, VBONameplate);
    glBufferData(GL_ARRAY_BUFFER, sizeof(nameplateVertices), nameplateVertices, GL_STATIC_DRAW);

    // pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ------------------ VAO/VBO za putnika/pojas ------------------
    // putnik malo uzi i nizi od samog vagona

    passengerHalfWidth = wagonSegmentHalfWidth * 0.7f;
    passengerHalfHeight = wagonSegmentHalfHeight * 0.6f;

    float passengerVertices[] = {
        -passengerHalfWidth,  passengerHalfHeight, 0.0f, 1.0f, // gornje lijevo
        -passengerHalfWidth, -passengerHalfHeight, 0.0f, 0.0f, // donje lijevo
         passengerHalfWidth, -passengerHalfHeight, 1.0f, 0.0f, // donje desno
         passengerHalfWidth,  passengerHalfHeight, 1.0f, 1.0f  // gornje desno
    };

    glGenVertexArrays(1, &VAOPassenger);
    glGenBuffers(1, &VBOPassenger);

    glBindVertexArray(VAOPassenger);
    glBindBuffer(GL_ARRAY_BUFFER, VBOPassenger);
    glBufferData(GL_ARRAY_BUFFER, sizeof(passengerVertices), passengerVertices, GL_STATIC_DRAW);

    // pozicija
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // tex koordinate
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // vracamo na neki default VAO
    glBindVertexArray(0);
}

// ------------------  KeyCallback ------------------

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
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
        // ne dodaj putnike ako voz vozi ili jos iskrcavamo staru turu
        if (RC_IsRideRunning() || unloadingPhase)
            break;
        // pronadji prvo slobodno sjediste (naprijed ka nazad)
        // punimo od pocetka vagona ka nazad
        for (int i = SEAT_COUNT - 1; i >= 0; --i) {
            if (!seats[i].occupied) {
                seats[i].occupied = true;
                seats[i].beltOn = false;
                seats[i].sick = false;   // novi putnik nije zelen 
                break;
            }
        }
        break;

    case GLFW_KEY_ENTER:
    {
        // pokusaj da pokrenes voznju
        bool canStart = AreAllOccupiedSeatsBelted();
        RC_TryStartRide(canStart);
        break;
    }

    // tasteri 1-8 ce kasnije simulirati signal da se nekom putniku slosilo
    case GLFW_KEY_1:
    case GLFW_KEY_2:
    case GLFW_KEY_3:
    case GLFW_KEY_4:
    case GLFW_KEY_5:
    case GLFW_KEY_6:
    case GLFW_KEY_7:
    case GLFW_KEY_8:
    {
        // ako voz ne vozi ili smo vec u emergency scenariju -> ignorisi signal
        if (!RC_IsRideRunning() || emergencyInProgress)
            return;

        int keyIndex = key - GLFW_KEY_1;

        // 0 = prednje sjediste (seat[7])
        // 7 = zadnje sjediste (seat[0])
        int seatNumber = SEAT_COUNT - 1 - keyIndex;

        // ako to sjediste nije zauzeto -> nista
        if (seatNumber < 0 || seatNumber >= SEAT_COUNT)
            break;
        if (!seats[seatNumber].occupied)
            break;

        // oznaci ga kao "zelenog" i pokreni emergency stop
        seats[seatNumber].sick = true;
        RC_RequestEmergencyStop();

        // ne primamo nove sick signale
        emergencyInProgress = true;
        break;
    }
    }
}

// ------------------  MouseButtonCallback ------------------

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    (void)window;
    (void)mods;

    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    if (RC_IsRideRunning())    // nije dozvoljeno dodavanje/uklanjanje pojaseva u toku voznje
        return;

    // pozicija misa u trenutku klika
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // pretvaranje koordinata misa u NDC (-1,1)
    float mouseNdcX = (float)((mouseX / screenWidthGlobal) * 2.0 - 1.0);
    float mouseNdcY = (float)(1.0 - (mouseY / screenHeightGlobal) * 2.0);

    // prodji kroz sva sjedista i vidi da li je klik unutar "kvadrata putnika"
    for (int i = 0; i < SEAT_COUNT; ++i) {
        if (!seats[i].occupied) continue; // ako nema putnika, nista

        const float wagonYOffset = wagonSegmentHalfHeight;
        const float passengerYOffset = wagonYOffset + wagonSegmentHalfHeight * 0.3f;

        float sx, sy, angle;
        RC_GetSeatBasePosAndAngle(i, sx, sy, angle);

        // centar putnika
        float cx = sx;
        float cy = sy + passengerYOffset;

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
                for (int s = 0; s < SEAT_COUNT; ++s) {
                    if (seats[s].occupied) {
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

// ------------------  Pomocne funkcije za voznju ------------------

// da li su sva zauzeta sjedista vezana
static bool AreAllOccupiedSeatsBelted()
{
    bool anyOccupied = false;

    for (int i = 0; i < SEAT_COUNT; ++i) {
        if (seats[i].occupied) {
            anyOccupied = true;
            if (!seats[i].beltOn) {
                return false; // neko sjedi bez pojasa -> ne smije da krene
            }
        }
    }

    // ne krece ako je prazan
    return anyOccupied;
}

// ------------------  UpdateScene ------------------

void UpdateScene(GLFWwindow* window, double deltaTime)
{
    (void)window;
    RC_Update(deltaTime);

    // ako se voz upravo vratio na pocetak (bilo normalno, bilo poslije emergency)
    if (RC_DidJustReturnToStart()) {
        // svi se automatski odvezu
        for (int i = 0; i < SEAT_COUNT; ++i) {
            if (seats[i].occupied) {
                seats[i].beltOn = false;
                // sick flag ne diramo - ostaju zeleni dok ih ne "skinemo"
            }
        }
        unloadingPhase = true;
        emergencyInProgress = false;   // odradili ture, spremni za novu
    }
}


// ------------------  RenderScene ------------------

void RenderScene()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // crtanje pruge - jedna glatka kriva sa ravnim dijelovima
    glUseProgram(trackShader);
    // tamno siva boja
    glUniform3f(uTrackColorLocation, 0.2f, 0.2f, 0.2f);

    glBindVertexArray(VAOTrack);
    glDrawArrays(GL_LINE_STRIP, 0, RC_TRACK_POINT_COUNT);

    // crtanje stubova ispod pruge (isti shader i boja kao za prugu)
    glBindVertexArray(VAOSupports);
    glDrawArrays(GL_LINES, 0, SUPPORT_COUNT * 2);

    // crtanje vozila
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, wagonTexture);

    glUseProgram(basicShader);
    glUniform1f(uAlphaLocation, 1.0f);

    glBindVertexArray(VAO);

    const float wagonYOffset = wagonSegmentHalfHeight;

    for (int i = 0; i < SEAT_COUNT; ++i) {
        float sx, sy, angle;
        RC_GetSeatBasePosAndAngle(i, sx, sy, angle);

        glUniform1f(uAngleLocation, angle);
        glUniform2f(uOffsetLocation, sx, sy + wagonYOffset);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }


    // crtanje putnika u sjedistima 
    glUseProgram(basicShader);
    glBindVertexArray(VAOPassenger);

    const float passengerYOffset = wagonYOffset + wagonSegmentHalfHeight * 0.3f;

    for (int i = 0; i < SEAT_COUNT; ++i) {
        if (!seats[i].occupied) continue;
        // svako sjediste ima svoju lokalnu poziciju u odnosu na vagon
        float sx, sy, angle;
        RC_GetSeatBasePosAndAngle(i, sx, sy, angle);

        // prvo crtamo putnika
        glActiveTexture(GL_TEXTURE0);
        unsigned int tex = seats[i].sick ? passengerSickTexture : passengerTexture;
        glBindTexture(GL_TEXTURE_2D, tex);

        glUniform1f(uAlphaLocation, 1.0f);
        glUniform1f(uAngleLocation, angle);
        glUniform2f(uOffsetLocation, sx, sy + passengerYOffset);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // ako je pojas zakacen, crtamo pojaseve preko putnika
        if (seats[i].beltOn) {
            glBindTexture(GL_TEXTURE_2D, beltTexture);
            // isti offset, isti VAO, samo druga tekstura
            glUniform1f(uAlphaLocation, 1.0f);
            glUniform1f(uAngleLocation, angle);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }


    // crtanje nameplate-a
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, nameplateTexture);

    glUseProgram(basicShader);

    // nameplate poluprovidan
    glUniform1f(uAlphaLocation, 0.1f);
    // nameplate statican u uglu, bez pomjeranja
    glUniform2f(uOffsetLocation, 0.0f, 0.0f);
    glUniform1f(uAngleLocation, 0.0f);

    glBindVertexArray(VAONameplate);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
}

// ------------------  CleanupScene ------------------

void CleanupScene()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glDeleteVertexArrays(1, &VAOTrack);
    glDeleteBuffers(1, &VBOTrack);

    glDeleteVertexArrays(1, &VAOSupports);
    glDeleteBuffers(1, &VBOSupports);

    glDeleteVertexArrays(1, &VAONameplate);
    glDeleteBuffers(1, &VBONameplate);

    glDeleteVertexArrays(1, &VAOPassenger);
    glDeleteBuffers(1, &VBOPassenger);

    glDeleteProgram(basicShader);
    glDeleteProgram(trackShader);
}
