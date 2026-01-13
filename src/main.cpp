#include "common.h"
#include <iostream>
#include <sys/wait.h>
#include <vector>

// Zmienne dla f. koniec
int semid = -1;
int msgid = -1;
int shmid_zegar = -1;
int shmid_uslugi = -1;
std::vector<pid_t> procesy_potomne;

StanZegara* zegar = nullptr;
Usluga* cennik = nullptr;

// Ctrl + C
void koniec(int sig) {
    if (sig != 0) log("MAIN", "Otrzymano sygnal przerwania. Zwalniam zasoby...");
        for (pid_t pid : procesy_potomne) {
            kill(pid, SIGTERM);
        }
    while (wait(NULL) > 0);
        if (semid != -1) {
            semctl(semid, 0, IPC_RMID);
        }
    if (msgid != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }
    if (zegar != nullptr) shmdt(zegar);
    if (shmid_zegar != -1) shmctl(shmid_zegar, IPC_RMID, NULL);
    if (cennik != nullptr) shmdt(cennik);
    if (shmid_uslugi != -1) shmctl(shmid_uslugi, IPC_RMID, NULL);

    log("MAIN", "Zwolniono zasoby. Symulacja zakonczona");
    if (sig != 0) exit(1);
}

void blad_krytyczny(int wynik, const char* komunikat) {
    if (wynik == -1) {
        perror(komunikat);
        koniec(0);
        exit(1);
    }
}

int main() {

    //WCZYTYWANIE ZASOBOW

    signal(SIGINT, koniec);
    srand(time(NULL));

    log("MAIN", "--- SYMULACJA SERWISU SAMOCHODOWEGO ---");

    semid = semget(SEM_KEY, LICZBA_SEM, IPC_CREAT | 0600);
    blad_krytyczny(semid, "Blad semget");

    if (semctl(semid, SEM_PRACOWNICY, SETVAL, 1) == -1) blad_krytyczny(-1, "Init SEM_PRACOWNICY");
    semctl(semid, SEM_ALARM, SETVAL, 0);
    semctl(semid, SEM_WARSZTAT_OGOLNY, SETVAL, 7);
    semctl(semid, SEM_WARSZTAT_SPECJALNY, SETVAL, 1);
    semctl(semid, SEM_KASA, SETVAL, 1);

    msgid = msgget(MSG_KEY, IPC_CREAT | 0600);
    blad_krytyczny(msgid, "Blad msgget");
    msgctl(msgid, IPC_RMID, NULL);
    msgid = msgget(MSG_KEY, IPC_CREAT | 0600);

    shmid_zegar = shmget(SHM_KEY, sizeof(StanZegara), IPC_CREAT | 0600);
    blad_krytyczny(shmid_zegar, "Blad shmget Zegar");
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);
    if (zegar == (void*)-1) blad_krytyczny(-1, "Blad shmat Zegar");

    zegar->dzien = 1;
    zegar->godzina = SERWIS_OTWARCIE;
    zegar->minuta = 0;
    zegar->czy_otwarte = true;
   
    shmid_uslugi = shmget(SHM_KEY + 1, sizeof(Usluga) * MAX_USLUG, IPC_CREAT | 0600);
    blad_krytyczny(shmid_uslugi, "Blad shmget Uslugi");

    cennik = (Usluga*)shmat(shmid_uslugi, NULL, 0);
    if (cennik == (void*)-1) blad_krytyczny(-1, "Blad shmat Uslugi");

    wczytaj_uslugi(cennik);

    log("MAIN", "Poprawnie utworzono zasoby potrzbne do przeprowadzenia symulacji.");


    //WLASCIWA PETLA SYMULACJI
    const int PREDKOSC_SYMULACJI = 100000;
    while (1) {
        zegar->minuta++;

        if (zegar->minuta >= 60) {
            zegar->minuta = 0;
            zegar->godzina++;
        }

        if (zegar->godzina >= 24) {
            zegar->godzina = 0;
            zegar->dzien++;
        }

        if (zegar->godzina == SERWIS_OTWARCIE && zegar->minuta == 0) {
            if (!zegar->czy_otwarte) {
                zegar->czy_otwarte = true;
                std::cout << "\n";
                log("ZEGAR", ">> OTWIERAMY SERWIS! <<");
                // Wywolanie pracownikow w przyszlosci
            }
        }
        else if (zegar->godzina == SERWIS_ZAMKNIECIE && zegar->minuta == 0) {
            if (zegar->czy_otwarte) {
                zegar->czy_otwarte = false;
                std::cout << "\n";
                log("ZEGAR", ">> ZAMYKAMY SERWIS! (Tylko dokonczenie napraw) <<");
                // Wszytkie procesy ida spac...w przyszlosci
            }
        }

        printf("\r[DZIEN: %d] Godzina: %02d:%02d | Status: %s | (Ctrl+C aby zakonczyc)",
            zegar->dzien,
            zegar->godzina,
            zegar->minuta,
            zegar->czy_otwarte ? "OTWARTE " : "ZAMKNIETE");

        fflush(stdout);

        usleep(PREDKOSC_SYMULACJI);
    }

    return 0;



}