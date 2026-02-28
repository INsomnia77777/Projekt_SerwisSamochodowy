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
    if (semid == -1) { perror("KLIENT: Blad semget"); exit(1); }

    msgid_klient = msgget(MSG_KEY_KLIENT, 0600);
    if (msgid_klient == -1) { perror("PRACOWNIK: Blad msgget_klient"); exit(1); }

    msgid_mechanik = msgget(MSG_KEY_MECHANIK, 0600);
    if (msgid_mechanik == -1) { perror("PRACOWNIK: Blad msgget_mechanik"); exit(1); }

    msgid_kasjer = msgget(MSG_KEY_KASJER, 0600);
    if (msgid_kasjer == -1) { perror("PRACOWNIK: Blad msgget_kasjer"); exit(1); }

    shmid_zegar = shmget(SHM_KEY_ZEGAR, 0, 0600);
    if (shmid_zegar == -1) { perror("KLIENT: Blad shmget Zegar"); exit(1); }
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);

    shmid_uslugi = shmget(SHM_KEY_USLUGI, 0, 0600);
    if (shmid_uslugi == -1) { perror("KLIENT: Blad shmget Uslugi"); exit(1); }
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

    // Sprawdzenie marki
    bool czy_obslugiwana = false;
    for (char m : MARKI_OBSLUGIWANE) {
        if (msg.marka_auta == m) {
            czy_obslugiwana = true;
            break;
        }
    }

    if (!czy_obslugiwana) {
        log(identyfikator, "Nie obslugujemy tej marki. Klient " + std::to_string(msg.id_klienta) +
            ", marka: " + std::string(1, msg.marka_auta));

        msg.mtype = msg.id_klienta;
        msg.czy_zaakceptowano = false;
        msg.cena_total = -1; 

        msgsnd(msgid_klient, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
        return;
    }

    // Wycenianie naprawy
    wycen_naprawe(&msg);

    log(identyfikator, "Przyjmuje zgloszenie od Klienta " + std::to_string(msg.id_klienta) +
        ". Cena: " + std::to_string(msg.cena_total) +
        ", Czas: " + std::to_string(msg.czas_total));

    msg.mtype = msg.id_klienta;
    if (msgsnd(msgid_klient, &msg, sizeof(Wiadomosc) - sizeof(long), 0) == -1) {
        perror("PRACOWNIK: Blad wysylania oferty");
        return;
    }

    Wiadomosc odp;
    if (msgrcv(msgid_klient, &odp, sizeof(Wiadomosc) - sizeof(long), msg.id_klienta, 0) == -1) {
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
    msgsnd(msgid_mechanik, &odp, sizeof(Wiadomosc) - sizeof(long), 0);
}

// Obs³uga wiadomoœci od Mechanika (Koniec naprawy LUB Pytanie o dodatkowe usterki)
void obsluz_mechanika(Wiadomosc msg) {
    if (msg.czy_gotowe) {
        // Naprawa bez dodatkowych usterek
        log(identyfikator, "Mechanik skonczyl auto Klienta " + std::to_string(msg.id_klienta) + ". Wysylam informacje do Klienta.");

        msg.mtype = msg.id_klienta;
        msgsnd(msgid_klient, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
    }
    else {
        // Dodatkowe usterki
        log(identyfikator, "Mechanik zglasza dodatkowa usterke u Klienta " + std::to_string(msg.id_klienta) + ". Pytam o zgode.");

        msg.mtype = msg.id_klienta;
        msgsnd(msgid_klient, &msg, sizeof(Wiadomosc) - sizeof(long), 0);

        Wiadomosc odp;
        msgrcv(msgid_klient, &odp, sizeof(Wiadomosc) - sizeof(long), msg.id_klienta, 0);

        if (odp.czy_zaakceptowano) {
            log(identyfikator, "Klient ZGODZIL SIE na dodatkowa naprawe. Przekazuje mechanikowi.");
        }
        else {
            log(identyfikator, "Klient ODMOWIL dodatkowej naprawy. Przekazuje mechanikowi.");
        }

        odp.mtype = TYP_ZLECENIE;
        msgsnd(msgid_mechanik, &odp, sizeof(Wiadomosc) - sizeof(long), 0);
    }
}

// Obs³uga potwierdzenia wp³aty od Kasjera
void obsluz_kasjera(Wiadomosc msg) {
    log(identyfikator, "Kasjer potwierdzil wplate Klienta " + std::to_string(msg.id_klienta) + ". Wydaje kluczyki.");

    msg.mtype = msg.id_klienta;
    msg.czy_gotowe = true;
    msgsnd(msgid_klient, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
}

int main() {
    srand(getpid());
    identyfikator = "PRACOWNIK " + std::to_string(getpid());
    podlacz_zasoby();

    log(identyfikator, "Rozpoczynam zmiane.");

    Wiadomosc msg;

    while (true) {

        P(semid, SEM_DZWONEK);

        // 1. Priorytet: Mechanik
        if (msgrcv(msgid_mechanik, &msg, sizeof(Wiadomosc) - sizeof(long), MSG_OD_MECHANIKA, IPC_NOWAIT) != -1) {
            obsluz_mechanika(msg);
        }
        // 2. Priorytet: Kasjer
        else if (msgrcv(msgid_kasjer, &msg, sizeof(Wiadomosc) - sizeof(long), MSG_OD_KASJERA, IPC_NOWAIT) != -1) {
            obsluz_kasjera(msg);
        }
        // 3. P³atnoœæ od klienta
        else if (msgrcv(msgid_klient, &msg, sizeof(Wiadomosc) - sizeof(long), MSG_PLATNOSC, IPC_NOWAIT) != -1) {
            msg.mtype = MSG_PLATNOSC;
            msgsnd(msgid_kasjer, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
        }
        // 4. Nowy Klient
        else if (msgrcv(msgid_klient, &msg, sizeof(Wiadomosc) - sizeof(long), MSG_NOWY_KLIENT, IPC_NOWAIT) != -1) {
            obsluz_nowego_klienta(msg);
        }
    }

    return 0;
}