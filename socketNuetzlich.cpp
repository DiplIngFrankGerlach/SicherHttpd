#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern "C" {
#include <pthread.h>
}

#include <sys/wait.h>
#include <stdlib.h>
#include <iostream>
#include "zk.h"
#include "ProzedurVerwalter.h"
#include "socketNuetzlich.h"


/* sende die gesamte Anzhal Oktets, blockiere bis
   vollstaendig gesendet
*/
void sendAlles(int socket, const char* puffer, uint16_t anzahl)
{
   uint16_t anzGesendet = 0;
   do
   {
      int ergebnis = send(socket,puffer,anzahl,0);
      if( ergebnis < 1 )
      {
        return; // fehler, Verbindung abgebrochen ?
      }
      else
      {
         anzGesendet += ergebnis;
      }
   }
   while(anzGesendet < anzahl);

}

/* sende eine Null-Terminierte Zeichenkette an die Gegenstelle */
void sendAllesZK(int socket, const char* pufferNT)
{
  //cout << "sendAllesZK() " << pufferNT << endl;
  sendAlles(socket,pufferNT,strlen(pufferNT));
}

void sendeHTTPantwort(int socket, const Zeichenkette& antwort)
{
    Zeichenkette antwortMitKopfzeilen;
    antwortMitKopfzeilen = "HTTP/1.1 200 OK\r\n";
    antwortMitKopfzeilen.dazu("Content-Type: text/html\r\n");
    antwortMitKopfzeilen.dazu("Content-Length: ");
    antwortMitKopfzeilen.dazuZahl(antwort.laenge());
    antwortMitKopfzeilen.dazu("\r\n");
    antwortMitKopfzeilen.dazu("Connection: Closed\r\n\r\n");

    antwortMitKopfzeilen.dazu(antwort);

    sendAlles( socket,antwortMitKopfzeilen.zkNT(),antwortMitKopfzeilen.laenge() );
}


