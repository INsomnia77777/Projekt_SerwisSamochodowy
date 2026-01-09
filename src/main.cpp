
#include <iostream>

int main()
{
    std::cout << "Hello World!\n";
}

//Klient - generator kierowców z markami samochodów A-Z

//Serwis - 8 stanowisk (1-7:A, E, I, O, U, Y; 8: U, Y); A, E, I, O, U, Y; cennik napraw (co najmniej 30 usług)
//Obsługa klienta - określa przybliżony czas naprawy oraz przewidywany koszt naprawy, 3 stanowiska (działa min. 1)
//Mechanik -  naprawia samochody, przekazuje do pracownika serwisu formularz z zakresem wykonanych napraw
// Kasjer - przyjmuje płatności od klientów
//Kierownik serwisu - sygnał1 (mechanik): zamyka dow stanowisko napraw, 
//                  - sygnał2 (mechanik): przyspieszyc czas naprawy o 50%
//                  - sygnał3 (mechanik): przywraca czas naprawy do stanu pierwotnego
//                  - sygnał4 (pozar): mechanicy przerywają pracę, wszytscy opuszczają serwis