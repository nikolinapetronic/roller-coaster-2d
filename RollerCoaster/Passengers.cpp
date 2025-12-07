#include "Passengers.h"
#include "RideMotion.h"

// globalni niz sjedista
Seat seats[SEAT_COUNT];

// faza iskrcavanja putnika
bool unloadingPhase = false;
// da li je u toku emergency scenario (vec prijavljen neki putnik)
bool emergencyInProgress = false;

// inicijalizacija sjedista (lokalne pozicije i pocetno stanje)
void initSeats(float seatsLeftX, float seatsY, float seatStep)
{
    // visina sjedista unutar vagona (po y-osi) se vec racuna u Scene.cpp
    // ovdje samo podesavamo lokalne X/Y i pocetno stanje

    for (int i = 0; i < SEAT_COUNT; ++i) {
        seats[i].occupied = false;
        seats[i].beltOn = false;
        seats[i].sick = false;

        // centri: 0.5, 1.5, 2.5, ... , 7.5
        seats[i].localX = seatsLeftX + seatStep * (0.5f + i);
        seats[i].localY = seatsY;
    }
}

// da li su sva zauzeta sjedista vezana
bool areAllOccupiedSeatsBelted()
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

// logika koja se izvrsava kad se voz vrati na pocetak
// (odvezivanje svih zauzetih sjedista + pokretanje unloading faze)
void handleRideReturned()
{
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

// dodavanje novog putnika sa zadnje strane vagona (taster SPACE)
void tryAddPassengerFromBack()
{
    // ne dodaj putnike ako voz vozi ili jos iskrcavamo staru turu
    if (isRideRunning() || unloadingPhase)
        return;

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
}

// obrada "slosilo se" signala za putnika na zadatom indeksu od naprijed (tasteri 1-8)
void handleSickKeyFromFrontIndex(int keyIndex)
{
    // ako voz ne vozi ili smo vec u emergency scenariju -> ignorisi signal
    if (!isRideRunning() || emergencyInProgress)
        return;

    // 0 = prednje sjediste (seat[SEAT_COUNT-1])
    // SEAT_COUNT-1 = zadnje sjediste (seat[0])
    int seatNumber = SEAT_COUNT - 1 - keyIndex;

    // ako to sjediste nije zauzeto -> nista
    if (seatNumber < 0 || seatNumber >= SEAT_COUNT)
        return;
    if (!seats[seatNumber].occupied)
        return;

    // oznaci ga kao "zelenog" i pokreni emergency stop
    seats[seatNumber].sick = true;
    requestEmergencyStop();

    // ne primamo nove sick signale dok se scena ne resetuje
    emergencyInProgress = true;
}
