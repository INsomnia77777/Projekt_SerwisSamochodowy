#include "common.h"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/select.h> // select

int shmid_zegar;
StanZegara* zegar = nullptr;
bool dziala = true;

void podlacz_zasoby() {
    shmid_zegar = shmget(SHM_KEY_ZEGAR, 0, 0600);
    if (shmid_zegar == -1) {
        perror("KIEROWNIK: Blad shmget Zegar");
        exit(1);
    }
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);
}

void koniec_pracy(int sig) {
    dziala = false;
}

// SYGNA£Y

void sygnal4_pozar() {
    log("KIEROWNIK", "POZAR ! Oglaszam natychmiastowa ewakuacje serwisu!");

    if (kill(zegar->pid_main, 4) == -1) {
        perror("KIEROWNIK: Blad wysylania sygnalu 4");
    }
}

int main(int argc, char* argv[]) {
    podlacz_zasoby();

    signal(4, koniec_pracy);
    signal(SIGUSR2, SIG_IGN);
    signal(SIGTERM, koniec_pracy);


    fd_set readfds;
    struct timeval tv;
    char bufor[100];

    while (dziala) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {

            int bajty = read(STDIN_FILENO, bufor, sizeof(bufor) - 1);
            if (bajty > 0) {
                bufor[bajty] = '\0';
                int typ_alarmu = 0, id_mechanika = 0;

                if (sscanf(bufor, "%d %d", &typ_alarmu, &id_mechanika) >= 1) {

                    if (typ_alarmu == 4) {
                        sygnal4_pozar();
                        continue;
                    }

                    if (id_mechanika >= 1 && id_mechanika <= 8) {
                        pid_t pid = zegar->pidy_mechanikow[id_mechanika];

                        if (pid > 0) {
                            if (typ_alarmu == 1) {
                                std::cout << "[KIEROWNIK] Zamykam stanowisko mechanika nr " << id_mechanika << ".\n";
                                kill(pid, 1);
                            }
                            else if (typ_alarmu == 2) {
                                std::cout << "[KIEROWNIK] Przyspieszam mechanika nr " << id_mechanika << " (Sygnal 2).\n";
                                kill(pid, SIGUSR1);
                            }
                            else if (typ_alarmu == 3) {
                                std::cout << "[KIEROWNIK] Odwoluje przyspieszenie mechanika nr " << id_mechanika << " (Sygnal 3).\n";
                                kill(pid, SIGUSR2);
                            }
                            else {
                                std::cout << "[KIEROWNIK] Nieznany kod sygnalu.\n";
                            }
                        }
                        else {
                            std::cout << "[KIEROWNIK] Mechanik nr " << id_mechanika << " jeszcze nie zglosil gotowosci.\n";
                        }
                    }
                    else {
                        std::cout << "[KIEROWNIK] BLAD: Podaj poprawne ID mechanika (1-8) np. '2 5'.\n";
                    }
                }
            }
        }
    }

    shmdt(zegar);
    return 0;
}