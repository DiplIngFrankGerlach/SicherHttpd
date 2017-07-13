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
//#include "ProzedurVerwalter.h"
#include "Streufeld.h"
#include "URLParser.h"
#include "LesePuffer.h"

using namespace std;


#define SNAME "Server: SicherServer\r\n"







void* arbeite(void* param);
void fehlermeldung(int);
void sendeDatei(int, FILE *);
void fatalerFehler(const char *);
int leseZeile(Lesepuffer& lesepuffer, Zeichenkette& zeichenkette, int);
void kopfZeilen(int, const char *);
void melde404(int);
void serve_file(Lesepuffer&,int, const char *);
int fahreHoch(u_short *);
void nichtRealisiert(int);

#define leerzeichen(x) isspace((int)(x))

bool istNormalZeichen(char z)
{
   switch(z)
   {
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
      case 'i':
      case 'j':
      case 'k':
      case 'l':
      case 'm':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w':
      case 'x':
      case 'y':
      case 'z':
      case 'A':
      case 'B':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J':
      case 'K':
      case 'L':
      case 'M':
      case 'N':
      case 'O':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
      case 'T':
      case 'U':
      case 'V':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '/':return true;
      default: return false;
   }
   return false;
}


bool istZiffer(char z)
{
   switch(z)
   {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':return true;
      default: return false;
   }
   return false;
}


/* stelle sicher, dass in der URL keine illegalen Zeichen oder
   Konstrukte, insbesondere ".." vorhanden sind.
*/
bool pruefeURL(const Zeichenkette& url)
{
   if( url.laenge() == 0 )
   {
      return false;
   }
   uint16_t zeiger = 0;
   char zeichen = url[zeiger++]; 
  
   while( true )
   {
       if( !istNormalZeichen(zeichen) )
       {
          if( zeichen == '.' )
          {
             zeichen = url[zeiger++];
             if( zeichen && !istNormalZeichen(zeichen) )
             {
               return false;
             } 
          }
          else return false;
       }
       else
       {
         if( zeiger == url.laenge() )
         {
           break;
         }
         zeichen = url[zeiger++];
       }
   }
   return true; 
}

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




void nichtRealisiert(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	sendAllesZK(client, buf);
	sprintf(buf, SNAME);
	sendAllesZK(client, buf);
	sprintf(buf, "Content-Type: text/html\r\n");
	sendAllesZK(client, buf);
	sprintf(buf, "\r\n");
	sendAllesZK(client, buf);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	sendAllesZK(client, buf);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	sendAllesZK(client, buf);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	sendAllesZK(client, buf);
	sprintf(buf, "</BODY></HTML>\r\n");
	sendAllesZK(client, buf);
}

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


//Beispiel: /action_page.php?fname=Frank&lname=Gerlach

bool leseEinenParameter(Zeichenkette& zk,uint16_t& stelle, Zeichenkette& name, Zeichenkette& wert)
{
   if( stelle >= zk.laenge() )
   {
     return false;
   }
   while( (stelle < zk.laenge()) && (stelle < 16383) && (zk[stelle] != '=') )
   {
       name.dazu(zk[stelle++]);
   }
   stelle++;
   while( (stelle < zk.laenge()) && (stelle < 16383) && (zk[stelle] != '&') )
   {
       wert.dazu(zk[stelle++]);
   }
   stelle++;
   return true;
}

bool leseZahl(Zeichenkette& zk,uint64_t& zahl)
{
    zahl = 0;
    if( zk.laenge() == 0)
    {
       return false;
    }
    char zeichen;
    uint8_t stelle(0);
    do
    {
       zeichen = zk[stelle++];
       if( istZiffer(zeichen) )
       {
          zahl += (zeichen - '0');
       }
       else return false;
       if( stelle < zk.laenge() )
       {
           zahl *= 10;
       }
       else break;
    }
    while( true );
    return true;
}

bool verarbeiteProzedur(const Zeichenkette& prozedurName,const SFzkzk& parameterListe, int ausgabeSocket)
{
     //beispiel:
     //rechne?A=15&B=177

     cout << "verarbeiteProzedur()" << endl;

     if( prozedurName == "/rechne" )
     { 
          bool erfolg;
          Zeichenkette name(5,erfolg);
          Zeichenkette wert(10,erfolg);
          name = "A";
          uint64_t paramA(0);
          if( !parameterListe.finde(name,wert) || !leseZahl(wert,paramA) )
          {
              return false;
          }
          name = "B";
          if( parameterListe.finde(name,wert) )
          {
              
              uint64_t paramB(0);
              if( !leseZahl(wert,paramB) )
              {
                  return false;
              }
              uint64_t ergebnis = paramA+paramB; 

              bool erfolg;
              Zeichenkette antwort(100,erfolg);
              antwort.dazu("<html>\n"
                           "  <body>\n "
                           "    Das Ergebis ist: ");
              antwort.dazuZahl(ergebnis);
              antwort.dazu("</body></html>"); 
              sendAllesZK(ausgabeSocket,antwort.zkNT());
              return true;
          }
     }
     return false;
}




