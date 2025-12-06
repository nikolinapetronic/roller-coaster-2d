#define _USE_MATH_DEFINES
#include "Motion.h"

#include <cmath>

// ------------------ KONSTANTE ------------------
// broj segmenata pruge i izvedeni broj tacaka
// (ukupno se crta RC_TRACK_POINT_COUNT tacaka)
const int RC_TRACK_SEGMENTS = 2000;
const int RC_TRACK_POINT_COUNT = RC_TRACK_SEGMENTS + 1;

// brzina ubrzavanja i maksimalna brzina voznje
static const float RIDE_ACCEL = 0.02f;
static const float RIDE_MAX_SPEED = 0.15f; 

static const float RIDE_SLOPE_ACCEL = 1.2f;   // koliko jako nagib utice na ubrzanje
static const float RIDE_MIN_SPEED = 0.01f;  // minimalna brzina koja se smatra kretanjem
static const float RIDE_MAX_SLOPE_SPEED = 0.60f; // apsolutni max, i nizbrdo

// ------------------ GLOBALNO STANJE MODULA ------------------
// opseg pruge po x-osi (NDC)
static float g_trackXMin = -0.9f;
static float g_trackXMax = 0.9f;

// niz verteksa za prugu (x,y) -> 2 float-a po tacki
static float g_trackVertices[RC_TRACK_POINT_COUNT * 2];
static float g_trackArcLengths[RC_TRACK_POINT_COUNT];
static float g_totalTrackLength = 0.0f;

// trenutno stanje voznje po parametru (0-1)
static float g_trackParam = 0.0f;
static float g_rideSpeed = 0.0f;
static float g_cartAngle = 0.0f;
static bool  g_isRideRunning = false;

static int   g_seatCount = 0;
static float g_seatStepNDC = 0.0f;  // razmak sjedista u NDC (po X)
static float g_seatStepLen = 0.0f;  // realna duzina izmedju sjedista po stazi
static float g_trainLen = 0.0f;  // realna duzina cijele kompozicije
static float g_trackParamStart = 0.0f;
static float g_trackParamEnd = 1.0f;

// ------------------ POMOCNE FUNKCIJE ---------------------------

// osnovni Bezier cubic evaluator za Y koordinatu pruge
static float BezierY(float t, float p0, float p1, float p2, float p3)
{
    float u = 1.0f - t;
    return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
}

// uzorkovanje pruge prema parametru (0-1)
// vraca interpolisani (x,y) i ugao tangente (nagib pruge)
// ugao se ogranicava zbog stabilnosti
static void SampleTrack(float param, float& outX, float& outY, float& outAngle)
{
    // clamp parametra
    if (param < 0.0f) param = 0.0f;
    if (param > 1.0f) param = 1.0f;

    float fIndex = param * (RC_TRACK_POINT_COUNT - 1);
    int i0 = (int)std::floor(fIndex);
    int i1 = i0 + 1;
    if (i1 >= RC_TRACK_POINT_COUNT) i1 = RC_TRACK_POINT_COUNT - 1;

    float localT = fIndex - (float)i0;

    // uzimamo dvije susjedne tacke za linearnu interpolaciju
    float x0 = g_trackVertices[i0 * 2 + 0];
    float y0 = g_trackVertices[i0 * 2 + 1];
    float x1 = g_trackVertices[i1 * 2 + 0];
    float y1 = g_trackVertices[i1 * 2 + 1];

    float dx = x1 - x0;
    float dy = y1 - y0;

    // ugao tangente pruge
    float angle = std::atan2(dy, dx);

    // ogranicenje maksimalnog nagiba da se kompozicija ne "prevrce"
    const float maxAngleDeg = 20.0f;
    const float maxAngleRad = maxAngleDeg * (float)M_PI / 180.0f;
    if (angle > maxAngleRad) angle = maxAngleRad;
    if (angle < -maxAngleRad) angle = -maxAngleRad;

    // ublazavanje efekta da ne bude previse agresivan vizuelno
    angle *= 0.5f;

    // interpolirano X/Y na segmentu
    float x = (1.0f - localT) * x0 + localT * x1;
    float y = (1.0f - localT) * y0 + localT * y1;

    outX = x;
    outY = y;
    outAngle = angle;
}

// nalazi parametar za datu DUZINU od pocetka staze (0 je pocetak)
static float GetParamAtArcLengthFromStart(float targetLen)
{
    if (targetLen <= 0.0f)            return 0.0f;
    if (targetLen >= g_totalTrackLength) return 1.0f;

    int i = 1;
    while (i < RC_TRACK_POINT_COUNT && g_trackArcLengths[i] < targetLen)
        ++i;

    int i0 = i - 1;
    int i1 = i;
    if (i1 >= RC_TRACK_POINT_COUNT) i1 = RC_TRACK_POINT_COUNT - 1;

    float l0 = g_trackArcLengths[i0];
    float l1 = g_trackArcLengths[i1];
    float t = (l1 > l0) ? (targetLen - l0) / (l1 - l0) : 0.0f;

    return (i0 + t) / (float)(RC_TRACK_POINT_COUNT - 1);
}

