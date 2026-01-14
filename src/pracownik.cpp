#include "common.h"
#include <iostream>
#include <string>
#include <unistd.h>

// ZMIENNE GLOBALNE ---
int semid;
int msgid;
int shmid_zegar;
int shmid_uslugi;
std::string identyfikator;

StanZegara* zegar;
Usluga* cennik;

#define TYP_ZLECENIE 10 

// --- FUNKCJE POMOCNICZE ---

void podlacz_zasoby() {
    semid = semget(SEM_KEY, 0, 0600);
    if (semid == -1) { perror("PRACOWNIK: Blad semget"); exit(1); }

    msgid = msgget(MSG_KEY, 0600);
    if (msgid == -1) { perror("PRACOWNIK: Blad msgget"); exit(1); }

    shmid_zegar = shmget(SHM_KEY, 0, 0600);
    if (shmid_zegar == -1) { perror("PRACOWNIK: Blad shmget Zegar"); exit(1); }
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);

    shmid_uslugi = shmget(SHM_KEY + 1, 0, 0600);
    if (shmid_uslugi == -1) { perror("PRACOWNIK: Blad shmget Uslugi"); exit(1); }
    cennik = (Usluga*)shmat(shmid_uslugi, NULL, 0);
}

void wycen_naprawe(Wiadomosc* msg) {
    msg->cena_total = 0;
    msg->czas_total = 0;
    for (int i = 0; i < msg->liczba_usterek; i++) {
        int szukane_id = msg->id_uslugi[i];
        bool znaleziono = false;
        for (int k = 0; k < MAX_USLUG; k++) {
            if (cennik[k].id == szukane_id) {
                msg->cena_total += cennik[k].cena;
                msg->czas_total += cennik[k].czas_bazowy;
                znaleziono = true;
                break;
            }
        }
        if (!znaleziono) msg->cena_total += 50;
        msg->czas_total += 10;
    }
}

// Obsluga klienta

void obsluz_nowego_klienta(Wiadomosc msg) {
    wycen_naprawe(&msg);

    log(identyfikator, "Przyjmuje zgloszenie od Klienta " + std::to_string(msg.id_klienta) +
        ". Cena: " + std::to_string(msg.cena_total) +
        ", Czas: " + std::to_string(msg.czas_total));

    msg.mtype = msg.id_klienta;
    if (msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0) == -1) {
        perror("PRACOWNIK: Blad wysylania oferty");
        return;
    }

    Wiadomosc odp;
    if (msgrcv(msgid, &odp, sizeof(Wiadomosc) - sizeof(long), msg.id_klienta, 0) == -1) {
        perror("PRACOWNIK: Blad odbierania decyzji");
        return;
    }

    if (!odp.czy_zaakceptowano) {
        log(identyfikator, "Klient " + std::to_string(msg.id_klienta) + " ODRZUCIL wycene. Zwalniam go.");
        return;
    }

    log(identyfikator, "Klient " + std::to_string(msg.id_klienta) + " ZAAKCEPTOWAL wycene. Szukam stanowiska...");

    // Przydzial stanowiska mechanika do zlecenia
    bool czy_specjalne = false;
    if (odp.marka_auta == 'U' || odp.marka_auta == 'Y') {
        int wolne_spec = semctl(semid, SEM_WARSZTAT_SPECJALNY, GETVAL);
        if (wolne_spec > 0) {
            log(identyfikator, "-> Kieruje na stanowisko SPECJALNE (nr 8).");
            P(semid, SEM_WARSZTAT_SPECJALNY);
        }
        else {
            log(identyfikator, "-> Specjalne zajete. Kieruje na OGOLNE.");
            P(semid, SEM_WARSZTAT_OGOLNY);
        }
    }
    else {
        log(identyfikator, "-> Kieruje na stanowisko OGOLNE.");
        P(semid, SEM_WARSZTAT_OGOLNY);
    }

    odp.mtype = TYP_ZLECENIE;
    msgsnd(msgid, &odp, sizeof(Wiadomosc) - sizeof(long), 0);
}

// Obs³uga wiadomoœci od Mechanika (Koniec naprawy LUB Pytanie o dodatkowe usterki)
void obsluz_mechanika(Wiadomosc msg) {
    if (msg.czy_gotowe) {
        // Naprawa bez dodatkowych usterek
        log(identyfikator, "Mechanik skonczyl auto Klienta " + std::to_string(msg.id_klienta) + ". Wysylam informacje do Klienta.");

        msg.mtype = msg.id_klienta;
        msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
    }
    else {
        // Dodatkowe usterki
        log(identyfikator, "Mechanik zglasza dodatkowa usterke u Klienta " + std::to_string(msg.id_klienta) + ". Pytam o zgode.");

        msg.mtype = msg.id_klienta;
        msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);

        Wiadomosc odp;
        msgrcv(msgid, &odp, sizeof(Wiadomosc) - sizeof(long), msg.id_klienta, 0);

        if (odp.czy_zaakceptowano) {
            log(identyfikator, "Klient ZGODZIL SIE na dodatkowa naprawe. Przekazuje mechanikowi.");
        }
        else {
            log(identyfikator, "Klient ODMOWIL dodatkowej naprawy. Przekazuje mechanikowi.");
        }

        odp.mtype = TYP_ZLECENIE;
        msgsnd(msgid, &odp, sizeof(Wiadomosc) - sizeof(long), 0);
    }
}

// Obs³uga potwierdzenia wp³aty od Kasjera
void obsluz_kasjera(Wiadomosc msg) {
    log(identyfikator, "Kasjer potwierdzil wplate Klienta " + std::to_string(msg.id_klienta) + ". Wydaje kluczyki.");

    msg.mtype = msg.id_klienta;
    msg.czy_gotowe = true;
    msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
}

int main() {
    srand(getpid());
    identyfikator = "PRACOWNIK " + std::to_string(getpid());
    podlacz_zasoby();

    log(identyfikator, "Rozpoczynam zmiane.");

    Wiadomosc msg;

    while (true) {

        ssize_t wynik = msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), -3, 0);

        if (wynik == -1) {
            if (errno == EINTR) continue;
            perror("PRACOWNIK: Blad msgrcv");
            break;
        }

        if (msg.mtype == MSG_OD_KASJERA) {
            obsluz_kasjera(msg);
        }
        else if (msg.mtype == MSG_OD_MECHANIKA) {
            obsluz_mechanika(msg);
        }
        else if (msg.mtype == MSG_NOWY_KLIENT) {
            obsluz_nowego_klienta(msg);
        }
    }

    return 0;
}