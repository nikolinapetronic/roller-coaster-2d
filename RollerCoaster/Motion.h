#pragma once

// sve sto je vezano za kretanje i prugu

// broj segmenata i tacaka pruge (za VAO i crtanje)
extern const int RC_TRACK_SEGMENTS;
extern const int RC_TRACK_POINT_COUNT;

// inicijalizacija logike kretanja i generisanje pruge
// - trackXMin / trackXMax : opseg po x-osi 
// - seatStep: razmak izmedju sjedista u NDC 
// - seatCount: broj sjedista 
void RC_InitMotion(float trackXMin,
    float trackXMax,
    float seatStep,
    int   seatCount);

// da li se voznja trenutno odvija
bool RC_IsRideRunning();

// pokusaj pokretanja voznje
// ako canStart == false ili voznja vec traje – nece se pokrenuti
void RC_TryStartRide(bool canStart);

// update logike kretanja 
void RC_Update(double deltaTime);

// bazna pozicija i ugao pruge za sjediste sa zadatim indeksom (0..seatCount-1)
void RC_GetSeatBasePosAndAngle(int seatIndex,
    float& sx,
    float& sy,
    float& angle);

// pointer na generisane verteks koordinate pruge 
const float* RC_GetTrackVertices();