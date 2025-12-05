#define _USE_MATH_DEFINES
#include "Scene.h"
#include "Util.h"

#include <iostream>
#include <cmath>

// pomjeraj kvadrata (vagona) po x i y osi
static float offsetX = 0.0f;
static float offsetY = 0.0f;

static bool spaceWasPressedLastFrame = false;
static bool leftMouseWasPressedLastFrame = false;
static bool enterWasPressedLastFrame = false;

// za kasnije (kretanje), za sad samo flag
static bool isRideRunning = false;

// podaci o 8 sjedista u vagonu
struct Seat {
    float localX;    // lokalna pozicija u odnosu na centar vagona (NDC)
    float localY;    // lokalna pozicija u odnosu na centar vagona (NDC)
    bool occupied;   // da li postoji putnik
    bool beltOn;     // da li je pojas zakopcan
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
static unsigned int beltTexture;

// sejderi
static unsigned int basicShader;
static unsigned int trackShader;

// uniforme
static int uTrackColorLocation = -1;
static int uOffsetLocation = -1;
static int uTexLocation = -1;
static int uAlphaLocation = -1;

// VAO i VBO
static unsigned int VAO;           // vagon
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
// putnik malo uzi od razmaka izmedju sjedista
static float passengerHalfWidth = 0.0f;
static float passengerHalfHeight = 0.0f;
static float seatStepGlobal = 0.0f;

// ------------------  VAO/VBO za prugu rolerkostera (Bezier sa C1 kontinuitetom) ------------------

static const int TRACK_SEGMENTS = 1500; // povecan broj segmenata za glatku krivu
static const int TRACK_POINT_COUNT = TRACK_SEGMENTS + 1;
static float trackVertices[(TRACK_SEGMENTS + 1) * 2];

// ------------------ STUBOVI ISPOD PRUGE ------------------

// broj stubova
static const int SUPPORT_COUNT = 35;
static float supportVertices[SUPPORT_COUNT * 4];
// svaki stub ima 2 verteksa: (x_top, y_top), (x_top, y_bottom) => 4 floats

// ------------------ POMOCNE FUNKCIJE ------------------

// ucitavanje teksture i podesavanje parametara
void preprocessTexture(unsigned& texture, const char* filepath) {
    texture = loadImageToTexture(filepath); // ucitavanje teksture
    glBindTexture(GL_TEXTURE_2D, texture); // vezujemo se za teksturu kako bismo je podesili

    // generisanje mipmapa - predefinisani razliciti formati za lakše skaliranje po potrebi
    glGenerateMipmap(GL_TEXTURE_2D);

    // podesavanje strategija za wrap-ovanje
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S - tekseli po x-osi
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T - tekseli po y-osi

    // podesavanje algoritma za smanjivanje i povecavanje rezolucije: nearest - bira najblizi piksel, linear - usrednjava okolne piksele
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// pomocna funkcija: kubna Bezier kriva za y koordinatu 
static float bezierY(float t, float p0, float p1, float p2, float p3) {
    float u = 1.0f - t;
    return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
}

// ------------------  InitScene ------------------

void InitScene(GLFWwindow* window, int screenWidth, int screenHeight)
{
    // cuvamo dimenzije ekrana globalno (za mis u NDC)
    screenWidthGlobal = screenWidth;
    screenHeightGlobal = screenHeight;

    // faktor potreban za dobijanje kvadrata na pravouaonom ekranu (fullscreen mod)
    aspect = static_cast<float>(screenHeightGlobal) / static_cast<float>(screenWidthGlobal);

    // ucitavanje custom kursora
    GLFWcursor* customCursor = loadImageToCursor("res/cursor2.png");
    if (customCursor != nullptr) {
        glfwSetCursor(window, customCursor);
        std::cout << "Custom kursor uspjesno postavljen." << std::endl;
    }
    else {
        std::cout << "Custom kursor NIJE postavljen." << std::endl;
    }

    // ucitavanje teksture vagona
    preprocessTexture(wagonTexture, "res/cart_one.png");

    // ucitavanje teksture nameplatea
    preprocessTexture(nameplateTexture, "res/nameplate1.png");

    // ucitavanje teksture putnika
    preprocessTexture(passengerTexture, "res/passenger.png");

    // ucitavanje teksture pojasa
    preprocessTexture(beltTexture, "res/seatbelt.png");

    // kreiranje shadera
    basicShader = createShader("basic.vert", "basic.frag");

    // shader za prugu (samo boja, bez teksture)
    trackShader = createShader("track.vert", "track.frag");
    uTrackColorLocation = glGetUniformLocation(trackShader, "uColor");
    if (uTrackColorLocation == -1) {
        std::cout << "uColor (track) nije pronadjen u shaderu!" << std::endl;
    }

    // pronalazimo lokaciju uniforme uOffset u shaderu
    uOffsetLocation = glGetUniformLocation(basicShader, "uOffset");
    if (uOffsetLocation == -1) {
        std::cout << "uOffset nije pronadjen u shaderu!" << std::endl;
    }

    // uniform za teksturu (sampler2D)
    uTexLocation = glGetUniformLocation(basicShader, "uTex");
    if (uTexLocation == -1) {
        std::cout << "uTex nije pronadjen u shaderu!" << std::endl;
    }

    // podesimo da uTex koristi teksturnu jedinicu 0
    glUseProgram(basicShader);
    glUniform1i(uTexLocation, 0);  // GL_TEXTURE0

    // uniforma za dodatnu providnost
    uAlphaLocation = glGetUniformLocation(basicShader, "uAlpha");
    if (uAlphaLocation == -1) {
        std::cout << "uAlpha nije pronadjen u shaderu!" << std::endl;
    }

    // ----------------- Geometrija vozila + 8 sjedista -----------------

// visina vagona na ekranu
    cartHalfHeight = 0.21f;

    // faktor koliko je sirina veca od visine (npr. 3x)
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

    // visina sjedista unutar vagona (po y-osi)
    float seatsY = cartHalfHeight * 0.3f;

    for (int i = 0; i < SEAT_COUNT; ++i) {
        seats[i].occupied = false;
        seats[i].beltOn = false;

        // centri: 0.5, 1.5, 2.5, ... , 7.5
        seats[i].localX = seatsLeftX + seatStep * (0.5f + i);
        seats[i].localY = seatsY;
    }

    // ----------------- Geometrija 8 malih kvadrata vozila -----------------
    // svaki kvadrat ce biti centriran ispod jednog sjedista

    // visina kvadrata (po y), u NDC
    wagonSegmentHalfHeight = cartHalfHeight * 0.45f;

    // da kvadrat stvarno izgleda kvadratno na pravougaonom ekranu, sirinu skaliramo sa aspect-om
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

    // ------------------  VAO/VBO za prugu rolerkostera (Bezier sa C1 kontinuitetom) ------------------

    // kontrolne tacke za cijelu stazu
    // definisanje unaprijed, osiguravanje C1 kontinuet

    // y-koordinate kljucnih tacaka (P0 i P3 za svaki segment)
    float y_start = -0.5f;      // pocetak staze (lijevo)
    float y_mid_valley1 = -0.3f; // prva dolina
    float y_peak1 = 0.2f;       // prvi vrh
    float y_mid_valley2 = -0.4f; // druga dublja dolina
    float y_peak2 = 0.5f;       // drugi visi vrh
    float y_mid_valley3 = -0.2f; // treca dolina
    float y_peak3 = 0.3f;       // treci vrh
    float y_end = -0.02f;       // kraj staze (desno)

    // odredjivanje X-pozicija segmenata
    // totalni opseg X je od -1.0 do 1.0
    // podijelicemo ga na 7 segmenata (jer imamo 8 kljucnih Y tacaka)
    float x_segments[] = {
        0.0f,  // t=0.0 -> x=-1.0
        0.15f, // kraj prvog segmenta (uspon ka peak1)
        0.35f, // kraj drugog segmenta (pad u valley2)
        0.50f, // kraj treceg segmenta (uspon ka peak2)
        0.65f, // kraj cetvrtog segmenta (pad u valley3)
        0.80f, // kraj petog segmenta (uspon ka peak3)
        0.95f, // kraj sestog segmenta (pad ka kraju)
        1.0f   // t=1.0 -> x=1.0
    };

    // kljucne y koordinate za C1 kontinuitet
    // ove P0 i P3 ce biti spojevi segmenata
    // P1_next = P0_next + (P0_next - P2_current)

    // pocetne tacke
    float current_P0 = y_start;
    float current_P1 = y_start + 0.05f; // blago nagore, glatki start
    float current_P2 = y_start + 0.15f; // dalje od P1 za sirinu
    float current_P3;

    for (int i = 0; i <= TRACK_SEGMENTS; ++i) {
        float t = i / static_cast<float>(TRACK_SEGMENTS); // t_global od 0 do 1
        float x = -1.0f + 2.0f * t;
        float y;

        // segment 1: start -> prvi brijeg (y_start do y_peak1)
        if (t <= x_segments[1]) {
            float u = t / x_segments[1]; // lokalni t za segment
            current_P0 = y_start;
            current_P1 = y_start + 0.1f; // kontrola nagiba od starta
            current_P2 = y_peak1 - 0.1f; // kontrola nagiba ka vrhu
            current_P3 = y_peak1;       // kraj prvog segmenta
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // segment 2: prvi brijeg -> prva dolina (y_peak1 do y_mid_valley2)
        else if (t <= x_segments[2]) {
            float u = (t - x_segments[1]) / (x_segments[2] - x_segments[1]);
            current_P0 = y_peak1;
            // P1 = P0 + (P0 - P2_prev)
            current_P1 = current_P0 + (current_P0 - (y_peak1 - 0.1f)); // C1 kontinuitet
            current_P2 = y_mid_valley2 + 0.1f; // kontrola nagiba ka dolini
            current_P3 = y_mid_valley2;       // kraj drugog segmenta
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // segment 3: prva dolina -> drugi brijeg (y_mid_valley2 do y_peak2)
        else if (t <= x_segments[3]) {
            float u = (t - x_segments[2]) / (x_segments[3] - x_segments[2]);
            current_P0 = y_mid_valley2;
            current_P1 = current_P0 + (current_P0 - (y_mid_valley2 + 0.1f)); // C1 kontinuitet
            current_P2 = y_peak2 - 0.15f; // kontrola nagiba ka visem vrhu
            current_P3 = y_peak2;         // kraj treceg segmenta
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // segment 4: drugi brijeg -> druga dolina (y_peak2 do y_mid_valley3)
        else if (t <= x_segments[4]) {
            float u = (t - x_segments[3]) / (x_segments[4] - x_segments[3]);
            current_P0 = y_peak2;
            current_P1 = current_P0 + (current_P0 - (y_peak2 - 0.15f)); // C1 kontinuitet
            current_P2 = y_mid_valley3 + 0.05f; // kontrola nagiba ka dolini
            current_P3 = y_mid_valley3;         // kraj cetvrtog segmenta
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // segment 5: druga dolina -> treci brijeg (y_mid_valley3 do y_peak3)
        else if (t <= x_segments[5]) {
            float u = (t - x_segments[4]) / (x_segments[5] - x_segments[4]);
            current_P0 = y_mid_valley3;
            current_P1 = current_P0 + (current_P0 - (y_mid_valley3 + 0.05f)); // C1 kontinuitet
            current_P2 = y_peak3 - 0.1f; // kontrola nagiba ka trecem vrhu
            current_P3 = y_peak3;       // kraj petog segmenta
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // segment 6: treci brijeg -> kraj (y_peak3 do y_end)
        else { // t > x_segments[5]
            float u = (t - x_segments[5]) / (x_segments[6] - x_segments[5]); // do kraja
            current_P0 = y_peak3;
            current_P1 = current_P0 + (current_P0 - (y_peak3 - 0.1f)); // C1 kontinuitet
            current_P2 = y_end + 0.05f; // kontrola nagiba ka kraju
            current_P3 = y_end;         // kraj staze
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }

        trackVertices[i * 2] = x;
        trackVertices[i * 2 + 1] = y;
    }

    // VAO/VBO za prugu (jedna linija)
    glGenVertexArrays(1, &VAOTrack);
    glGenBuffers(1, &VBOTrack);

    glBindVertexArray(VAOTrack);
    glBindBuffer(GL_ARRAY_BUFFER, VBOTrack);
    glBufferData(GL_ARRAY_BUFFER, sizeof(trackVertices), trackVertices, GL_STATIC_DRAW);

    // atribut 0: pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // deblja linija da izgleda kao sina
    glLineWidth(3.0f);

    // ------------------ STUBOVI ISPOD PRUGE ------------------

    float bottomY = -0.98f;

    for (int i = 0; i < SUPPORT_COUNT; ++i) {
        // uzmi tacku sa pruge na odredjenom mjestu
        int trackIndex = i * (TRACK_POINT_COUNT - 1) / (SUPPORT_COUNT - 1);
        float x_top = trackVertices[trackIndex * 2];
        float y_top = trackVertices[trackIndex * 2 + 1];

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

// ------------------  UpdateScene ------------------

void UpdateScene(GLFWwindow* window, double deltaTime)
{
    // SPACE: dodavanje novog putnika (edge detection)
    int spaceState = glfwGetKey(window, GLFW_KEY_SPACE);
    if (spaceState == GLFW_PRESS && !spaceWasPressedLastFrame) {
        // pronadji prvo slobodno sjediste (naprijed ka nazad)
        // punimo od pocetka vagona ka nazad
        for (int i = SEAT_COUNT - 1; i >= 0; --i) {
            if (!seats[i].occupied) {
                seats[i].occupied = true;
                seats[i].beltOn = false;
                break;
            }
        }
    }
    spaceWasPressedLastFrame = (spaceState == GLFW_PRESS);

    // --- Lijevi klik misa: vezivanje/otkopcavanje pojasa na pojedinacnom sjedistu ---
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // pretvaranje koordinata misa u NDC (-1,1)
    float mouseNdcX = (float)((mouseX / screenWidthGlobal) * 2.0 - 1.0);
    float mouseNdcY = (float)(1.0 - (mouseY / screenHeightGlobal) * 2.0);

    int leftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (leftState == GLFW_PRESS && !leftMouseWasPressedLastFrame) {
        // prodji kroz sva sjedista i vidi da li je klik unutar "kvadrata putnika"
        for (int i = 0; i < SEAT_COUNT; ++i) {
            if (!seats[i].occupied) continue; // ako nema putnika, nista

            float cx = offsetX + seats[i].localX;
            float cy = offsetY + seats[i].localY;

            if (mouseNdcX >= cx - passengerHalfWidth && mouseNdcX <= cx + passengerHalfWidth &&
                mouseNdcY >= cy - passengerHalfHeight && mouseNdcY <= cy + passengerHalfHeight) {

                // toggle pojasa za tog putnika
                seats[i].beltOn = !seats[i].beltOn;
                break; // samo jedan putnik po kliku
            }
        }
    }
    leftMouseWasPressedLastFrame = (leftState == GLFW_PRESS);

    // pomjeranje kvadrata - skalirano deltaTime-om 
    float speed = 0.5f; // jedinica u sekundi
    float velocity = speed * static_cast<float>(deltaTime);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        offsetX -= velocity; // lijevo
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        offsetX += velocity; // desno
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        offsetY += velocity; // gore
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        offsetY -= velocity; // dole
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
    glDrawArrays(GL_LINE_STRIP, 0, TRACK_POINT_COUNT);

    // crtanje stubova ispod pruge (isti shader i boja kao za prugu)
    glBindVertexArray(VAOSupports);
    glDrawArrays(GL_LINES, 0, SUPPORT_COUNT * 2);


    // biramo teksturnu jedinicu 0 i vezujemo teksturu vagona
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, wagonTexture);

    // crtanje vozila: 8 malih kvadrata, po jedan ispod svakog sjedista
    glUseProgram(basicShader); // koristi shader 

    // vagon je potpuno neprovidan
    glUniform1f(uAlphaLocation, 1.0f);

    glBindVertexArray(VAO);    // koristi VAO sa jednim malim kvadratom

    for (int i = 0; i < SEAT_COUNT; ++i) {
        // svaki segment ce biti centriran oko istih lokalnih koordinata kao sjediste
        float worldX = offsetX + seats[i].localX;
        float worldY = offsetY + seats[i].localY;

        glUniform2f(uOffsetLocation, worldX, worldY); // slanje offseta u shader
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);          // 4 verteksa kao kvadrat
    }


    // crtanje putnika u sjedistima 
    glUseProgram(basicShader);
    glBindVertexArray(VAOPassenger);

    for (int i = 0; i < SEAT_COUNT; ++i) {
        if (!seats[i].occupied) continue;

        // svako sjediste ima svoju lokalnu poziciju u odnosu na vagon
        float worldX = offsetX + seats[i].localX;
        float worldY = offsetY + seats[i].localY;

        // prvo crtamo putnika
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, passengerTexture);

        glUniform1f(uAlphaLocation, 1.0f);        // pun prikaz
        glUniform2f(uOffsetLocation, worldX, worldY);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // ako je pojas zakacen, crtamo pojaseve preko putnika
        if (seats[i].beltOn) {
            glBindTexture(GL_TEXTURE_2D, beltTexture);
            // isti offset, isti VAO, samo druga tekstura
            glUniform1f(uAlphaLocation, 1.0f);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }


    // crtanje nameplate-a
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, nameplateTexture);

    glUseProgram(basicShader);

    // nameplate poluprovidan
    glUniform1f(uAlphaLocation, 0.1f);

    // nameplate statican u uglu, bez pomjeranja WASD-om
    glUniform2f(uOffsetLocation, 0.0f, 0.0f);

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
