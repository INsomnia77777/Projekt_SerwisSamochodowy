#include "common.h"
#include <iostream>
#include <sys/wait.h>
#include <vector>
#include <algorithm>

// Zmienne dla f. koniec
int semid = -1;
int msgid_klient = -1;
int msgid_mechanik = -1;
int msgid_kasjer = -1;
int shmid_zegar = -1;
int shmid_uslugi = -1;
std::vector<pid_t> procesy_potomne;

StanZegara* zegar = nullptr;
Usluga* cennik = nullptr;

void uruchom_program(const char* sciezka, const char* nazwa, std::string arg1 = "") {
    pid_t pid = fork();
    if (pid == 0) {
        if (arg1.empty()) {
            execl(sciezka, nazwa, NULL);
        }
        else {
            execl(sciezka, nazwa, arg1.c_str(), NULL);
        }
        perror("Blad execl");
        exit(1);
    }
    else if (pid > 0) {
        procesy_potomne.push_back(pid);
        log("SYSTEM", "Uruchomiono proces: " + std::string(nazwa) + " (PID: " + std::to_string(pid) + ")");
    }
    else {
        perror("Blad fork");
    }
}

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

    if (msgid_klient != -1) msgctl(msgid_klient, IPC_RMID, NULL);
    if (msgid_mechanik != -1) msgctl(msgid_mechanik, IPC_RMID, NULL);
    if (msgid_kasjer != -1) msgctl(msgid_kasjer, IPC_RMID, NULL);

    if (zegar != nullptr) {
        shmdt(zegar);
        zegar = nullptr;
    }
    if (shmid_zegar != -1) {
        shmctl(shmid_zegar, IPC_RMID, NULL);
    }

    if (cennik != nullptr) {
        shmdt(cennik);
        cennik = nullptr;
    }
    if (shmid_uslugi != -1) shmctl(shmid_uslugi, IPC_RMID, NULL);

    log("MAIN", "Zwolniono zasoby. Symulacja zakonczona");
    if (sig != 0) exit(1);
}

