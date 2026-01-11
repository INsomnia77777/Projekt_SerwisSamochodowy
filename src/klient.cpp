#include "common.h"
#include <ctime>

bool wylosuj_szanse(int procent) {
    if (procent <= 0) return false;
    if (procent >= 100) return true;
    return (rand() % 100) < procent;
}

int main() {

    // Segmenty pamiêci dzielonej (shmget, shmat)
    int shmid = shmget(PROJECT_KEY, sizeof(StanSerwisu), 0600);
    if (shmid == -1) {
        perror("Klient: shmget failed");
        exit(EXIT_FAILURE);
    }

    StanSerwisu* stan = (StanSerwisu*)shmat(shmid, NULL, 0);
    if (stan == (void*)-1) {
        perror("Klient: shmat failed");
        exit(EXIT_FAILURE);
    }

    //Kolejka komunikatów (msgget)
    int msgid = msgget(MSG_QUEUE_KEY, 0600);
    if (msgid == -1) {
        perror("Klient: msgget failed");
        exit(EXIT_FAILURE);
    }

    //Semafory
    int semid = semget(PROJECT_KEY, 0, 0600);
    if (semid == -1) { perror("Klient: semget failed"); return 1; }


    pid_t mpid = getpid();

    srand(time(NULL) ^ (mpid << 16)); // Unikalne ziarno losowania

    //Losowanie marki z zakresu A-Z
    //Na razie losujemy tylko z obs³ugiwanych
    int losowy_indeks = rand() % 6;
    char dostepne_marki[] = { 'A', 'E', 'I', 'O', 'U', 'Y' };
    char moja_marka = dostepne_marki[losowy_indeks]; //'A' + (rand() % 26);
    //Wybór us³ugi z cennika
    int moje_id_uslugi = 1 + (rand() % MAX_USLUG);
    stan->liczba_klientow_w_kolejce++;

    char bufor_czasu[10];
    sprintf(bufor_czasu, "%02d:%02d", stan->aktualna_godzina, stan->aktualna_minuta);
    loguj("Klient " + std::to_string(mpid), "Przyjechalem o godzinie: " + std::string(bufor_czasu) + ", czekam na wolne stanowisko.");

    P(semid, 0);

    loguj("KLIENT" + std::to_string(mpid), "Podszedlem do stanowiska nr. "); //tu w przyszlosci mozeee bedzie nr stanowiska 
    
    //Wys³anie zlecenia do Pracownika Serwisu
    Wiadomosc msg;
    msg.mtype = 1; // 1: Klient -> Pracownik
    msg.nadawca_pid = mpid;
    msg.marka_auta = moja_marka;
    msg.id_uslugi = moje_id_uslugi;
    msg.czy_zaakceptowano = false;

    if (msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0) == -1) {
        perror("Klient: msgsnd failed");
        V(semid, 0);
        shmdt(stan);
        return 1;
    }

    pid_t pid_pracownika = msg.nadawca_pid;

    // 1. Oczekiwanie na decyzjê o przyjêciu
    if (msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), mpid, 0) == -1) {
        perror("Klient: msgrcv (decyzja) failed");
        V(semid, 0);
        return 1;
    }
    if (!msg.czy_zaakceptowano) {
        loguj("KLIENT " + std::to_string(mpid), "Marka nieobslugiwana. Odjezdzam.");
    }
    else {
        // 2% szans, ¿e klientowi nie pasuje
        if (wylosuj_szanse(2)) {
            loguj("KLIENT " + std::to_string(mpid), " Rezygnuje z us³ugi.");
            msg.czy_zaakceptowano = false;
        }
        else {
            loguj("KLIENT " + std::to_string(mpid),  "Akceptuje warunki umowy. Zlecam naprawe.");
            msg.czy_zaakceptowano = true;
        }

        msg.mtype = mpid;
        msg.nadawca_pid = mpid;

        if (msgsnd(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), 0) == -1) {
            perror("Klient: msgsnd decision failed");
        }
    }

    // 2. Oczekiwanie na informacje o zakoñczeniu lub usterkach (uproszczone na ten moment)
    // W pe³nej wersji tutaj bêdzie kolejna pêtla lub msgrcv czekaj¹ce na sygna³ od Pracownika o odbiorze.
    
    //if (msgrcv(msgid, &msg, sizeof(Wiadomosc) - sizeof(long), mpid, 0) == -1) {
    //    perror("Klient: msgrcv (odbior) failed");
    //}
    //else {
    //    std::cout << "Klient [" << mpid << "]: Odebralem auto. Place i odjezdzam!" << std::endl;
    //}

    loguj("KLIENT " + std::to_string(mpid), "Wychodze z serwisu.");

    shmdt(stan);
    return 0;
}