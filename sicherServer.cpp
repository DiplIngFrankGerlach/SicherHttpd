/* Einfacher und sicherer Webserver, in C++.

   Erweiterbar durch benutzerdefinierten C++ Code.

   (C) Frank Gerlach 2017, frankgerlach.tai@gmx.de

   Kommerzielle Nutzung erfordert eine kostenpflichtige Lizenz.

*/




#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
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
#include "Streufeld.h"
#include "URLParser.h"
#include "LesePuffer.h"
#include "socketNuetzlich.h"
#include "KopfzeilenParser.h"
#include "OptionsLeser.h"
#include "ThreadWorkQueue.h"

using namespace std;


#define SNAME "Server: SicherServer\r\n"







void* arbeite(void* param);
void fehlermeldung(int);

void fatalerFehler(const char *);




int fahreHoch(u_short *);
void nichtRealisiert(int);
extern void meldeProzedurenAn();

ProzedurVerwalter g_prozedurVerwalter;
Zeichenkette g_htdocsVerzeichenis;
Zeichenkette g_PWherunterfahren;

ThreadWorkQueue<int> __workQueue;
const int cNumberOfWorkers(10);
pthread_t __threads[cNumberOfWorkers];

const int WORKER_SHUTDOWN = -100;

#define leerzeichen(x) isspace((int)(x))



class SocketSchliesserHelfer
{
   int m_socket;
public:
   SocketSchliesserHelfer(int socket): m_socket(socket)
   {
   }
   ~SocketSchliesserHelfer()
   {
      close(m_socket);
   }
};







class HTTPVerarbeiter
{
   int m_clientSocket;
   Lesepuffer* m_lesepuffer;
   Zeichenkette m_host;

   //SFzkzk m_kopfzeilen;

   void nichtRealisiert()
   {
	   Zeichenkette antwort;
	   antwort.dazu("HTTP/1.0 501 Method Not Implemented\r\n"
	                "Content-Type: text/html\r\n\r\n"
                    "<HTML><HEAD><TITLE>Method Not Implemented\r\n"
                    "</TITLE></HEAD>\r\n"
                    "<BODY><P>HTTP request method not supported.\r\n"
                    "</BODY></HTML>\r\n");
	   sendAlles(m_clientSocket,antwort.zkNT(),antwort.laenge());
   }


   bool verarbeiteProzedur(const Zeichenkette& prozedurName,
                           const SFzkzk& parameterListe)
   {
        //cout << "verarbeiteProzedur()" << endl;

        if( !g_prozedurVerwalter.fuehreAus(m_host,prozedurName,parameterListe,m_clientSocket) )
        {
            fehlermeldung();
            return false;
        }
        return true;
   }




   bool verarbeiteDateiAnforderung(Zeichenkette& url)
   {
       bool erfolg;
       Zeichenkette dateiPfad(50,erfolg);
       dateiPfad.dazu(g_htdocsVerzeichenis);

       dateiPfad.dazu(m_host);
       dateiPfad.dazu('/');
       dateiPfad.dazu(url);

       if ( dateiPfad.letztesZeichenIst('/') )
       {
          dateiPfad.dazu("index.html");
       }

       struct stat st;
       if (stat(dateiPfad.zkNT(), &st) == -1)
       {
           cout << "kann Datei " << dateiPfad.zkNT() << " nicht finden " << endl;
           bool erfolg;
           Zeichenkette zeile(1000, erfolg);
           int numchars(1);
           while ( numchars > 0 )  /* lese und verwerfe kopfZeilen */
           {
              numchars = leseZeile(zeile, 1000);
              zeile.leere();
           }
           melde404();
           return false;
       }
       else
       {
           serve_file(dateiPfad.zkNT());
           return true;
       }
   }

