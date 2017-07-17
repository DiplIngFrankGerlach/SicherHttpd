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

using namespace std;


#define SNAME "Server: SicherServer\r\n"







void* arbeite(void* param);
void fehlermeldung(int);
 
void fatalerFehler(const char *);
 
 
 
 
int fahreHoch(u_short *);
void nichtRealisiert(int);
extern void meldeProzedurenAn();

ProzedurVerwalter g_prozedurVerwalter;

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
        cout << "verarbeiteProzedur()" << endl;

        if( !g_prozedurVerwalter.fuehreAus(prozedurName,parameterListe,m_clientSocket) )
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
       dateiPfad.dazu("/home/buero/htdocs");
       dateiPfad.dazu(url);

       if ( dateiPfad.letztesZeichenIst('/') )
       {
          dateiPfad.dazu("index.html");
       }

       struct stat st;
       if (stat(dateiPfad.zkNT(), &st) == -1) 
       {
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




public:
   HTTPVerarbeiter(int clientSocket):m_clientSocket(clientSocket),m_lesepuffer(NULL)
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

       cout << "methode: " << methode.zkNT() << endl;

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



};


/*arbeite die Anfrage auf einem Socket ab */
void* arbeite(void* param) //int client)
{
    long long clientLL = (long long)param;
    int clientSocket = clientLL;
     
    SocketSchliesserHelfer sslh(clientSocket);

    HTTPVerarbeiter httpVerarbeiter(clientSocket);
    httpVerarbeiter.verarbeiteAnfrage();
    
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








int main(void)
{
    int server_sock = -1;
    u_short port = 8000;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = fahreHoch(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
      client_sock = accept(server_sock,
                          (struct sockaddr *)&client_name,
                          &client_name_len);
      if (client_sock == -1)
      {
        perror("accept failed");
      }

      long long client_sockLL = client_sock;
      if (pthread_create(&newthread , NULL, arbeite, (void*) client_sockLL) != 0)
      {
         perror("pthread_create");
      }
    }

    close(server_sock);

    return(0);
}
