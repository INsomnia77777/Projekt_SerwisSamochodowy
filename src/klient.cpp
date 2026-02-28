#include "common.h"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <ctime>

// ZMIENNE GLOBALNE
int semid;
int msgid;
int shmid_zegar;
int shmid_uslugi;
std::string identyfikator;

StanZegara* zegar;
Usluga* cennik;

// FUNKCJE POMOCNICZE

void podlacz_zasoby() {
    semid = semget(SEM_KEY, 0, 0600);
    if (semid == -1) { perror("KLIENT: Blad semget"); exit(1); }

    msgid = msgget(MSG_KEY_KLIENT, 0600);
    if (msgid == -1) { perror("KLIENT: Blad msgget"); exit(1); }

    shmid_zegar = shmget(SHM_KEY_ZEGAR, 0, 0600);
    if (shmid_zegar == -1) { perror("KLIENT: Blad shmget Zegar"); exit(1); }
    zegar = (StanZegara*)shmat(shmid_zegar, NULL, 0);

    shmid_uslugi = shmget(SHM_KEY_USLUGI, 0, 0600);
    if (shmid_uslugi == -1) { perror("KLIENT: Blad shmget Uslugi"); exit(1); }
    cennik = (Usluga*)shmat(shmid_uslugi, NULL, 0);
}

// Funkcja generuj¹ca usterki (z wa¿onym losowaniem marek)
void generuj_usterki(Wiadomosc* msg) {
    msg->mtype = MSG_NOWY_KLIENT;
    msg->nadawca_pid = getpid();
    msg->id_klienta = getpid();
    msg->czy_zaakceptowano = false;
    msg->czy_gotowe = false;
    msg->cena_total = 0;

    std::vector<char> pula_marek;

    for (char c = 'A'; c <= 'Z'; c++) {
        pula_marek.push_back(c);
    }

    // Wrzucamy marki obs³ugiwane (Specjalne) dodatkowo 5 razy
    char obslugiwane[] = { 'A', 'E', 'I', 'O', 'U', 'Y' };
    for (char m : obslugiwane) {
        for (int k = 0; k < 5; k++) {
            pula_marek.push_back(m);
        }
    }
    msg->marka_auta = pula_marek[rand() % pula_marek.size()];

    // LOSOWANIE USTEREK
    int dostepne_uslugi_count = 0;
    while (dostepne_uslugi_count < MAX_USLUG && cennik[dostepne_uslugi_count].id != 0) {
        dostepne_uslugi_count++;
    }

    if (dostepne_uslugi_count == 0) { msg->liczba_usterek = 0; return; }

    msg->liczba_usterek = (rand() % 3) + 1; // 1 -3 usterek

    for (int i = 0; i < msg->liczba_usterek; i++) {
        int index = rand() % dostepne_uslugi_count;
        msg->id_uslugi[i] = cennik[index].id;
    }
}

// Sprawdzenie czy usterka jest krytyczna
bool mam_usterke_krytyczna(Wiadomosc* msg) {
    for (int i = 0; i < msg->liczba_usterek; i++) {
        if (czy_krytyczna(msg->id_uslugi[i])) return true;
    }
    return false;
}

bool czekaj_na_otwarcie(Wiadomosc* msg) {
    if (zegar->czy_otwarte) return true;

    int minuty_teraz = zegar->godzina * 60 + zegar->minuta;
    int minuty_otwarcie = SERWIS_OTWARCIE * 60;
    int diff;

    if (minuty_teraz > minuty_otwarcie) diff = (1440 - minuty_teraz) + minuty_otwarcie;
    else diff = minuty_otwarcie - minuty_teraz;

    bool krytyczna = mam_usterke_krytyczna(msg);
    bool zostaje = false;

    if (krytyczna) {
        log(identyfikator, "Serwis zamkniety (KRYTYCZNA). Czekam na otwarcie bramy (blokada).");
        zostaje = true;
    }
    else if (diff <= T1) {
        log(identyfikator, "Serwis zamkniety (otwarcie za " + std::to_string(diff) + " min). Czekam (blokada).");
        zostaje = true;
    }
    else {
        log(identyfikator, "Serwis zamkniety. Czas oczekiwania " + std::to_string(diff) + " min to za dlugo. Odjezdzam.");
        zostaje = false;
    }

    if (zostaje) {
        P(semid, SEM_ALARM);

        log(identyfikator, "Otwarto serwis. Wchodze");
        return true;
    }

    return false;
}

