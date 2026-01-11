//Klient -> sprawdzenie marki klienta, okreœlenie prztbli¿onego czasu naprawy i przewidywany koszt
//Mechanik -> (dodatkowe prace serwisowe) Pracownik -> Klient
//Mechanik -> (formularz zakoñczenia prac serwisowych), ustala kwotê -> Klient
//Kasjer -> (op³ata od klienta) oddanie kluczyków -> Klient

#include "common.h"
#include <unistd.h>
#include <csignal>

volatile sig_atomic_t czy_pracowac = 1;
void zakonczenie_sym(int sig) {
    czy_pracowac = 0;
}

bool czy_obslugiwana(char marka) {
    char samogloski[] = { 'A', 'E', 'I', 'O', 'U', 'Y' };
    for (char s : samogloski) {
        if (marka == s) return true;
    }
    return false;
}

int main() {
    signal(SIGINT, zakonczenie_sym);
    signal(SIGTERM, zakonczenie_sym);

    //PAMIEC DZIELONA
    int shmid = shmget(PROJECT_KEY, sizeof(StanSerwisu), 0600);
    if (shmid == -1) { perror("Pracownik: shmget"); return 1;}
    StanSerwisu* stan = (StanSerwisu*)shmat(shmid, NULL, 0);
    
    //KOLEJKA KOMUNIKATOW
    int msgid = msgget(MSG_QUEUE_KEY, 0600);
    if (msgid == -1) { perror("Pracownik: msgget"); return 1; }
    
    //SEMAFORY
    int semid = semget(PROJECT_KEY, 0, 0600);
    if (semid == -1) { perror("Pracownik: semget"); return 1; }


    pid_t mpid = getpid();
    loguj("PRACOWNIK" + std::to_string(mpid), "Zaczynam prace");

    Wiadomosc msg;

    while (czy_pracowac) {

        ssize_t rozmiar = msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 1, 0);

        while (true) {
            if (rozmiar == -1) {
                //Ctrl+C
                if (errno == EINTR) {
                    if (errno == EIDRM || errno == EINVAL) break;
                    continue;
                }
                if (errno == EIDRM || errno == EINVAL) { czy_pracowac = 0; break; }
                perror("Pracownik: msgrcv error");
                break;
            }
            break;
        }
        if (!czy_pracowac) break;

        pid_t pid_klienta = msg.nadawca_pid;
        loguj("PRACOWNIK", "Odebralem zgloszenie od PID: " + std::to_string(pid_klienta));
        if (stan->liczba_klientow_w_kolejce > 0) stan->liczba_klientow_w_kolejce--;

        bool wstepna_akceptacja = false;
        if (czy_obslugiwana(msg.marka_auta)) {
            wstepna_akceptacja = true;
            msg.czy_zaakceptowano = true;
            
            int indeks = msg.id_uslugi - 1;
            if (indeks >= 0 && indeks < MAX_USLUG-1) {
                msg.cena = stan->cennik[indeks].cena;
                msg.czas = stan->cennik[indeks].czas_bazowy;
            }
            else {
                msg.cena = 100;
                msg.czas = 1;
            }
            loguj("PRACOWNIK", "Wycena dla PID " + std::to_string(pid_klienta)
                + ": " + std::to_string(msg.cena) + " PLN");
        }
        else {
            wstepna_akceptacja = false;
            msg.czy_zaakceptowano = false;
            msg.cena = 0;
            loguj("PRACOWNIK", "Odrzucam klienta (zla marka)." + std::to_string(pid_klienta));
        }

        msg.mtype = pid_klienta;
        msg.nadawca_pid = mpid;

        usleep(500 * 1000);//pó³ sekundy symulacji pracy
        
        if (msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0) == -1) {
            perror("Pracownik: msgsnd offer failed");
            goto koniec_obslugi;
        }

        if (wstepna_akceptacja) {
            Wiadomosc odp;
            bool odebrano_decyzje = false;


            while (czy_pracowac) {
                // Czekamy na wiadomoœæ TYLKO od tego konkretnego klienta
                if (msgrcv(msgid, &odp, sizeof(Wiadomosc) - sizeof(long), mpid, 0) == -1) {
                    // Obs³uga przerwania wywo³ania funkcji systemowej
                    if (errno == EINTR) continue;
                    perror("Pracownik: blad odbierania decyzji");
                    break;
                }
                else {
                    // Sprawdzamy czy to na pewno ten klient (dla bezpieczeñstwa)
                    if (odp.nadawca_pid == pid_klienta) {
                        odebrano_decyzje = true;
                        break;
                    }
                }
            }

            if (odebrano_decyzje) {
                if (odp.czy_zaakceptowano) {
                    loguj("PRACOWNIK", "Klient " + std::to_string(pid_klienta) + " ZAAKCEPTOWAL.");
                    // Tutaj w przysz³oœci wyœlesz do Mechanika (Lab 10 wspomina o pamiêci dzielonej/semaforach)
                }
                else {
                    loguj("PRACOWNIK", "Klient " + std::to_string(pid_klienta) + " ODRZUCIL.");
                }
            }
        }

    koniec_obslugi:
        // Sygnalizujemy zwolnienie stanowiska (podniesienie semafora V)
        // Semafor nr 0 w naszym zbiorze odpowiada za wolne okienka
        V(semid, 0);
    }

    shmdt(stan);
    return 0;

}