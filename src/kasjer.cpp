#include "common.h"
#include <iostream>
#include <unistd.h>
#include <string>

// ZMIENNE GLOBALNE
int semid;
int msgid;
int shmid_zegar;
std::string identyfikator;

StanZegara* zegar;

// FUNKCJE POMOCNICZE

void podlacz_zasoby() {
    semid = semget(SEM_KEY, 0, 0600);
    if (semid == -1) { perror("KLIENT: Blad semget"); exit(1); }

    msgid = msgget(MSG_KEY_KASJER, 0600);
    if (msgid == -1) { perror("KLIENT: Blad msgget"); exit(1); }

    shmid_zegar = shmget(SHM_KEY_ZEGAR, 0, 0600);
    if (shmid_zegar == -1) { perror("KLIENT: Blad shmget Zegar"); exit(1); }
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);
}

int main() {
    srand(getpid());
    identyfikator = "KASJER " + std::to_string(getpid());
    podlacz_zasoby();

    log(identyfikator, "Otwieram kase. Czekam na klientow.");

    Wiadomosc msg;

    while (true) {
        ssize_t wynik = msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), MSG_PLATNOSC, 0);

        if (wynik == -1) {
            if (errno == EINTR) continue;
            perror("KASJER: Blad msgrcv");
            break;
        }

        //komunikaty od niego siê nie wyœwietlaj¹
        log(identyfikator, "Przyjmuje wplate " + std::to_string(msg.cena_total) +
            " od Klienta " + std::to_string(msg.id_klienta));

        // Symulacja pracy - konwersacja przy kasie
        usleep(200000); // 0.2 sekundy

        msg.mtype = MSG_OD_KASJERA;

        if (msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0) == -1) {
            perror("KASJER: Blad msgsnd");
        }
        else {
            V(semid, SEM_DZWONEK);
            log(identyfikator, "Zaksiegowano wplate. Wyslalem info do Pracownika.");
        }
    }

    return 0;
}