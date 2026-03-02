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

// SYGNA£Y

void sygnal4_pozar() {
    log("KIEROWNIK", "POZAR ! Oglaszam natychmiastowa ewakuacje serwisu!");

    if (kill(zegar->pid_main, 4) == -1) {
        perror("KIEROWNIK: Blad wysylania sygnalu 4");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uzycie: " << argv[0] << " <numer_sygnalu>\n";
        std::cerr << "Dostepne komendy:\n";
        std::cerr << "  4 - Pozar (Natychmiastowa ewakuacja)\n";
        return 1;
    }
    int typ_alarmu = std::stoi(argv[1]);
    podlacz_zasoby();
    
    switch (typ_alarmu) {
    case 4:
        sygnal4_pozar();
        break;

    default:
        std::cout << "[KIEROWNIK] Nieznany kod sygnalu: " << typ_alarmu << ".\n";
        break;
    }

    shmdt(zegar);

    return 0;
}