   //todo:entfernen
   int leseZeile(Zeichenkette& zeile,int maxZeichen)
   {
       bool erfolg;
       int anzahl(0);
       do
       {
          char zeichen = m_lesepuffer->leseZeichen(erfolg);
          if( !erfolg  || (anzahl > maxZeichen) || (zeichen == '\n') )
          {
              break;
          }
          else
          {
              if( zeichen != '\r')
              {
                 zeile.dazu(zeichen);
                 anzahl++;
              }

          }
       }
       while( true );

       return anzahl;
   }

   void fehlermeldung()
   {
       Zeichenkette antwort;
       antwort.dazu("HTTP/1.0 400 BAD REQUEST\r\n"
                    "Content-type: text/html\r\n"
                    "\r\n"
                    "<P>Your browser sent a bad request, "
                    "such as a POST without a Content-Length.\r\n");
       sendAlles( m_clientSocket, antwort.zkNT(),antwort.laenge() );
   }


   //TODO: Binaere Datei ???
   void sendeDatei(FILE *resource)
   {
       char buf[1024];

       fgets(buf, sizeof(buf), resource);
       while (!feof(resource))
       {
          sendAllesZK(m_clientSocket, buf);
          fgets(buf, sizeof(buf), resource);
       }
   }

   void kopfZeilen( const char *filename)
   {
       Zeichenkette antwort;
       antwort.dazu("HTTP/1.0 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "\r\n");
       sendAlles(m_clientSocket, antwort.zkNT(),antwort.laenge());
   }


   void melde404()
   {
       Zeichenkette antwort;
       antwort.dazu("HTTP/1.0 404 NOT FOUND\r\n"
                    "Content-Type: text/html\r\n"
                    "\r\n"
                    "<HTML><TITLE>Not Found</TITLE>\r\n"
                    "<BODY><P>The server could not find the file\r\n"
                    "</BODY></HTML>\r\n");
       sendAlles( m_clientSocket, antwort.zkNT(),antwort.laenge() );
   }

   /* Schicke eine Datei zum Webbrowser */
   void serve_file(const char *filename)
   {
       FILE *resource = NULL;
       int numchars = 1;

       bool erfolg(false);
       Zeichenkette zeile(1000,erfolg);

       while ( numchars > 0 )  /* read & discard kopfZeilen */
       {
         numchars = leseZeile(zeile, 1000);
         zeile.leere();
       }

       resource = fopen(filename, "r");
       if (resource == NULL)
       {
           melde404();
       }
       else
       {
          kopfZeilen(filename);
          sendeDatei(resource);
       }
       fclose(resource);
   }

   bool leseKopfzeilen()
   {
      return true;
   }





   bool m_erfolg;
public:
   HTTPVerarbeiter(int clientSocket):m_clientSocket(clientSocket),
                                     m_lesepuffer(NULL)
   {}


   void verarbeiteAnfrage()
   {
       bool erfolg;
       m_lesepuffer = new Lesepuffer(m_clientSocket,erfolg);

       if( m_lesepuffer == NULL )
       {
         sicherSturz("kein Speicher");
       }

       Zeichenkette ersteZeile(1025,erfolg);

       Zeichenkette methode;
       bool istProzedur;
       Zeichenkette prozedurName;

       SFzkzk parameterListe(5,erfolg);
       URLParser urlparser(m_lesepuffer);
       Zeichenkette urlPfad(20,erfolg);

       if( !urlparser.parseURL(methode,istProzedur,urlPfad,prozedurName,parameterListe) )
       {
          cout << "bad first HTTP line" << endl;
          return ;
       }

       Zeichenkette koName, koWert;
       KopfzeilenParser kop(m_lesepuffer);
       while( kop.leseZeile(koName, koWert))
       {
           cout << "Kopfz: " << koName.zkNT() << " " << koWert.zkNT() << endl;
           //m_kopfzeilen.trageEin(koName, koWert);
           if( koName == "Host")
           {
              HostWertPruefer hwp(&koWert);
              if(!hwp.pruefe())
              {
                 cout << "schlechter Host" << koWert.zkNT() << endl;
                 return;
              }
              else
              {
                 m_host = koWert;
              }

           }
       }

       if( m_host == "" )
       {
          cout << "host-Header nicht gesetzt, breche ab" << endl;
          return;
       }
       //cout << "methode: " << methode.zkNT() << endl;

       if( methode == "GET" )
       {
          if( istProzedur )
          {
             verarbeiteProzedur(prozedurName,parameterListe);
          }
          else
          {
             verarbeiteDateiAnforderung(urlPfad);
          }
       }
       else
       {
          nichtRealisiert();
       }
   }

   ~HTTPVerarbeiter()
   {
      delete m_lesepuffer;
      m_lesepuffer = NULL;
   }



};


/*arbeite die Anfrage auf einem Socket ab */
void* arbeite(void* param) //int client)
{
    
    while(true)
    {
       int clientSocket(-1);
       
       __workQueue.getWork(clientSocket);

       if( clientSocket == WORKER_SHUTDOWN )
       {
         return NULL;
       }
       if( clientSocket >= 0 )
       {
          SocketSchliesserHelfer sslh(clientSocket);

          HTTPVerarbeiter httpVerarbeiter(clientSocket);
          httpVerarbeiter.verarbeiteAnfrage();
       }
    }
   
    return NULL;
}




void fatalerFehler(const char *sc)
{
   sicherSturz(sc);
}




int fahreHoch(u_short *port)
{
    int acceptPort = 0;
    struct sockaddr_in name;

    acceptPort = socket(PF_INET, SOCK_STREAM, 0);
    if (acceptPort == -1)
    {
      fatalerFehler("socket");
    }

    int reuse = 1;
    if (setsockopt(acceptPort, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
    }


    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(acceptPort, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
       fatalerFehler("bind");
    }
    if (*port == 0)
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(acceptPort, (struct sockaddr *)&name, &namelen) == -1)
        {
            fatalerFehler("getsockname");
        }
        *port = ntohs(name.sin_port);
    }
    if (listen(acceptPort, 5) < 0)
    {
        fatalerFehler("listen");
    }

