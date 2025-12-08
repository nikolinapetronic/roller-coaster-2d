#define _USE_MATH_DEFINES
#include "RideMotion.h"

#include <cmath>

// ------------------ KONSTANTE ------------------

enum class RideState {
    AtStartIdle,
    RunningForward,
    EmergencyStopping,
    StoppedForSick,
    ReturningToStart
};

static RideState state = RideState::AtStartIdle;

static bool  emergencyRequested = false;
static bool  justReturnedToStart = false;

static const float EMERGENCY_DECEL = 0.20f;  // koliko brzo koci kad se nekom slosi
static const float RETURN_SPEED = 0.04f;  // mala konst. brzina nazad
static const float EMERGENCY_STOP_DURATION = 10.0f;  // 10 sekundi pauze

static double stopTimer = 0.0;

// brzina ubrzavanja i maksimalna brzina voznje
static const float RIDE_ACCEL = 0.08f;
static const float RIDE_MAX_SPEED = 0.15f; 

static const float RIDE_SLOPE_ACCEL = 0.90f;   // koliko jako nagib utice na ubrzanje
static const float RIDE_MIN_SPEED = 0.06f;  // minimalna brzina koja se smatra kretanjem
static const float RIDE_MAX_SLOPE_SPEED = 0.60f; // apsolutni max, i nizbrdo

// ------------------ GLOBALNO STANJE MODULA ------------------
// opseg pruge po x-osi (NDC)
static float trackXMin = -0.9f;
static float trackXMax = 0.9f;

// niz verteksa za prugu (x,y) -> 2 float-a po tacki
static float trackVertices[TRACK_POINT_COUNT * 2];
static float trackArcLengths[TRACK_POINT_COUNT];
static float totalTrackLength = 0.0f;

// trenutno stanje voznje po parametru (0-1)
static float trackParam = 0.0f;
static float rideSpeed = 0.0f;
static float cartAngle = 0.0f;
static bool  isRideOngoing = false;

static int   seatCount = 0;
static float seatStepNDC = 0.0f;  // razmak sjedista u NDC (po X)
static float seatStepLen = 0.0f;  // realna duzina izmedju sjedista po stazi
static float trainLen = 0.0f;  // realna duzina cijele kompozicije
static float trackParamStart = 0.0f;
static float trackParamEnd = 1.0f;

// ------------------ POMOCNE FUNKCIJE ---------------------------

// osnovni Bezier cubic evaluator za Y koordinatu pruge
static float bezierY(float t, float p0, float p1, float p2, float p3)
{
    float u = 1.0f - t;
    return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
}

