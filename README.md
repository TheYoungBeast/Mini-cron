# Mini-cron

Demon uruchamiany w sposób następujący
```
./minicron <taskfile> <outfile>
```
Plik taskfile zawiera zadania zapisane w następującym formacie:
```
<hour>:<minutes>:<command>:<mode>
```
Przy czym command jest dowolnym programem.

Parametr mode może mieć jedną z trzech wartości:

- 0 - użytkownik chce otrzymać treść, jaką polecenie wypisało na standardowe wyjście (stdout)
- 1 - użytkownik chce otrzymać treść, jaką polecenie wypisało na wyjście błędów (stderr).
- 2 - użytkownik chce otrzymać treść, jaką polecenie wypisało na standardowe wyjście i wyjście błędów.

## Działanie
Demon wczytuje zadania z pliku i porządkuje je chronologicznie, a następnie zasypia. Budzi się, kiedy nadejdzie czas wykonania pierwszego zadania, tworzy proces potomny wykonujący zadanie, a sam znowu zasypia do czasu kolejnego zadania. Proces potomny wykonuje zadanie uruchamiając nowy proces (lub grupę procesów tworzącą potok w przypadku obsługi potoków). Informacje zlecane przez użytkownika (standardowe wejście/wyjście) są dopisywane do pliku outfile, poprzedzone informacją o poleceniu, które je wygenerowało. Demon kończy pracę po otrzymaniu sygnału SIGINT i ewentualnym zakończeniu bieżącego zadania (zadań). Informacje o uruchomieniu zadania i kodzie wyjścia zakończonego zadania umieszczane są w logu systemowym.

dodatkowo:
- Po otrzymaniu sygnału SIGUSR1 demon ponownie wczytuje zadania z pliku, porzucając ewentualne nie wykonane jeszcze zadania. Po otrzymaniu sygnału SIGUSR2 demon wpisuje do logu systemowego listę zadań, jakie pozostały do wykonania.
- Obsługa potoków, w miejscu  może wystąpić ciąg poleceń odseparowany znakami | (np ls -l | wc -l). W takiej sytuacji standardowe wyjście polecenia przesyłane jest do wejścia kolejnego.


### Biblioteka obsługi błędów.
Do kompilacji wymagana jest biblioteka [libexplain](http://libexplain.sourceforge.net/) 1.4.
Instalacja standardowa dodaj flage: -lexplain przy kompilacji.
