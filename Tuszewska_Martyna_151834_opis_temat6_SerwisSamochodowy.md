https://github.com/MartynaTuszewska/Projekt_SerwisSamochodowy.git

Temat 6 – Serwis samochodów.
W pewnej miejscowości znajduje się serwis samochodów dostępny w godzinach od Tp do Tk. Serwis
obsługuje tylko samochody marek: A, E, I, O, U i Y. Pozostałe marki – z zakresu od A do Z (łącznie
26 różnych marek) nie są obsługiwane. W serwisie znajduje się 8 stanowisk do naprawy pojazdów,
przy czym na stanowiskach 1-7 możliwa jest naprawa marek A, E, I, O, U i Y, natomiast na stanowisku
8 możliwa jest naprawa tylko marek U i Y.
Samochody (marki z zakresu A-Z) pojawiają się w serwisie w losowej chwili (nawet poza godzinami
otwarcia) i są obsługiwane/serwisowane wg następujących zasad:
• Serwis obsługuje tylko samochody marek: A, E, I, O, U i Y;
• Jeżeli samochód przyjedzie poza godzinami pracy może czekać w kolejce (jeżeli usterka
krytyczna – określić 3 takie naprawy) lub czas do otwarcia krótszy niż T1;
• Czas naprawy każdego z pojazdów ustalany jest indywidualnie przez pracownika serwisu
(obsługa klienta) - kierowca/właściciel podaje zakres napraw – pracownik serwisu określa
przybliżony czas naprawy oraz przewidywany koszt naprawy określony w oparciu o cennik
(co najmniej 30 usług);
• Kierowca musi zaakceptować warunki naprawy (szacowany czas i kwotę) – ok. 2%
kierowców nie akceptuje warunków i odjeżdża z serwisu bez naprawy;
• W ok. 20% przypadków podczas diagnostyki na hali wychodzą dodatkowe usterki,
kwalifikujące się do naprawy – mechanik przekazuje te informacje do pracownika serwisu,
który komunikuje się z kierowcą i ustalają czy dodatkowe prace serwisowe mają być
wykonane czy nie. Ok. 20% kierowców nie zgadza się na rozszerzenie pierwotnego zakresu
napraw. Rozszerzenie zakresu może (nie musi) wydłużyć czas naprawy;
• Po zakończeniu naprawy mechanik przekazuje do pracownika serwisu formularz z zakresem
wykonanych napraw, pracownik serwisu ustala kwotę za usługę i informuje klienta o
możliwości odbioru samochodu;
• Klient po uiszczeniu opłaty w kasie odbiera kluczyki od pracownika serwisu i opuszcza
serwis;
Stanowisko bezpośredniej obsługi kierowców - pracownik serwisu – działa dodatkowo wg
następujących reguł:
• W serwisie są 3 stanowiska obsługi klientów, zawsze działa min. 1 stanowisko;
10
• Jeżeli w kolejce do rejestracji stoi więcej niż K1 kierowców (K1>=3) otwiera się drugie
stanowisko obsługi. Drugie stanowisko zamyka się jeżeli liczba klientów w kolejce jest
mniejsza niż lub równa 2;
• Jeżeli w kolejce do rejestracji stoi więcej niż K2 kierowców (K2>=5) otwiera się trzecie
stanowisko obsługi. Trzecie stanowisko zamyka się jeżeli liczba klientów w kolejce jest
mniejsza niż lub równa 3;
Kierownik serwisu po wysłaniu sygnału1 (do mechanika) może zamknąć dowolne stanowisko napraw
– jeżeli w momencie otrzymania sygnału mechanik był w trakcie obsługi samochodu, kończy jego
naprawę wg ustalonego spisu i zamyka stanowisko. Kolejne/oczekujące samochody zostają
przekierowane na inne stanowiska.
Kierownik serwisu po wysłaniu sygnału2 (do mechanika) może przyspieszyć czas naprawy
samochodów na danym stanowisku o 50% - próba kolejnego przyśpieszenia ma być ignorowana.
Przyśpieszyć można tylko proces, który pracuje w trybie normalnym;
Kierownik serwisu po wysłaniu sygnału3 (do mechanika) może przywrócić czas naprawy
samochodów na danym stanowisku do stanu pierwotnego – sygnał3 może być przyjęty tylko wtedy,
gdy wcześniej proces otrzymał sygnał2;
Kierownik serwisu po wysłaniu sygnału4 (pożar) zamyka cały serwis- mechanicy przerywają pracę,
wszyscy opuszczają serwis;
Napisz programy kierownika, pracownika serwisu, mechanika, kasjera i kierowcy, które zagwarantują
sprawną obsługę i korzystanie z serwisu zgodnie z regulaminem. Raport z przebiegu symulacji
zapisać w pliku (plikach) tekstowym.
