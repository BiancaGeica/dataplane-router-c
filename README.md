Repository for the first homework of the Communication Networks class. In this homework the students will implement the dataplane of a router.

pas 1. make run_router0 de pe router0
pas 2.  make run_router1

## Exercitiul 1: Procesul de dirijare

In captura de ecran de la primul exercitiu se observa trimiterea a 7 pachete ICMP de request si reply in wireshark trimise intre host 3 si host 0.

In ambele terminale ale routerelor apar mesajele de debug care confirma trimiterea pachetelor, iar pentru pachetul care nu are protocolul IPv4, din cauza implementarii codului din momentul rezolvarii primului exercitiu care da drop tuturor pachetelor care nu au acest tip se pot observa erorile aferente. 

Deoarece incerc sa dau ping de la host 3 la host 0, pachetul este forwardat prin ambele routere, daca dau ping de la doua host-uri care sunt conectate la acelasi router se pot observa mesajele de debug doar pe routerul respectiv, deci captura de ecran nu ar fi la fel de sugestiva.

Alt detaliu important care se observa in wireshark este campul ttl care ajunge la destinatie cu valoarea 62 (pentru ca a trecut doua hopuri, adica prin cele doua routere) si pleaca cu valoarea intreaga, adica 64.
In ambele terminale ale routerelor apar mesajele de debug care confirma trimiterea pachetelor, iar pentru pachetul care nu are protocolul IPv4, din cauza implementarii codului din momentul rezolvarii primului exercitiu care da drop tuturor pachetelor care nu au acest tip se pot observa erorile aferente. 

Deoarece incerc sa dau ping de la host 3 la host 0, pachetul este forwardat prin ambele routere. Daca dau ping de la doua host-uri care sunt conectate la acelasi router se pot observa mesajele de debug doar pe routerul respectiv, deci captura de ecran nu ar fi la fel de sugestiva.

Alt detaliu important care se observa in wireshark este campul ttl (total time limit) care ajunge la destinatie cu valoarea 62 (pentru ca a trecut doua hopuri, adica prin cele doua routere) si pleaca cu valoarea intreaga, adica 64.