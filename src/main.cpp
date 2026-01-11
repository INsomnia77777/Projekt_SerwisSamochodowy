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


int main()
{
    std::cout << "Witamy w symulacji serwisu samochodowego\n";

    int shmid = shmget(PROJECT_KEY, sizeof(StanSerwisu), IPC_CREAT | 0600);
    blad(shmid, "shmget");

    StanSerwisu* stan = (StanSerwisu*)shmat(shmid, NULL, 0);
    if (stan == (void*)-1) blad(-1, "shmat");

    stan->czy_pozar = false;
    stan->liczba_klientow_w_kolejce = 0;
    for (int i = 0; i < MAX_MECHANIKOW; i++) stan->stanowisko_mechanika_zajete[i] = false;

    wczytaj_uslugi(stan->cennik);

    int msgid = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0600);
    blad(msgid, "msgget");

    shmdt(stan);
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msgid, IPC_RMID, NULL);

    return 0;
}
