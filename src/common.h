#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

// Klucze IPC - musz¹ byæ identyczne dla wszystkich procesów
#define PROJECT_KEY 0x12345
#define MSG_QUEUE_KEY 0x67890

// Sta³e serwisu
const int MAX_MECHANIKOW = 8;
const int MAX_USLUG = 30;

// Struktura us³ugi (Twój cennik 30 us³ug)
struct Usluga {
    int id;
    char nazwa[64];
    int cena;
    int czas_bazowy;
};

inline void wczytaj_uslugi(Usluga* tablica) {
    std::ifstream plik("uslugi.txt");
    if (!plik.is_open()) {
        std::cerr << "Blad otwarcia pliku uslugi.txt" << std::endl;
        return;
    }

    std::string linia;
    int i = 0;
    while (std::getline(plik, linia) && i < MAX_USLUG) {
        std::stringstream ss(linia);
        std::string segment;

        std::getline(ss, segment, '\t'); tablica[i].id = std::stoi(segment);
        std::getline(ss, segment, '\t'); strncpy(tablica[i].nazwa, segment.c_str(), 63);
        std::getline(ss, segment, '\t'); tablica[i].cena = std::stoi(segment);
        std::getline(ss, segment, '\t'); tablica[i].czas_bazowy = std::stoi(segment);

        i++;
    }
    plik.close();
    std::cout << "Wczytano " << i << " uslug do pamieci dzielonej." << std::endl;
}

// STRUKTURA PAMIÊCI DZIELONEJ - stan ca³ego serwisu
struct StanSerwisu {
    Usluga cennik[MAX_USLUG];
    int liczba_klientow_w_kolejce;       // Do dynamicznego otwierania okienek
    int otwarte_okienka_obslugi;         // K1 i K2
    bool stanowisko_mechanika_zajete[MAX_MECHANIKOW];
    bool czy_pozar;                     // Sygna³ 4
    int aktualna_godzina;               // Czas od Tp do Tk
};

// STRUKTURA KOLEJKI KOMUNIKATÓW - komunikacja miêdzy procesami
struct Wiadomosc {
    long mtype;             // 1: Klient->Pracownik, 2: Pracownik->Mechanik, itd.
    pid_t nadawca_pid;      // PID procesu wysy³aj¹cego
    char marka_auta;        // A, E, I, O, U, Y lub inne
    int id_uslugi;          // Wybrana us³uga z cennika
    bool czy_zaakceptowano; // Do obs³ugi 2% odrzuceñ i 20% dodatkowych usterek
};

inline void blad(int wynik, const char* komunikat) {
    if (wynik == -1) {
        perror(komunikat);
    }
}

#endif
