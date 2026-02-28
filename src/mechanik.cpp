#include "common.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

// ZMIENNE GLOBALNE
int semid;
int msgid;
int shmid_zegar;
int shmid_uslugi;
std::string identyfikator;
int id_mechanika = 0;

StanZegara* zegar;
Usluga* cennik;

#define TYP_ZLECENIE 10 

//FUNKCJE POMOCNICZE

void podlacz_zasoby() {
    semid = semget(SEM_KEY, 0, 0600);
    if (semid == -1) { perror("KLIENT: Blad semget"); exit(1); }

    msgid = msgget(MSG_KEY_MECHANIK, 0600);
    if (msgid == -1) { perror("KLIENT: Blad msgget"); exit(1); }

    shmid_uslugi = shmget(SHM_KEY_USLUGI, 0, 0600);
    if (shmid_uslugi == -1) { perror("KLIENT: Blad shmget Uslugi"); exit(1); }
    cennik = (Usluga*)shmat(shmid_uslugi, NULL, 0);

    shmid_zegar = shmget(SHM_KEY_ZEGAR, 0, 0600);
    if (shmid_zegar == -1) { perror("MECHANIK: Blad shmget Zegar"); exit(1); }
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);
}

// Funkcja obliczaj¹ca czas naprawy na podstawie listy us³ug
int oblicz_czas_naprawy(Wiadomosc* msg) {
    int czas_total = 0;
    for (int i = 0; i < msg->liczba_usterek; i++) {
        int szukane_id = msg->id_uslugi[i];
        for (int k = 0; k < MAX_USLUG; k++) {
            if (cennik[k].id == szukane_id) {
                czas_total += cennik[k].czas_bazowy;
                break;
            }
        }
    }
    // Zabezpieczenie, ¿eby naprawa nie trwa³a 0
    if (czas_total == 0) czas_total = 10;
    return czas_total;
}

void symuluj_prace(int jednostki_czasu) {
    int przelicznik = 50000;

    log(identyfikator, "Rozpoczynam naprawe... (Czas: " + std::to_string(jednostki_czasu) + ")");
    usleep(jednostki_czasu * przelicznik);
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        id_mechanika = std::stoi(argv[1]);
    }
    else {
        id_mechanika = getpid();
    }

    srand(getpid());
    identyfikator = "MECHANIK " + std::to_string(id_mechanika);
    podlacz_zasoby();

    log(identyfikator, "Gotowy do pracy.");

    Wiadomosc msg;

    while (true) {
        ssize_t wynik = msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), TYP_ZLECENIE, 0);

        if (wynik == -1) {
            if (errno == EINTR) continue;
            perror("MECHANIK: Blad msgrcv");
            break;
        }

        if (msg.marka_auta == 'U' || msg.marka_auta == 'Y') {
            if (id_mechanika == 8) P(semid, SEM_WARSZTAT_SPECJALNY);
            else P(semid, SEM_WARSZTAT_OGOLNY);
        }
        else {
            P(semid, SEM_WARSZTAT_OGOLNY);
        }

        log(identyfikator, "Pobralem auto Klienta " + std::to_string(msg.id_klienta) +
            " [" + std::string(1, msg.marka_auta) + "]");

        symuluj_prace(msg.czas_total);

        // Losowanie dodatkowej usterki (20%)
        int los = rand() % 100;
        if (los < 20) {
            log(identyfikator, "Znalazlem dodatkowa usterke! Pytam o zgode.");

            int id_nowej = cennik[rand() % 10].id;
            int cena_nowej = 0;
            int czas_nowej = 0;

            for (int k = 0; k < MAX_USLUG; k++) {
                if (cennik[k].id == id_nowej) {
                    cena_nowej = cennik[k].cena;
                    czas_nowej = cennik[k].czas_bazowy;
                    break;
                }
            }

            msg.cena_total += cena_nowej;

            msg.nadawca_pid = getpid();
            msg.czy_gotowe = false;
            msg.mtype = MSG_OD_MECHANIKA;
            msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
            V(semid, SEM_DZWONEK);

            // Czekamy na decyzjê
            log(identyfikator, "Czekam na decyzje klienta...");
            msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), getpid(), 0);

            if (msg.czy_zaakceptowano) {
                
                bool czy_wydluzyc = (rand() % 100) < 60;

                if (czy_wydluzyc) {
                    msg.czas_total += czas_nowej;
                    log(identyfikator, "Klient sie zgodzil. Naprawa wydluzy sie o " + std::to_string(czas_nowej));
                    symuluj_prace(czas_nowej);
                }
                else {
                    log(identyfikator, "Klient sie zgodzil. Udalo sie naprawic bez wydluzania czasu!");
                }
            }
            else {
                log(identyfikator, "Klient ODMOWIL. Koncze pierwotna naprawe.");
                msg.cena_total -= cena_nowej;
            }
        }

        // Koniec naprawy
        log(identyfikator, "Naprawa zakonczona. Zwalniam stanowisko.");

        if (msg.marka_auta == 'U' || msg.marka_auta == 'Y') {
            if (id_mechanika == 8) V(semid, SEM_WARSZTAT_SPECJALNY);
            else V(semid, SEM_WARSZTAT_OGOLNY);
        }
        else {
            V(semid, SEM_WARSZTAT_OGOLNY);
        }

        // --- TERAZ ZG£ASZAMY KONIEC ---
        msg.czy_gotowe = true;
        msg.mtype = MSG_OD_MECHANIKA;
        msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
        V(semid, SEM_DZWONEK);
    }

    return 0;
}