void inicjalizacja() {
    // 1. Semafor
    semid = semget(pobierz_klucz(ID_SEM), LICZBA_SEM, IPC_CREAT | 0600);
    if (semid == -1) { perror("Blad semget"); exit(1); }

    semctl(semid, SEM_LIMIT_KLIENTOW, SETVAL, MAX_KLIENTOW_W_KOLEJCE_MSG);
    if (semctl(semid, SEM_BUDZIK_2, SETVAL, 0) == -1) perror("Błąd zerowania BUDZIK_2");
    if (semctl(semid, SEM_BUDZIK_3, SETVAL, 0) == -1) perror("Błąd zerowania BUDZIK_3");

    semctl(semid, SEM_PRACOWNICY, SETVAL, 1);           // 1-3 pracownikow
    semctl(semid, SEM_KASA, SETVAL, 1);                 // 1 osoba przy kasie
    semctl(semid, SEM_WARSZTAT_OGOLNY, SETVAL, 7);      // 3 stanowiska ogólne
    semctl(semid, SEM_WARSZTAT_SPECJALNY, SETVAL, 1);   // 1 stanowisko specjalne
    semctl(semid, SEM_ALARM, SETVAL, 1);                // Brama nocna zamknięta
    semctl(semid, 5, SETVAL, 1);                        // Mutex logowania
    semctl(semid, SEM_DZWONEK, SETVAL, 0);              // Dzwonek dla pracownika
    semctl(semid, SEM_BUDZIK_2, SETVAL, 0);             // Pracownik 2 - stanowisko
    semctl(semid, SEM_BUDZIK_3, SETVAL, 0);             // Pracownik 3 - stanowisko

    // 2. Kolejki
    msgid_klient = msgget(MSG_KEY_KLIENT, IPC_CREAT | 0600);
    if (msgid_klient == -1) { perror("Blad msgget klient"); exit(1); }

    msgid_mechanik = msgget(MSG_KEY_MECHANIK, IPC_CREAT | 0600);
    if (msgid_mechanik == -1) { perror("Blad msgget mechanik"); exit(1); }

    msgid_kasjer = msgget(MSG_KEY_KASJER, IPC_CREAT | 0600);
    if (msgid_kasjer == -1) { perror("Blad msgget kasjer"); exit(1); }

    // 3. Pamięć - Zegar
    shmid_zegar = shmget(pobierz_klucz(ID_ZEGAR), sizeof(StanZegara), IPC_CREAT | 0600);
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);
    zegar->dzien = 1;
    zegar->godzina = 6;
    zegar->minuta = 0;
    zegar->czy_otwarte = false;
    zegar->otwarte_stanowiska = 1;
    zegar->liczba_klientow = 0;

    // 4. Pamięć - Usługi (Wczytanie z pliku)
    shmid_uslugi = shmget(pobierz_klucz(ID_USLUGI), sizeof(Usluga) * MAX_USLUG, IPC_CREAT | 0600);
    cennik = (Usluga*)shmat(shmid_uslugi, NULL, 0);
    wczytaj_uslugi(cennik);

    log("ZEGAR", "Zasoby zainicjalizowane.");
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
    inicjalizacja();

    log("MAIN", "--- SYMULACJA SERWISU SAMOCHODOWEGO ---");

    // Kasjer i 3 pracownikow
    uruchom_program("./kasjer", "kasjer", "1");
    for (int i = 1; i <= 3; i++) {
        uruchom_program("./pracownik", "pracownik", std::to_string(i));
    }

    // Mechanicy (7 zwykłych, 1 specjalista nr 8)
    for (int i = 1; i <= 7; i++) {
        uruchom_program("./mechanik", "mechanik", std::to_string(i));
    }
    uruchom_program("./mechanik", "mechanik", "8");

    log("MAIN", "Poprawnie utworzono zasoby potrzbne do przeprowadzenia symulacji.");


    //WLASCIWA PETLA SYMULACJI
    const int PREDKOSC_SYMULACJI = 100000;
    while (1) {
        zegar->minuta++;

        int aktualni = (int)procesy_potomne.size() - LICZBA_PERSONELU; //
        zegar->liczba_klientow = (aktualni < 0) ? 0 : aktualni;

        if (zegar->czy_otwarte) {
            // Otwieranie stanowisk
            if (zegar->otwarte_stanowiska == 1 && zegar->liczba_klientow > K1) {
                V(semid, SEM_PRACOWNICY);
                V(semid, SEM_BUDZIK_2);
                zegar->otwarte_stanowiska = 2;
                log("STANOWISKA", "Kolejka: " + std::to_string(zegar->liczba_klientow) + " os. -> Otwieram 2. stanowisko obslugi!");
            }
            else if (zegar->otwarte_stanowiska == 2 && zegar->liczba_klientow > K2) {
                V(semid, SEM_PRACOWNICY);
                V(semid, SEM_BUDZIK_3);
                zegar->otwarte_stanowiska = 3;
                log("STANOWISKA", "Kolejka: " + std::to_string(zegar->liczba_klientow) + " os. -> Otwieram 3. stanowisko obslugi!");
            }
        }

            // Zamykanie stanowisk
            if (zegar->otwarte_stanowiska == 3 && (zegar->liczba_klientow <= 3 || !zegar->czy_otwarte)) {
                struct sembuf op[1]; op[0].sem_num = SEM_PRACOWNICY; op[0].sem_op = -1; op[0].sem_flg = IPC_NOWAIT;
                if (semop(semid, op, 1) != -1) {
                    struct sembuf op_budzik[1]; op_budzik[0].sem_num = SEM_BUDZIK_3; op_budzik[0].sem_op = -1; op_budzik[0].sem_flg = IPC_NOWAIT;
                    semop(semid, op_budzik, 1);

                    zegar->otwarte_stanowiska = 2;
                    log("STANOWISKA", "Zamykam 3. stanowisko (Kolejka: " + std::to_string(zegar->liczba_klientow) + " os.");
                }
            }
            else if (zegar->otwarte_stanowiska == 2 && (zegar->liczba_klientow <= 2 || !zegar->czy_otwarte)) {
                struct sembuf op[1]; op[0].sem_num = SEM_PRACOWNICY; op[0].sem_op = -1; op[0].sem_flg = IPC_NOWAIT;
                if (semop(semid, op, 1) != -1) {
                    struct sembuf op_budzik[1]; op_budzik[0].sem_num = SEM_BUDZIK_2; op_budzik[0].sem_op = -1; op_budzik[0].sem_flg = IPC_NOWAIT;
                    semop(semid, op_budzik, 1);

                    zegar->otwarte_stanowiska = 1;
                    log("STANOWISKA", "Zamykam 2. stanowisko (Kolejka: " + std::to_string(zegar->liczba_klientow) + " os.");
                }
            }

        if (zegar->minuta >= 60) {
            zegar->minuta = 0;
            zegar->godzina++;
        }

        if (zegar->godzina >= 24) {
            zegar->godzina = 0;
            zegar->dzien++;
        }

        if (zegar->minuta == 0) {

            const int LICZBA_PERSONELU = 12;

            long wszystkie_procesy = procesy_potomne.size();
            long liczba_klientow = wszystkie_procesy - LICZBA_PERSONELU;

            if (liczba_klientow < 0) liczba_klientow = 0;

            std::cout << "\n----------------------------------------" << std::endl;
            char bufor[100];
            sprintf(bufor, "[RAPORT GODZINOWY %02d:00]", zegar->godzina);
            std::cout << bufor << std::endl;
            std::cout << " > Wszystkie procesy: " << wszystkie_procesy << std::endl;
            std::cout << " > Personel (staly):  " << LICZBA_PERSONELU << std::endl;
            std::cout << " > AKTYWNI KLIENCI:   " << zegar->liczba_klientow << std::endl;
            std::cout << " > Otwarte stanowiska:   " << zegar->otwarte_stanowiska << std::endl;
            std::cout << "----------------------------------------\n" << std::endl;
        }

        if (zegar->godzina == SERWIS_OTWARCIE && zegar->minuta == 0) {
            if (!zegar->czy_otwarte) {
                zegar->czy_otwarte = true;
                std::cout << "\n";
                log("ZEGAR", ">> OTWIERAMY SERWIS! <<");

                semctl(semid, SEM_ALARM, SETVAL, 0);
                int czekalo = semctl(semid, SEM_ALARM, GETZCNT, 0);
                if (czekalo > 0) {
                    log("ZEGAR", "Obudzono " + std::to_string(czekalo) + " procesow.");
                }
            }
        }
        else if (zegar->godzina == SERWIS_ZAMKNIECIE && zegar->minuta == 0) {
            if (zegar->czy_otwarte) {
                zegar->czy_otwarte = false;
                std::cout << "\n";
                log("ZEGAR", ">> ZAMYKAMY SERWIS! (Tylko dokonczenie napraw) <<");

                semctl(semid, SEM_ALARM, SETVAL, 1);
            }
        }

        if (zegar->czy_otwarte) {
            if (procesy_potomne.size() < MAX_KLIENTOW) {
                // 40% szans co 10 minut symulowanych - w dzien
                if ((rand() % 100) < 10) {
                    uruchom_program("./klient", "klient");
                }
            }
            else {
                //log("SYSTEM", "Maksymalna liczba klientow osiagnieta. Czekam na zwolnienie miejsc.");
            }
        }
        else {
            if (procesy_potomne.size() < MAX_KLIENTOW) {
                //  10% szans - w nocy
                if ((rand() % 100) < 2) {
                    uruchom_program("./klient", "klient");
                }
            }
            else {
                //log("SYSTEM", "Maksymalna liczba klientow osiagnieta. Czekam na zwolnienie miejsc.");
            }
        }

        int status;
        pid_t zakonczony_pid;

        while ((zakonczony_pid = waitpid(-1, &status, WNOHANG)) > 0) {

            auto it = std::remove(procesy_potomne.begin(), procesy_potomne.end(), zakonczony_pid);
            if (it != procesy_potomne.end()) {
                procesy_potomne.erase(it, procesy_potomne.end());
                log("SYSTEM", "Klient (PID: " + std::to_string(zakonczony_pid) + ") zakonczyl symulacje.");
            }
        }

        usleep(PREDKOSC_SYMULACJI);
    }

    return 0;
}