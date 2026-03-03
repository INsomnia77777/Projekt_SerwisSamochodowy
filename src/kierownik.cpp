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
        std::cerr << "  1 - Zamknij stanowisko (Uzycie: ./kierownik 1 <PID_MECHANIKA>)\n";
        std::cerr << "  2 - Przyspiesz mechanika o 50% (Uzycie: ./kierownik 2 <PID_MECHANIKA>)\n";
        std::cerr << "  3 - Przywroc normalne tempo (Uzycie: ./kierownik 3 <PID_MECHANIKA>)\n";
        std::cerr << "  4 - Pozar (Natychmiastowa ewakuacja)\n";
        return 1;
    }
    int typ_alarmu = std::stoi(argv[1]);
    podlacz_zasoby();

    switch (typ_alarmu) {
    case 1:
        if (argc < 3) {
            std::cerr << "[BLAD] Zapomniales podac PID mechanika!\n";
            std::cerr << "Przyklad: ./kierownik 1 12345\n";
        }
        else {
            pid_t pid_mechanika = std::stoi(argv[2]);
            std::cout << "[KIEROWNIK] Wysylam polecenie zamkniecia stanowiska do mechanika (PID: " << pid_mechanika << ").\n";

            if (kill(pid_mechanika, 1) == -1) {
                perror("KIEROWNIK: Blad wysylania sygnalu 1 do mechanika");
            }
        }
        break;

    case 2:
        if (argc < 3) {
            std::cerr << "[BLAD] Zapomniales podac PID mechanika!\n";
            std::cerr << "Przyklad: ./kierownik 2 12345\n";
        }
        else {
            pid_t pid_mechanika = std::stoi(argv[2]);
            std::cout << "[KIEROWNIK] Wysylam polecenie przyspieszenia stanowiska do mechanika (PID: " << pid_mechanika << ").\n";

            if (kill(pid_mechanika, SIGUSR1) == -1) {
                perror("KIEROWNIK: Blad wysylania sygnalu 2 do mechanika");
            }
        }
        break;

    case 3:
        if (argc < 3) {
            std::cerr << "[BLAD] Zapomniales podac PID mechanika!\n";
            std::cerr << "Przyklad: ./kierownik 3 12345\n";
        }
        else {
            pid_t pid_mechanika = std::stoi(argv[2]);
            std::cout << "[KIEROWNIK] Wysylam polecenie przywrocenia normalnego tempa do mechanika (PID: " << pid_mechanika << ").\n";

            if (kill(pid_mechanika, SIGUSR2) == -1) {
                perror("KIEROWNIK: Blad wysylania sygnalu 3 do mechanika");
            }
        }
        break;

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