bool verarbeiteDateiAnforderung(Lesepuffer& lesepuffer,Zeichenkette& url,int clientSocket)
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
           numchars = leseZeile(lesepuffer, zeile, 1000);
           zeile.leere();
        }
        melde404(clientSocket);
        return false;
    }
    else
    {
        serve_file(lesepuffer,clientSocket, dateiPfad.zkNT());
        return true;
    }
}


int leseZeile(Lesepuffer& lesepuffer,Zeichenkette& zeile,int maxZeichen)
{
    bool erfolg;
    int anzahl(0);
    do
    {
       char zeichen = lesepuffer.leseZeichen(erfolg);
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


/*arbeite die Anfrage auf einem Socket ab */
void* arbeite(void* param) //int client)
{
    long long clientLL = (long long)param;
    int clientSocket = clientLL;
     

    SocketSchliesserHelfer sslh(clientSocket);
    

    bool erfolg;
    Lesepuffer lesepuffer(clientSocket,erfolg);
    
    Zeichenkette ersteZeile(1025,erfolg);
     
    Zeichenkette methode;
    bool istProzedur;
    Zeichenkette prozedurName;
     
    SFzkzk parameterListe(5,erfolg);
    URLParser urlparser(&lesepuffer);
    Zeichenkette urlPfad(20,erfolg);

    if( !urlparser.parseURL(methode,istProzedur,urlPfad,prozedurName,parameterListe) )
    {
       cout << "bad first HTTP line" << endl;
       return NULL;
    }

    cout << "methode: " << methode.zkNT() << endl;

    if ( ! ((methode == "GET") || (methode == "POST"))  )
    { 
        return NULL;
    }

    if( methode == "GET" )
    {
       if( istProzedur )
       {
          verarbeiteProzedur(prozedurName,parameterListe,clientSocket);
       }
       else
       {
          verarbeiteDateiAnforderung(lesepuffer,urlPfad,clientSocket);
       }          
    }
    else
    {
       nichtRealisiert(clientSocket);
    }
    return NULL;
}

void fehlermeldung(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, "Content-type: text/html\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, "\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    sendAllesZK(client, buf);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    sendAllesZK(client, buf);
}


//TODO: Binaere Datei ???
void sendeDatei(int client, FILE *resource)
{
 char buf[1024];

 fgets(buf, sizeof(buf), resource);
 while (!feof(resource))
 {
  sendAllesZK(client, buf);
  fgets(buf, sizeof(buf), resource);
 }
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
    return acceptPort;
}

void kopfZeilen(int client, const char *filename)
{
 char buf[1024];
 (void)filename;  /* could use filename to determine file type */

 strcpy(buf, "HTTP/1.0 200 OK\r\n");
 sendAllesZK(client, buf);
 strcpy(buf, SNAME);
 sendAllesZK(client, buf);
 sprintf(buf, "Content-Type: text/html\r\n");
 sendAllesZK(client, buf);
 strcpy(buf, "\r\n");
 sendAllesZK(client, buf);
}

 
void melde404(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, SNAME);
    sendAllesZK(client, buf);
    sprintf(buf, "Content-Type: text/html\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, "\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, "<BODY><P>The server could not find the file\r\n");
    sendAllesZK(client, buf);
    sprintf(buf, "</BODY></HTML>\r\n");
    sendAllesZK(client, buf);
}

/* Schicke eine Datei zum Webbrowser */
void serve_file(Lesepuffer& lesepuffer,int clientSocket, const char *filename)
{
 FILE *resource = NULL;
 int numchars = 1;
 
 bool erfolg(false);
 Zeichenkette zeile(1000,erfolg);

 while ( numchars > 0 )  /* read & discard kopfZeilen */
 {  
   numchars = leseZeile(lesepuffer, zeile, 1000);
   zeile.leere();
 }

 resource = fopen(filename, "r");
 if (resource == NULL)
 {
     melde404(clientSocket);
 }
 else
 {
    kopfZeilen(clientSocket, filename); 
    sendeDatei(clientSocket, resource);
 }
 fclose(resource);
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