    meldeProzedurenAn();

    return acceptPort;
}


bool g_signalisiereHerunterfahren(false);


void fahreServerHerunter()
{
   for(uint8_t i=0; i < cNumberOfWorkers;i++)
   {
      __workQueue.insertWork(WORKER_SHUTDOWN);
   } 
}





int main(void)
{
    //starte Threads
    for(uint8_t i=0; i < cNumberOfWorkers; i++)
    {
      pthread_create(&__threads[i],NULL,arbeite,NULL);
    }    


    Zeichenkette dn;
    dn = "Einstellungen.txt";
    OptionsLeser optionsLeser(dn);
    wahrOderSturz(optionsLeser.leseDateiEin(),"Optionsdatei einlesen" );

    Zeichenkette name,portZK;

    name = "ListenPort";
    wahrOderSturz(optionsLeser.leseOption(name,portZK),"Option ListenPort" );

    name = "htdocsWurzelVerzeichnis";
    wahrOderSturz(optionsLeser.leseOption(name,g_htdocsVerzeichenis),"Option htdocsWurzelVerzeichnis" );

    name = "PWherunterfahren";
    wahrOderSturz(optionsLeser.leseOption(name,g_PWherunterfahren),"Option PWherunterfahren"  );



    int server_sock = -1;
    u_short port = atoi(portZK.zkNT());
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);


    server_sock = fahreHoch(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
      client_sock = accept(server_sock,
                           (struct sockaddr *)&client_name,
                           &client_name_len);

      if( g_signalisiereHerunterfahren )
      {
         fahreServerHerunter();
         break;
      }

      if (client_sock == -1)
      {
        perror("accept failed");
      }

      __workQueue.insertWork(client_sock);
    }

    close(server_sock);

    return(0);
}
