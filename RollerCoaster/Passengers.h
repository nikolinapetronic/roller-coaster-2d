#pragma once

// podaci o 8 sjedista u vagonu
struct Seat {
    float localX;    // lokalna pozicija u odnosu na centar vagona (NDC)
    float localY;    // lokalna pozicija u odnosu na centar vagona (NDC)
    bool occupied;   // da li postoji putnik
    bool beltOn;     // da li je pojas zakopcan
    bool sick;       // da li mu je lose 
};

// broj sjedista u vagonu
constexpr int SEAT_COUNT = 8;

// globalni niz sjedista
extern Seat seats[SEAT_COUNT];

// faza iskrcavanja putnika
extern bool unloadingPhase;
// da li je u toku emergency scenario (vec prijavljen neki putnik)
extern bool emergencyInProgress;

// inicijalizacija sjedista (lokalne pozicije i pocetno stanje)
void initSeats(float seatsLeftX, float seatsY, float seatStep);

// da li su sva zauzeta sjedista vezana
bool areAllOccupiedSeatsBelted();

// logika koja se izvrsava kad se voz vrati na pocetak
// (odvezivanje svih zauzetih sjedista + pokretanje unloading faze)
void handleRideReturned();

// dodavanje novog putnika sa zadnje strane vagona (taster SPACE)
void tryAddPassengerFromBack();

// obrada "slosilo se" signala za putnika na zadatom indeksu od naprijed (tasteri 1-8)
void handleSickKeyFromFrontIndex(int keyIndex);
