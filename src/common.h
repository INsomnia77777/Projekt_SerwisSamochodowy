#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <ctime>
#include <cerrno>
#include <iostream>
#include <vector>

#define PLIK_KLUCZA "."   // Œcie¿ka do generowania klucza
#define ID_PROJEKTU 'S'   // Znak projektu (Serwis)

// --- LIMITY I STA£E ---
#define T1 60             // Decyzja dla klienta o czekaniu kiedy serwis nieczynny
#define LICZBA_SEM 5
#define MAX_USLUG 30
#define MAX_USTER_W_AUCIE 5

const int K1 = 3;
const int K2 = 5;

const int T_POCZATEK = 0;
const int T_KONIEC = 24;
const int SERWIS_OTWARCIE = 8;
const int SERWIS_ZAMKNIECIE = 18;
const int JEDNOSTKA_CZASU_MS = 60000; // 60000 ms = 60 sekund czasu rzeczywistego, 1 min real= 1h symulacja

// RAPORT SYMULACJI
#define PLIK_RAPORTU "raport_symulacji.txt"

// INDEKSY SEMAFORÓW
enum SemIndex {
    SEM_PRACOWNICY = 0,         // Kolejka klientów (zmienna wartoœæ 1-3)
    SEM_ALARM = 1,              // Kolejka klientów - drugi semafor
    SEM_WARSZTAT_OGOLNY = 2,    // Stanowiska mech 1-7 (wartoœæ pocz. 7)
    SEM_WARSZTAT_SPECJALNY = 3, // Stanowisko mech 8 (wartoœæ pocz. 1 - tylko U i Y)
    SEM_KASA = 4                // Kolejka do kasy (wartoœæ pocz. 1)
};

// PRIORYTETY WIADOMOŒCI
enum TypKomunikatu {
    MSG_OD_KASJERA = 1,      // Najwy¿szy: od kasjera
    MSG_OD_MECHANIKA = 2,    // Wysoki: od mechanika
    MSG_NOWY_KLIENT = 3      // Standard: od klienta
};

// USTERKI
inline bool czy_krytyczna(int id_uslugi) {
    // Kody usterek krytycznych: 4, 16, 21
    return (id_uslugi == 4 || id_uslugi == 16 || id_uslugi == 21);
}

// US£UGI
struct Usluga {
    int id;
    char nazwa[64];
    int cena;
    int czas_bazowy;
};

// 1. Pamiêæ Dzielona: ZEGAR
struct StanZegara {
    int dzien;
    int godzina;
    int minuta;
    bool czy_otwarte;
    //trzeba bedzie dodac liczby otwartych stanowisk ale to po semaforach
};

// 2. Kolejka komunikatów
struct Wiadomosc {
    long mtype;
    pid_t nadawca_pid;
    int id_klienta;
    char marka_auta;
    int id_uslugi[MAX_USTER_W_AUCIE];
    int liczba_usterek;
    bool czy_zaakceptowano;
    bool czy_gotowe; //koniec naprawy
    int cena_total;
};

//Klucze (ftok) - trzeba to bêdzie zaktualizowaæ
#define SHM_KEY 0xB12345
#define MSG_KEY 0xA67890
#define SEM_KEY 0xC12345

//Funkcje pomocnicze

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

#endif
