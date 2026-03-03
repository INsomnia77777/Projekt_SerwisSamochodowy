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
bool zamykam_stanowisko = false;
double mnoznik_czasu = 1.0;
int minuty_koniec_pracy = 0;

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

//ew mo¿na to poprawiæ na wektor z czasami zakoñczenia w pamiêci dzielonej do którego bêdzie przydzielony budzik, który bêdzie panowaæ nad tym haosem
void symuluj_prace(int jednostki_czasu) {
    int aktualne_minuty = (zegar->dzien * 1440) + (zegar->godzina * 60) + zegar->minuta;

    if (minuty_koniec_pracy <= aktualne_minuty) {
        minuty_koniec_pracy = aktualne_minuty;
    }

    int dodawany_czas = jednostki_czasu * mnoznik_czasu;
    minuty_koniec_pracy += dodawany_czas;

    log(identyfikator, "Rozpoczynam naprawe... (Koniec za " + std::to_string(dodawany_czas) + " min.)");

    while (true) {
        aktualne_minuty = (zegar->dzien * 1440) + (zegar->godzina * 60) + zegar->minuta;

        if (aktualne_minuty >= minuty_koniec_pracy) {
            break;
        }

        usleep(10000);
    }
}

// OBS£UGA SYGNA£ÓW

//Sygna³1 - zamkniêcie stanowiska
void sygnal1_zamkniecie(int sig) {
    zamykam_stanowisko = true;
    log(identyfikator, "Odebralem rozkaz od Kierownika! Zamkne stanowisko jak tylko skoncze naprawe.");
}

//Sygna³2 - przyœpieszenie pracy jednorazowo o 50%
void sygnal2_przyspieszenie(int sig) {
    if (mnoznik_czasu == 1.0) {
        mnoznik_czasu = 0.5;
        log(identyfikator, "Otrzymalem Sygnal 2! Przyspieszam prace stanowiska o 50%.");

        // jeœli mechanik jest w trakcie naprawy
        int aktualne_minuty = (zegar->dzien * 1440) + (zegar->godzina * 60) + zegar->minuta;
        if (minuty_koniec_pracy > aktualne_minuty) {
            int pozostalo = minuty_koniec_pracy - aktualne_minuty;
            int nowe_pozostalo = pozostalo * mnoznik_czasu;
            minuty_koniec_pracy = aktualne_minuty + nowe_pozostalo;

            log(identyfikator, "Skrocono trwajaca naprawe! Zostalo " + std::to_string(nowe_pozostalo) + " min.");
        }
    }
    else {
        log(identyfikator, "Otrzymalem Sygnal 2, ale juz pracuje w trybie przyspieszonym.");
    }
}

//Sygna³3 - przywrócenie normalnego tempa pracy
void sygnal3_przywrocenie(int sig) {
    if (mnoznik_czasu == 0.5) {
        mnoznik_czasu = 1.0;
        log(identyfikator, "Otrzymalem Sygnal 3! Odwolano przyspieszenie. Wracam do normalnego trybu.");

        int aktualne_minuty = (zegar->dzien * 1440) + (zegar->godzina * 60) + zegar->minuta;
        if (minuty_koniec_pracy > aktualne_minuty) {
            int pozostalo = minuty_koniec_pracy - aktualne_minuty;

            int nowe_pozostalo = pozostalo * 2;
            minuty_koniec_pracy = aktualne_minuty + nowe_pozostalo;

            log(identyfikator, "Wydluzono trwajaca naprawe z powrotem! Zostalo " + std::to_string(nowe_pozostalo) + " min.");
        }
    }
    else {
        log(identyfikator, "Otrzymalem Sygnal 3, ale pracuje w normalnym tempie.");
    }
}

// Sygna³4 - po¿ar
void ewakuacja(int sig) {
    log(identyfikator, "ALARM! Rzucam wszystko i uciekam z budynku!");
    if (zegar != nullptr) shmdt(zegar);
    if (cennik != nullptr) shmdt(cennik);

    exit(0);
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
    signal(4, ewakuacja);
    signal(1, sygnal1_zamkniecie);
    signal(SIGUSR1, sygnal2_przyspieszenie);
    signal(SIGUSR2, sygnal3_przywrocenie);

    log(identyfikator, "Gotowy do pracy. PID(" + std::to_string(getpid()) + ")");

    Wiadomosc msg;

    while (true) {
        ssize_t wynik = msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), TYP_ZLECENIE, 0);

        if (wynik == -1) {
            //SYGNA£1
            if (errno == EINTR) {
                if (zamykam_stanowisko) {
                    log(identyfikator, "Zamykam stanowisko - rozkaz Kierownika");

                    if (id_mechanika == 8) P(semid, SEM_WARSZTAT_SPECJALNY);
                    else P(semid, SEM_WARSZTAT_OGOLNY);

                    shmdt(zegar);
                    shmdt(cennik);
                    return 0;
                }
                continue;
            }
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
            msg.id_mechanika = id_mechanika;
            msg.czy_gotowe = false;
            msg.mtype = MSG_OD_MECHANIKA;
            msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
            V(semid, SEM_DZWONEK);

            // Czekamy na decyzjê
            log(identyfikator, "Czekam na decyzje Klienta " + std::to_string(msg.id_klienta) + "...");
            msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), getpid(), 0);

            if (msg.czy_zaakceptowano) {

                bool czy_wydluzyc = (rand() % 100) < 60;

                if (czy_wydluzyc) {
                    msg.czas_total += czas_nowej;
                    log(identyfikator, "Klient " + std::to_string(msg.id_klienta) + " zgodzil sie. Naprawa wydluzy sie o " + std::to_string(czas_nowej));
                    symuluj_prace(czas_nowej);
                }
                else {
                    log(identyfikator, "Klient " + std::to_string(msg.id_klienta) + " zgodzil sie. Udalo sie naprawic bez wydluzania czasu!");
                }
            }
            else {
                log(identyfikator, "Klient " + std::to_string(msg.id_klienta) + " ODMOWIL. Koncze pierwotna naprawe.");
                msg.cena_total -= cena_nowej;
            }
        }

        // Koniec naprawy
        log(identyfikator, "Naprawa dla Klienta " + std::to_string(msg.id_klienta) + " zakonczona. Zwalniam stanowisko.");

        if (!zamykam_stanowisko) {
            if (msg.marka_auta == 'U' || msg.marka_auta == 'Y') {
                if (id_mechanika == 8) V(semid, SEM_WARSZTAT_SPECJALNY);
                else V(semid, SEM_WARSZTAT_OGOLNY);
            }
            else {
                V(semid, SEM_WARSZTAT_OGOLNY);
            }
        }
        else {
            log(identyfikator, "Zamykam stanowisko - rozkaz Kierownika");
        }

        msg.id_mechanika = id_mechanika;
        msg.czy_gotowe = true;
        msg.mtype = MSG_OD_MECHANIKA;
        msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
        V(semid, SEM_DZWONEK);

        if (zamykam_stanowisko) {
            shmdt(zegar);
            shmdt(cennik);
            return 0;
        }
    }

    return 0;
}