bool czy_krytyczna_lokalnie(Wiadomosc* msg) {
    for (int i = 0; i < msg->liczba_usterek; i++) {
        if (czy_krytyczna(msg->id_uslugi[i])) return true;
    }
    return false;
}

int main() {
    srand(getpid() + time(NULL));
    identyfikator = "KLIENT " + std::to_string(getpid());
    podlacz_zasoby();

    Wiadomosc msg;
    generuj_usterki(&msg);

    if (!zegar->czy_otwarte) {
        bool kryt = czy_krytyczna_lokalnie(&msg);
        if (!kryt) {
            // Jeœli to nie awaria krytyczna, klient mo¿e zrezygnowaæ, bo jest zamkniête
            if ((rand() % 100) < 50) { // 50% szans ¿e odjedzie
                log(identyfikator, "Serwis zamkniety. Nie chce mi sie czekac. Odjezdzam.");
                return 0;
            }
        }
        log(identyfikator, "Serwis zamkniety. Czekam na otwarcie (przed brama)...");
    }

    bramka_serwisu(semid);

    P(semid, SEM_LIMIT_KLIENTOW);

    P(semid, SEM_PRACOWNICY);

    log(identyfikator, "Chce oddac auto marki " + std::string(1, msg.marka_auta));

    msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
    V(semid, SEM_DZWONEK);

    if (msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), getpid(), 0) == -1) {
        perror("KLIENT: Blad msgrcv (negocjacje)");
        V(semid, SEM_PRACOWNICY); return 1;
    }

    if (msg.cena_total == -1) {
        log(identyfikator, "Pracownik odmowil przyjecia auta (" + std::string(1, msg.marka_auta) + "). Odjezdzam.");
        V(semid, SEM_PRACOWNICY);
        V(semid, SEM_LIMIT_KLIENTOW);
        return 0;
    }

    int szansa = rand() % 100;
    if (szansa < 2) {
        log(identyfikator, "Rezygnuje.");
        msg.czy_zaakceptowano = false;
        msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
        V(semid, SEM_PRACOWNICY);
        V(semid, SEM_LIMIT_KLIENTOW);
        return 0;
    }

    log(identyfikator, "Zlecam naprawe.");
    msg.czy_zaakceptowano = true;
    msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);

    V(semid, SEM_PRACOWNICY);

    while (true) {
        msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), getpid(), 0);

        if (msg.czy_gotowe) {
            break;
        }
        else {
            int decyzja = rand() % 100;
            if (decyzja < 20) {
                log(identyfikator, "Nie zgadzam sie na dodatkowa naprawe.");
                msg.czy_zaakceptowano = false;
            }
            else {
                log(identyfikator, "Zgadzam sie na dodatkowe koszty.");
                msg.czy_zaakceptowano = true;
            }
            msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
        }
    }

    log(identyfikator, "Auto gotowe! Ide do kasy. Kwota: " + std::to_string(msg.cena_total));

    P(semid, SEM_KASA);

    msg.mtype = MSG_PLATNOSC;
    msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0);
    V(semid, SEM_DZWONEK);

    log(identyfikator, "Zaplacilem. Czekam na kluczyki.");

    msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), getpid(), 0);

    log(identyfikator, "Mam kluczyki. Do widzenia!");
    V(semid, SEM_KASA);
    V(semid, SEM_LIMIT_KLIENTOW);
    shmdt(zegar);
    shmdt(cennik);

    return 0;
}