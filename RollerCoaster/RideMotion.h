#pragma once

// sve sto je vezano za kretanje i prugu

// broj segmenata i tacaka pruge (za VAO i crtanje)
constexpr int TRACK_SEGMENTS = 5000;
constexpr int TRACK_POINT_COUNT = TRACK_SEGMENTS + 1;

// inicijalizacija logike kretanja i generisanje pruge
// - trackXMin / trackXMax : opseg po x-osi 
// - seatStep: razmak izmedju sjedista u NDC 
// - seatCount: broj sjedista 
void initRideMotion(float trackXMin,
    float trackXMax,
    float seatStep,
    int   seatCount);

// stanje - da li kompozicija trenutno vozi
bool isRideRunning();

// pokusavamo pokrenuti voznju samo ako:
// - trenutno ne vozi
// - canStart je true (provjera iz Scene.cpp)
void tryStartRide(bool canStart);

// update logike kretanja 
void updateRide(double deltaTime);

// bazna pozicija i ugao pruge za sjediste sa zadatim indeksom (0..seatCount-1)
void getSeatBasePositionAndAngle(int seatIndex,
    float& sx,
    float& sy,
    float& angle);

// pointer na generisane verteks koordinate pruge 
const float* getTrackVertices();

// zahtjev za vanrednim zaustavljanjem
void requestEmergencyStop();

// da li smo se upravo vratili na pocetak (flag se resetuje nakon poziva)
bool didJustReturnToStart();
