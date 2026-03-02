#include "common.h"
#include <iostream>
#include <csignal>
#include <unistd.h>

int shmid_zegar;
StanZegara* zegar = nullptr;

void podlacz_zasoby() {
    shmid_zegar = shmget(SHM_KEY_ZEGAR, 0, 0600);
    if (shmid_zegar == -1) {
        perror("KIEROWNIK: Blad shmget Zegar (serwis wylaczony?)");
        exit(1);
    }
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);
}

int main() {
    podlacz_zasoby();
    log("KIEROWNIK", "POZAR! Oglaszam ewakulacje!");
    if (kill(zegar->pid_main, 4) == -1) {
        perror("KIEROWNIK: Blad wysylania sygnalu");
    }

    shmdt(zegar);

    return 0;
}