// vraca parametar koji je "backDistance" unazad od currentParam po REALNOJ duzini
static float GetParamAtArcLengthBackwards(float currentParam, float backDistance)
{
    // trenutna duzina od pocetka
    float currentIndexF = currentParam * (RC_TRACK_POINT_COUNT - 1);
    int   currentIndex = (int)std::floor(currentIndexF);
    if (currentIndex < 0) currentIndex = 0;
    if (currentIndex >= RC_TRACK_POINT_COUNT) currentIndex = RC_TRACK_POINT_COUNT - 1;

    float currentLength = g_trackArcLengths[currentIndex];
    float targetLen = currentLength - backDistance;
    if (targetLen < 0.0f) targetLen = 0.0f;

    return GetParamAtArcLengthFromStart(targetLen);
}

// ------------------------ API ---------------------------------

void RC_InitMotion(float trackXMin,
    float trackXMax,
    float seatStep,
    int   seatCount)
{
    g_trackXMin = trackXMin;
    g_trackXMax = trackXMax;
    g_seatCount = seatCount;
    g_seatStepNDC = seatStep;

    // --------- generisanje pruge i akumulisane duzine ---------
    float y_start = -0.5f;
    float y_mid_valley1 = -0.3f;
    float y_peak1 = 0.2f;
    float y_mid_valley2 = -0.4f;
    float y_peak2 = 0.5f;
    float y_mid_valley3 = -0.2f;
    float y_peak3 = 0.3f;
    float y_end = -0.02f;

    // segmenti po X - gdje prelazimo na naredni Bezier
    float x_segments[] = {
        0.0f,
        0.15f,
        0.35f,
        0.50f,
        0.65f,
        0.80f,
        0.95f,
        1.0f
    };

    float current_P0 = y_start;
    float current_P1 = y_start + 0.05f;
    float current_P2 = y_start + 0.15f;
    float current_P3;

    float prevX = 0.0f, prevY = 0.0f;
    g_totalTrackLength = 0.0f;

    for (int i = 0; i <= RC_TRACK_SEGMENTS; ++i)
    {
        float t = i / (float)RC_TRACK_SEGMENTS;
        float x = g_trackXMin + (g_trackXMax - g_trackXMin) * t;
        float y;

        // segment 0 -> peak1
        if (t <= x_segments[1]) {
            float u = t / x_segments[1];
            current_P0 = y_start;
            current_P1 = y_start + 0.1f;
            current_P2 = y_peak1 - 0.1f;
            current_P3 = y_peak1;
            y = BezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // peak1 -> valley2
        else if (t <= x_segments[2]) {
            float u = (t - x_segments[1]) / (x_segments[2] - x_segments[1]);
            current_P0 = y_peak1;
            current_P1 = current_P0 + (current_P0 - (y_peak1 - 0.1f));
            current_P2 = y_mid_valley2 + 0.1f;
            current_P3 = y_mid_valley2;
            y = BezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // valley2 -> peak2
        else if (t <= x_segments[3]) {
            float u = (t - x_segments[2]) / (x_segments[3] - x_segments[2]);
            current_P0 = y_mid_valley2;
            current_P1 = current_P0 + (current_P0 - (y_mid_valley2 + 0.1f));
            current_P2 = y_peak2 - 0.15f;
            current_P3 = y_peak2;
            y = BezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // peak2 -> valley3
        else if (t <= x_segments[4]) {
            float u = (t - x_segments[3]) / (x_segments[4] - x_segments[3]);
            current_P0 = y_peak2;
            current_P1 = current_P0 + (current_P0 - (y_peak2 - 0.15f));
            current_P2 = y_mid_valley3 + 0.05f;
            current_P3 = y_mid_valley3;
            y = BezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // valley3 -> peak3
        else if (t <= x_segments[5]) {
            float u = (t - x_segments[4]) / (x_segments[5] - x_segments[4]);
            current_P0 = y_mid_valley3;
            current_P1 = current_P0 + (current_P0 - (y_mid_valley3 + 0.05f));
            current_P2 = y_peak3 - 0.1f;
            current_P3 = y_peak3;
            y = BezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // peak3 -> end
        else {
            float u = (t - x_segments[5]) / (x_segments[6] - x_segments[5]);
            current_P0 = y_peak3;
            current_P1 = current_P0 + (current_P0 - (y_peak3 - 0.1f));
            current_P2 = y_end + 0.05f;
            current_P3 = y_end;
            y = BezierY(u, current_P0, current_P1, current_P2, current_P3);
        }

        // upisujemo tacku u globalni niz verteksa
        g_trackVertices[i * 2 + 0] = x;
        g_trackVertices[i * 2 + 1] = y;

        if (i == 0) {
            g_trackArcLengths[i] = 0.0f;
            prevX = x;
            prevY = y;
        }
        else {
            float dx = x - prevX;
            float dy = y - prevY;
            float segLen = std::sqrt(dx * dx + dy * dy);
            g_totalTrackLength += segLen;
            g_trackArcLengths[i] = g_totalTrackLength;
            prevX = x;
            prevY = y;
        }
    }

    // --------- razmak sjedista po realnoj duzini ----------
    float ndcTrackWidth = g_trackXMax - g_trackXMin;
    float seatStepNorm = g_seatStepNDC / ndcTrackWidth;      // u odnosu na sirinu po X
    g_seatStepLen = seatStepNorm * g_totalTrackLength;  // projekcija na realnu stazu
    g_trainLen = g_seatStepLen * (g_seatCount - 1);  // ukupna duzina kompozicije

    // zadnji vagon (seatIndex 0) -> pocetak pruge (duzina = 0)
    // prvi vagon (seatIndex 7)   -> na duzini g_trainLen od pocetka
    float frontLen = g_trainLen;
    g_trackParamStart = GetParamAtArcLengthFromStart(frontLen);
    g_trackParamEnd = 1.0f;
    g_trackParam = g_trackParamStart;

    g_rideSpeed = 0.0f;
    g_isRideRunning = false;
    g_cartAngle = 0.0f;
}

// stanje
bool RC_IsRideRunning()
{
    return g_isRideRunning;
}

// pokusavamo pokrenuti voznju samo ako:
// - trenutno ne vozi
// - canStart je true (provjera iz Scene.cpp)
void RC_TryStartRide(bool canStart)
{
    if (g_isRideRunning)
        return;
    if (!canStart)
        return;

    // cijeli voz staje na prugu pa pocinjemo od g_trackParamStart
    g_trackParam = g_trackParamStart;
    g_rideSpeed = 0.0f;
    g_isRideRunning = true;
}

// glavni update
void RC_Update(double deltaTime)
{
    if (!g_isRideRunning)
        return;

    // osnovno ubrzavanje: iz mirovanja do "krstarece" brzine na ravnom
    g_rideSpeed += RIDE_ACCEL * (float)deltaTime;
    if (g_rideSpeed > RIDE_MAX_SPEED)
        g_rideSpeed = RIDE_MAX_SPEED;

    // --- SMOOTH NAGIBA OKO TRENUTNE POZICIJE -----
    float xCurr, yCurr, angleCurr;
    SampleTrack(g_trackParam, xCurr, yCurr, angleCurr);

    float xA, yA, angleAhead;
    float xB, yB, angleBehind;

    const float paramOffset = 0.002f; 

    // malo ispred
    float paramAhead = g_trackParam + paramOffset;
    if (paramAhead > 1.0f) paramAhead = 1.0f;
    SampleTrack(paramAhead, xA, yA, angleAhead);

    // malo iza
    float paramBehind = g_trackParam - paramOffset;
    if (paramBehind < 0.0f) paramBehind = 0.0f;
    SampleTrack(paramBehind, xB, yB, angleBehind);

    // prosjek tri ugla -> zagladjen nagib
    float smoothAngle = (angleBehind + angleCurr + angleAhead) / 3.0f;

    // koristimo zagladjeni ugao umjesto "skacuceg"
    float slopeFactor = std::sin(smoothAngle);

    // nizbrdo: slopeFactor < 0 -> -slopeFactor > 0 -> ubrzavamo
    // uzbrdo: slopeFactor > 0 -> -slopeFactor < 0 -> usporavamo
    g_rideSpeed += RIDE_SLOPE_ACCEL * (-slopeFactor) * (float)deltaTime;

    // ogranicenja brzine
    if (g_rideSpeed < RIDE_MIN_SPEED)
        g_rideSpeed = RIDE_MIN_SPEED;
    if (g_rideSpeed > RIDE_MAX_SLOPE_SPEED)
        g_rideSpeed = RIDE_MAX_SLOPE_SPEED;

    // napredovanje po pruzi sa finalnom brzinom
    g_trackParam += g_rideSpeed * (float)deltaTime;

    // ako smo stigli do kraja pruge -> zaustavi
    if (g_trackParam >= g_trackParamEnd) {
        g_trackParam = g_trackParamEnd;
        g_rideSpeed = 0.0f;
        g_isRideRunning = false;
    }

    // ugao za crtanje vagona
    float x, y, angle;
    SampleTrack(g_trackParam, x, y, angle);
    g_cartAngle = angle;
}

// bazna pozicija sjedista N, ukljucujuci ugao pruge u toj tacki
void RC_GetSeatBasePosAndAngle(int seatIndex,
    float& sx,
    float& sy,
    float& angle)
{
    // seatIndex 0  -> zadnji vagon (najblize pocetku)
    // seatIndex 7  -> prvi vagon (naprijed)
    int seatOrderIndex = g_seatCount - 1 - seatIndex;
    // sjediste je pomjereno unazad po parametru
    float backDist = seatOrderIndex * g_seatStepLen;
    float seatParam = GetParamAtArcLengthBackwards(g_trackParam, backDist);

    // uzorkujemo tacku na pruzi
    SampleTrack(seatParam, sx, sy, angle);
}

// vracamo pointer na sve tacke pruge (za OpenGL VBO/VAO)
const float* RC_GetTrackVertices()
{
    return g_trackVertices;
}
