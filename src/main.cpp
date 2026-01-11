//Klient - generator kierowców z markami samochodów A-Z

//Serwis - 8 stanowisk (1-7:A, E, I, O, U, Y; 8: U, Y); A, E, I, O, U, Y; cennik napraw (co najmniej 30 usług)
//Obsługa klienta - określa przybliżony czas naprawy oraz przewidywany koszt naprawy, 3 stanowiska (działa min. 1)
//Mechanik -  naprawia samochody, przekazuje do pracownika serwisu formularz z zakresem wykonanych napraw
// Kasjer - przyjmuje płatności od klientów
//Kierownik serwisu - sygnał1 (mechanik): zamyka dow stanowisko napraw, 
//                  - sygnał2 (mechanik): przyspieszyc czas naprawy o 50%
//                  - sygnał3 (mechanik): przywraca czas naprawy do stanu pierwotnego
//                  - sygnał4 (pozar): mechanicy przerywają pracę, wszytscy opuszczają serwis

#include "common.h"
#include <iostream>
#include <sys/wait.h>
#include <csignal>

volatile sig_atomic_t czy_dzialac = 1;

void obsluga_ctrl_c(int sig) {
    loguj("SYSTEM", "Ctrl+C. Konczenie symulacji...");
    czy_dzialac = 0;
}


int main()
{
    std::cout << "Witamy w symulacji serwisu samochodowego\n";

    signal(SIGINT, obsluga_ctrl_c);

    int shmid = shmget(PROJECT_KEY, sizeof(StanSerwisu), IPC_CREAT | 0600);
    blad(shmid, "shmget");

    StanSerwisu* stan = (StanSerwisu*)shmat(shmid, NULL, 0);
    if (stan == (void*)-1) blad(-1, "shmat");

    stan->czy_pozar = false;
    stan->liczba_klientow_w_kolejce = 0;
    for (int i = 0; i < MAX_MECHANIKOW; i++) stan->stanowisko_mechanika_zajete[i] = false;
    
    //SEMAFOR - kolejka klientow
    int semid = semget(PROJECT_KEY, 1, IPC_CREAT | 0600);
    if (semid == -1) { perror("Zarzadca: semget"); exit(1); }

    // Ustawienie wartości semafora nr 0 na 3 (3 wolne stanowiska)
    //
    if (semctl(semid, 0, SETVAL, 0) == -1) {
        perror("Zarzadca: semctl SETVAL");
        return 1; //tu ma być exit czy return 1
    }
    loguj("SYSTEM", "Zainicjowano semafor System V na 3 stanowiska");

    //WCZYTANIE USLUG ITD

    wczytaj_uslugi(stan->cennik);

    int msgid = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0600);
    blad(msgid, "msgget");

    std::ofstream czyszczenie(PLIK_RAPORTU, std::ios::trunc);
    czyszczenie.close();
    loguj("ZARZADCA", "Rozpoczecie symulacji serwisu samochodwego");

    std::cout << "Zeby zakonczyc symulacje wcisnic: Ctrl+C" << std::endl;

    //TWORZENIE PRACOWNIKÓW
    pid_t pracownicy[LICZBA_PRACOWNIKOW];

    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            execl("./pracownik", "pracownik", NULL);
            perror("Blad execl pracownik");
            exit(1);
        }
        pracownicy[i] = pid;
        kill(pid, SIGSTOP);
        loguj("SYSTEM", "Utworzono pracownika (PID: " + std::to_string(pracownik_pid) + ") i uśpiono go.");
    }



    

    int dzien = 1;

    while (czy_dzialac) {

        loguj("KALENDARZ", "Dzien: " + std::to_string(dzien));

        for (int godzina = T_POCZATEK; godzina <= T_KONIEC; godzina++) {
            if (!czy_dzialac) break;

            if (godzina == SERWIS_OTWARCIE) {
                kill(pracownicy[0], SIGCONT);
                stan->otwarte_okienka_obslugi = 1;
                V(semid, 0);
                loguj("ZARZADCA", "Otwarto serwis");
            }

            if (godzina == SERWIS_ZAMKNIECIE) {
                kill(pracownicy[0], SIGSTOP);
                loguj("ZARZADCA", "Zamknieto serwis");
            }

            for (int minuta = 0; minuta < 60; minuta++) {
                if (!czy_dzialac) break;

                stan->aktualna_godzina = godzina;
                stan->aktualna_minuta = minuta;

                //SPAWN KLIENTOW - 3% na min
                if (godzina >= SERWIS_OTWARCIE && godzina < SERWIS_ZAMKNIECIE) {
                    int kolejka_klientow = stan->liczba_klientow_w_kolejce;


                    //OTWIERANIE I ZAMYKANIE OKIENEK PRACOWNIKOW
                    if (kolejka_klientow > K1 && stan->otwarte_okienka_obslugi<2) {
                        kill(pracownicy[1], SIGCONT);
                        stan->otwarte_okienka_obslugi = 2;
                        V(semid, 0);
                    }

                    if (kolejka_klientow > K2 && stan->otwarte_okienka_obslugi < 3) {
                        kill(pracownicy[2], SIGCONT);
                        stan->otwarte_okienka_obslugi = 3;
                        V(semid, 0);
                    }

                    //Trzeba jakoś zrobić zamykanie
                    
                    
                    if ((rand() % 100) < 3) {
                        pid_t pid = fork();
                        if (pid == 0) {
                            execl("./klient", "klient", NULL);
                            exit(1);
                        }
                    }
                }
                // 5. Wyświetlanie zegara na żywo w terminalu (format HH:MM)
                 // \r powoduje nadpisywanie linii, %02d dodaje zero wiodące (np. 08:05)
                printf("\r[Dzien %d | Czas: %02d:%02d] (Ctrl+C = Stop) ", dzien, godzina, minuta);
                fflush(stdout); // Wymuszenie wypisania na ekran

                usleep(JEDNOSTKA_CZASU_MS * 1000);
            }
        }
        if (dzien == 3) break;
        dzien++;
    }

    loguj("ZARZADCA", "Zamykam procesy");

    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        kill(pracownicy[i], SIGCONT);
        kill(pracownicy[i], SIGTERM);
    }

    //ZAKOŃCZENIE SYMULACJI
    while (wait(NULL) > 0);

    semctl(semid, 0, IPC_RMID);
    shmdt(stan);
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msgid, IPC_RMID, NULL);

    loguj("ZARZADCA", "Koniec symulacji serwisu samochodowego");

    return 0;
}
