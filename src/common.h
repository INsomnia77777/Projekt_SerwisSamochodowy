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
#include <semaphore.h>

//Klucze (ftok) - trzeba to bêdzie zaktualizowaæ
#define PROJECT_KEY 0xB12345
#define MSG_QUEUE_KEY 0xA67890

// Sta³e serwisu
const int MAX_MECHANIKOW = 8;
const int MAX_USLUG = 30;
const int K1 = 3;
const int K2 = 5;

// Sta³e czas
const int T_POCZATEK = 0;
const int T_KONIEC = 24;
const int SERWIS_OTWARCIE = 10;
const int SERWIS_ZAMKNIECIE = 18;
const int JEDNOSTKA_CZASU_MS = 60000; // 60000 ms = 60 sekund czasu rzeczywistego, 1 min real= 1h symulacja

// RAPORT SYMULACJI
const std::string PLIK_RAPORTU = "raport_symulacji.txt";

// Struktura us³ugi
//Trzeba okreœliæ jeszcze 3 kryrtyczne usterki
struct Usluga {
    int id;
    char nazwa[64];
    int cena;
    int czas_bazowy;
};

// STRUKTURA PAMIÊCI DZIELONEJ
struct StanSerwisu {
    Usluga cennik[MAX_USLUG];
    sem_t sem_dostepne_stanowiska_obslugi;
    int liczba_klientow_w_kolejce;
    int otwarte_okienka_obslugi;
    bool stanowisko_mechanika_zajete[MAX_MECHANIKOW];
    bool czy_pozar;                     // Sygna³ 4
    int aktualna_godzina;
    int aktualna_minuta;
};

// STRUKTURA KOLEJKI KOMUNIKATÓW
struct Wiadomosc {
    long mtype;             // 1: Klient->Pracownik, 2: Pracownik->Mechanik, itd.
    pid_t nadawca_pid;      // PID procesu wysy³aj¹cego
    char marka_auta;        // A, E, I, O, U, Y lub inne
    int id_uslugi;          // Wybrana us³uga z cennika
    bool czy_zaakceptowano; // Do obs³ugi 2% odrzuceñ i 20% dodatkowych usterek
    int cena;
    int czas;
};

//SEMAFORY
//OPUSZCZENIE
inline void P(int semid, int sem_num) {
    struct sembuf operacje[1];
    operacje[0].sem_num = sem_num;
    operacje[0].sem_op = -1;
    operacje[0].sem_flg = 0;

    while (semop(semid, operacje, 1) == -1) {
        if (errno == EINTR) continue;
        perror("Blad operacji P (semop)");
        exit(1);
    }
}

//PODNIESIENIE
inline void V(int semid, int sem_num) {
    struct sembuf operacje[1];
    operacje[0].sem_num = sem_num;
    operacje[0].sem_op = 1;
    operacje[0].sem_flg = 0;

    if (semop(semid, operacje, 1) == -1) {
        perror("Blad operacji V (semop)");
        exit(1);
    }
}

inline void loguj(std::string nadawca, std::string komunikat) {

    std::cout << "[" << nadawca << "] " << komunikat << std::endl;

    std::ofstream plik(PLIK_RAPORTU, std::ios::app);
    if (plik.is_open()) {
        plik << "[" << nadawca << "] " << komunikat << std::endl;
        plik.close();
    }
    else {
        perror("Blad zapisu do raportu");
    }
}

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

inline void blad(int wynik, const char* komunikat) {
    if (wynik == -1) {
        perror(komunikat);
    }
}

#endif