// generisanje pruge i akumulisane duzine duz pruge
static void buildTrackCurve()
{
    // visine bitnih tacaka po y-osi
    float y_start = -0.3f;
    float y_mid_valley1 = -0.3f;
    float y_peak1 = 0.2f;
    float y_mid_valley2 = -0.25f;
    float y_peak2 = 0.35f;
    float y_mid_valley3 = -0.2f;
    float y_peak3 = 0.15f;
    float y_end = -0.001f;

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
    float current_P2 = y_start + 0.05f;
    float current_P3;

    float prevX = 0.0f, prevY = 0.0f;
    totalTrackLength = 0.0f;

    for (int i = 0; i <= TRACK_SEGMENTS; ++i)
    {
        float t = i / (float)TRACK_SEGMENTS;
        float x = trackXMin + (trackXMax - trackXMin) * t;
        float y;

        // segment 0 -> peak1
        if (t <= x_segments[1]) {
            float u = t / x_segments[1];
            current_P0 = y_start;
            current_P1 = y_start + 0.1f;
            current_P2 = y_peak1 - 0.1f;
            current_P3 = y_peak1;
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // peak1 -> valley2
        else if (t <= x_segments[2]) {
            float u = (t - x_segments[1]) / (x_segments[2] - x_segments[1]);
            current_P0 = y_peak1;
            current_P1 = current_P0 + (current_P0 - (y_peak1 - 0.1f));
            current_P2 = y_mid_valley2 + 0.1f;
            current_P3 = y_mid_valley2;
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // valley2 -> peak2
        else if (t <= x_segments[3]) {
            float u = (t - x_segments[2]) / (x_segments[3] - x_segments[2]);
            current_P0 = y_mid_valley2;
            current_P1 = current_P0 + (current_P0 - (y_mid_valley2 + 0.1f));
            current_P2 = y_peak2 - 0.15f;
            current_P3 = y_peak2;
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // peak2 -> valley3
        else if (t <= x_segments[4]) {
            float u = (t - x_segments[3]) / (x_segments[4] - x_segments[3]);
            current_P0 = y_peak2;
            current_P1 = current_P0 + (current_P0 - (y_peak2 - 0.15f));
            current_P2 = y_mid_valley3 + 0.05f;
            current_P3 = y_mid_valley3;
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // valley3 -> peak3
        else if (t <= x_segments[5]) {
            float u = (t - x_segments[4]) / (x_segments[5] - x_segments[4]);
            current_P0 = y_mid_valley3;
            current_P1 = current_P0 + (current_P0 - (y_mid_valley3 + 0.05f));
            current_P2 = y_peak3 - 0.1f;
            current_P3 = y_peak3;
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }
        // peak3 -> end
        else {
            float u = (t - x_segments[5]) / (x_segments[6] - x_segments[5]);
            current_P0 = y_peak3;
            current_P1 = current_P0 + (current_P0 - (y_peak3 - 0.1f));
            current_P2 = y_end + 0.05f;
            current_P3 = y_end;
            y = bezierY(u, current_P0, current_P1, current_P2, current_P3);
        }

        // upisujemo tacku u globalni niz verteksa
        trackVertices[i * 2 + 0] = x;
        trackVertices[i * 2 + 1] = y;

        if (i == 0) {
            trackArcLengths[i] = 0.0f;
            prevX = x;
            prevY = y;
        }
        else {
            float dx = x - prevX;
            float dy = y - prevY;
            float segLen = std::sqrt(dx * dx + dy * dy);
            totalTrackLength += segLen;
            trackArcLengths[i] = totalTrackLength;
            prevX = x;
            prevY = y;
        }
    }
}

// uzorkovanje pruge prema parametru (0-1)
// vraca interpolisani (x,y) i ugao tangente (nagib pruge)
// ugao se ogranicava zbog stabilnosti
static void sampleTrack(float param, float& outX, float& outY, float& outAngle)
{
    // ogranicavanje parametra
    if (param < 0.0f) param = 0.0f;
    if (param > 1.0f) param = 1.0f;

    float fIndex = param * (TRACK_POINT_COUNT - 1);
    int i0 = (int)std::floor(fIndex);
    int i1 = i0 + 1;
    if (i1 >= TRACK_POINT_COUNT) i1 = TRACK_POINT_COUNT - 1;

    float localT = fIndex - (float)i0;

    // uzimamo dvije susjedne tacke za linearnu interpolaciju
    float x0 = trackVertices[i0 * 2 + 0];
    float y0 = trackVertices[i0 * 2 + 1];
    float x1 = trackVertices[i1 * 2 + 0];
    float y1 = trackVertices[i1 * 2 + 1];

    float dx = x1 - x0;
    float dy = y1 - y0;

    // ugao tangente pruge
    float angle = std::atan2(dy, dx);

    // ogranicenje maksimalnog nagiba da se kompozicija ne "prevrce"
    const float maxAngleDeg = 10.0f;
    const float maxAngleRad = maxAngleDeg * (float)M_PI / 180.0f;
    if (angle > maxAngleRad) angle = maxAngleRad;
    if (angle < -maxAngleRad) angle = -maxAngleRad;

    // ublazavanje efekta da ne bude previse agresivan vizuelno
    //angle *= 0.5f;

    // interpolirano X/Y na segmentu
    float x = (1.0f - localT) * x0 + localT * x1;
    float y = (1.0f - localT) * y0 + localT * y1;

    outX = x;
    outY = y;
    outAngle = angle;
}

// nalazi parametar za datu DUZINU od pocetka staze (0 je pocetak)
static float getParamAtArcLengthFromStart(float targetLen)
{
    if (targetLen <= 0.0f)            return 0.0f;
    if (targetLen >= totalTrackLength) return 1.0f;

    int i = 1;
    while (i < TRACK_POINT_COUNT && trackArcLengths[i] < targetLen)
        ++i;

    int i0 = i - 1;
    int i1 = i;
    if (i1 >= TRACK_POINT_COUNT) i1 = TRACK_POINT_COUNT - 1;

    float l0 = trackArcLengths[i0];
    float l1 = trackArcLengths[i1];
    float t = (l1 > l0) ? (targetLen - l0) / (l1 - l0) : 0.0f;

    return (i0 + t) / (float)(TRACK_POINT_COUNT - 1);
}

// vraca parametar koji je "backDistance" unazad od currentParam po REALNOJ duzini
static float getParamAtArcLengthBackwards(float currentParam, float backDistance)
{
    // trenutna duzina od pocetka
    float currentIndexF = currentParam * (TRACK_POINT_COUNT - 1);
    int   currentIndex = (int)std::floor(currentIndexF);
    if (currentIndex < 0) currentIndex = 0;
    if (currentIndex >= TRACK_POINT_COUNT) currentIndex = TRACK_POINT_COUNT - 1;

    float currentLength = trackArcLengths[currentIndex];
    float targetLen = currentLength - backDistance;
    if (targetLen < 0.0f) targetLen = 0.0f;

    return getParamAtArcLengthFromStart(targetLen);
}

// ------------------------ API ---------------------------------

void initRideMotion(float inTrackXMin,
    float inTrackXMax,
    float seatStep,
    int   inSeatCount)
{
    trackXMin = inTrackXMin;
    trackXMax = inTrackXMax;
    seatCount = inSeatCount;
    seatStepNDC = seatStep;

    // generisemo geometriju pruge i arc-length tabelu
    buildTrackCurve();

    // --------- razmak sjedista po realnoj duzini ----------
    float ndcTrackWidth = trackXMax - trackXMin;
    float seatStepNorm = seatStepNDC / ndcTrackWidth;      // u odnosu na sirinu po X
    seatStepLen = seatStepNorm * totalTrackLength;  // projekcija na realnu stazu
    trainLen = seatStepLen * (seatCount - 1);  // ukupna duzina kompozicije

    // zadnji vagon (seatIndex 0) -> pocetak pruge (duzina = 0)
    // prvi vagon (seatIndex 7)   -> na duzini trainLen od pocetka
    float frontLen = trainLen;
    trackParamStart = getParamAtArcLengthFromStart(frontLen);
    trackParamEnd = 1.0f;
    trackParam = trackParamStart;

    rideSpeed = 0.0f;
    isRideOngoing = false;
    cartAngle = 0.0f;
    state = RideState::AtStartIdle;
    emergencyRequested = false;
    justReturnedToStart = false;
    stopTimer = 0.0;
}

// stanje
bool isRideRunning()
{
    return isRideOngoing;
}

// pokusavamo pokrenuti voznju samo ako:
// - trenutno ne vozi
// - canStart je true (provjera iz Scene.cpp)
void tryStartRide(bool canStart)
{
    if (!canStart)
        return;

    if (isRideOngoing)
        return;

    if (state != RideState::AtStartIdle)
        return;

    trackParam = trackParamStart;
    rideSpeed = 0.0f;
    isRideOngoing = true;
    state = RideState::RunningForward;
    emergencyRequested = false;
    stopTimer = 0.0;
}

// glavni update
void updateRide(double deltaTime)
{
    float dt = (float)deltaTime;

    switch (state)
    {
    case RideState::AtStartIdle:
        // stoji na pocetku, nista se ne desava
        isRideOngoing = false;
        break;

    case RideState::RunningForward:
    {
        isRideOngoing = true;

        // osnovno ubrzavanje do krstarece brzine
        rideSpeed += RIDE_ACCEL * dt;
        if (rideSpeed > RIDE_MAX_SPEED)
            rideSpeed = RIDE_MAX_SPEED;

        // ako je neko trazio emergency -> odmah prelazimo u mod kocenja
        if (emergencyRequested) {
            state = RideState::EmergencyStopping;
            break;
        }

        // normalna “gravitaciona” fizika
        float xCurr, yCurr, angleCurr;
        sampleTrack(trackParam, xCurr, yCurr, angleCurr);

        float slopeFactor = std::sin(angleCurr);
        rideSpeed += RIDE_SLOPE_ACCEL * (-slopeFactor) * dt;

        if (rideSpeed < RIDE_MIN_SPEED)
            rideSpeed = RIDE_MIN_SPEED;
        if (rideSpeed > RIDE_MAX_SLOPE_SPEED)
            rideSpeed = RIDE_MAX_SLOPE_SPEED;

        trackParam += rideSpeed * dt;

        // stigli do kraja -> automatski prelazimo u povratak na pocetak
        /*if (trackParam >= trackParamEnd) {
            trackParam = trackParamEnd;
            state = RideState::ReturningToStart;
            rideSpeed = -RETURN_SPEED;
            isRideOngoing = true;
        }*/
        break;
    }

    case RideState::EmergencyStopping:
    {
        isRideOngoing = true;

        // lagano kocenje, nezavisno od nagiba
        rideSpeed -= EMERGENCY_DECEL * dt;
        if (rideSpeed < 0.0f)
            rideSpeed = 0.0f;

        trackParam += rideSpeed * dt;

        if (rideSpeed <= 0.0f) {
            state = RideState::StoppedForSick;
            isRideOngoing = false;
            emergencyRequested = false;
            stopTimer = 0.0;
        }
        break;
    }

    case RideState::StoppedForSick:
        isRideOngoing = false;
        stopTimer += deltaTime;
        if (stopTimer >= EMERGENCY_STOP_DURATION) {
            state = RideState::ReturningToStart;
            isRideOngoing = true;
            rideSpeed = -RETURN_SPEED;
        }
        break;

    case RideState::ReturningToStart:
        isRideOngoing = true;

        // idemo KONSTANTNOM brzinom nazad (bez gravitacije)
        trackParam += rideSpeed * dt;  // rideSpeed < 0

        if (trackParam <= trackParamStart) {
            trackParam = trackParamStart;
            rideSpeed = 0.0f;
            isRideOngoing = false;
            state = RideState::AtStartIdle;
            justReturnedToStart = true;   // javi sceni da smo stigli
        }
        break;
    }

    // ugao za crtanje vagona (zavisno od trenutnog parametra)
    float x, y, angle;
    sampleTrack(trackParam, x, y, angle);
    cartAngle = angle;
}

// bazna pozicija sjedista N, ukljucujuci ugao pruge u toj tacki
void getSeatBasePositionAndAngle(int seatIndex,
    float& sx,
    float& sy,
    float& angle)
{
    // seatIndex 0  -> zadnji vagon (najblize pocetku)
    // seatIndex 7  -> prvi vagon (naprijed)
    int seatOrderIndex = seatCount - 1 - seatIndex;
    // sjediste je pomjereno unazad po parametru
    float backDist = seatOrderIndex * seatStepLen;
    float seatParam = getParamAtArcLengthBackwards(trackParam, backDist);

    // uzorkujemo tacku na pruzi
    sampleTrack(seatParam, sx, sy, angle);
}

// vracamo pointer na sve tacke pruge (za OpenGL VBO/VAO)
const float* getTrackVertices()
{
    return trackVertices;
}

void requestEmergencyStop()
{
    // emergency smije samo dok normalno vozimo naprijed
    if (!isRideOngoing)
        return;
    if (state != RideState::RunningForward)
        return;

    emergencyRequested = true;
}

bool didJustReturnToStart()
{
    if (!justReturnedToStart)
        return false;
    justReturnedToStart = false;  // reset flaga
    return true;
}
