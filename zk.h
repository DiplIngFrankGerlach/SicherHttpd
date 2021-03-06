#ifndef ZEICHENKETTE_HDR
#define ZEICHENKETTE_HDR

/* Sichere, hocheffiziente und komfortable C++ Stringklasse.

   (C) Frank Gerlach 2017, frankgerlach.tai@gmx.de

   Kommerzielle Nutzung erfordert eine kostenpflichtige Lizenz.

*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdint.h>
#include <iostream>
#include "feld.h"
#include "sicherSturz.h"

using namespace std;
 
class Zeichenkette
{
   char* m_puffer;
   uint64_t m_kapazitaet;//groesse des Puffers
   uint64_t m_gueltig;//anzahl Zeichen gueltig

   void richteEin()
   {
      m_puffer = NULL;
      m_kapazitaet = 0;
      m_gueltig = 0;
   }

public:
   Zeichenkette(const char* zkC)
   {
     richteEin();
     dazu(zkC);
   }
   
   Zeichenkette(const Zeichenkette& andere) 
   {
       richteEin();
       dazu(andere);
   }

   Zeichenkette(Zeichenkette& andere)
   {
      richteEin();
      dazu(andere);
   }

   Zeichenkette(uint64_t kapazitaet, bool& erfolg)
   {
      richteEin();
      erfolg = sichereKapazitaet(kapazitaet);
   }

   Zeichenkette() 
   {
       richteEin();
   }



   char operator[](uint64_t stelle) const
   {
      if( stelle < m_gueltig )
      {
         return m_puffer[stelle];
      }
      else
      {
         //perror("Indexfehler in Zeichenkette::operator[]");
         //exit(-1);        
         sicherSturz("Indexfehler in Zeichenkette::operator[]");
      }
      return 0xFF;//dummy
   }
 
   bool sichereKapazitaet(uint64_t mindestKapazitaet)
   {
       
       if( mindestKapazitaet > m_kapazitaet )
       {
          if( mindestKapazitaet > 500000000 )
          {
               cerr << "String length too large. Stopping program for safety reasons" << endl;
               exit(-1);   
          }
          uint64_t neuKapazitaet = (m_kapazitaet+1)*2;
          if( neuKapazitaet < mindestKapazitaet )
          {
              neuKapazitaet = mindestKapazitaet;
          }
          char* neuPuffer = new char[neuKapazitaet];
          if( neuPuffer == NULL )
          {
             return false;
          }
          for(uint64_t i=0; i < m_gueltig; i++)
          {
             neuPuffer[i] =  m_puffer[i];
          }
          delete[] m_puffer;
          m_puffer = neuPuffer;
          m_kapazitaet = neuKapazitaet;
       }
       return true;
   }

   bool dazu(const char* zeichenketteNT)
   {
      uint64_t laengeDazu = strlen(zeichenketteNT);
      return dazu(zeichenketteNT,laengeDazu);
   }

   bool dazu(const char* zeichenkette, uint64_t laengeZK)
   {   
       uint64_t neuLaenge = m_gueltig+laengeZK;
       if( !sichereKapazitaet(neuLaenge) )
       {
         return false;
       }

       uint64_t i,j;
       for(j=0,i=m_gueltig; i < neuLaenge; i++,j++)
       {
          m_puffer[i] = zeichenkette[j];
       }
       m_gueltig = neuLaenge;
       return true;
   }

   bool dazu(const Zeichenkette& zeichenkette)
   {
      return dazu(zeichenkette.m_puffer,zeichenkette.m_gueltig);
   }

   bool dazu(char zeichen)
   {
      if( m_gueltig == m_kapazitaet )
      {
          if( !sichereKapazitaet(m_gueltig+1) )
          {
            return false;
          }
      }
      m_puffer[m_gueltig++] = zeichen;
      return true;
   }

   /* fuege eine Zahl in ihrer Zeichenketten-Darstellung zur Zeichenkette dazu
      basis darf sein 2..36
   */
   bool dazuZahl(uint64_t wert, uint8_t basis = 10)
   {
      char puffer[20];
      if( wert == 0 )
      {
         if( !dazu('0') ) 
         { 
           return false;
         }
         else return true;
      }
      uint8_t ausgabeZeiger=0;
      while( wert > 0 )
      {
         uint8_t ziffer = wert%basis;
         if( ziffer <= 9 )
         {
            ziffer += '0';
         }
         else
         {
            ziffer += ('A' - 10);
         }
         puffer[ausgabeZeiger++] = ziffer;
         wert /= basis;
      }
      if( !sichereKapazitaet(m_gueltig+ausgabeZeiger) )
      {
         return false;
      }
      for(int8_t i=(ausgabeZeiger-1); i>=0; i--)
      {
         m_puffer[m_gueltig++] = puffer[i];
      }
      return true; 
   }

   bool weiseZu(const char* dazuNT)
   {
       leere();
       return dazu(dazuNT);
   }

   bool weiseZu(const Zeichenkette& zkDazu)
   {
       leere();
       return dazu(zkDazu);
   }

   void operator=(const char* dazuNT)
   {
         if(!weiseZu(dazuNT) )
         {
            sicherSturz("Speichermangel");
         }
   }
   void operator=(const Zeichenkette& dazu)
   {
         if(!weiseZu(dazu) )
         {
            sicherSturz("Speichermangel");
         }
   }

   uint64_t laenge() const
   {
      return m_gueltig;
   }

   bool operator==(const char* vergleichZK) const
   {
       return gleich(vergleichZK);
   }

   bool operator==(const Zeichenkette& vergleichZK) const
   {
       if(vergleichZK.m_gueltig == m_gueltig)
       {
          for(uint64_t i=0; i < m_gueltig; i++)
          {
             if( m_puffer[i] != vergleichZK.m_puffer[i] )
             {
                return false;
             }
          }
          return true;
       }
       return false;
   }

   bool gleich(const char* vergleichZK) const
   {
       uint64_t i=0;
       while( (i < m_gueltig) && vergleichZK[i] && (vergleichZK[i] == m_puffer[i]) )
       {
          i++;
       } 
       return (i == m_gueltig) && (vergleichZK[i] == 0);
   }

   /* gebe einen Nullterminierten "C String" zurueck */
   const char* zkNT()
   {
      if( !sichereKapazitaet(m_gueltig+1) )
      {
         return NULL;
      }
      m_puffer[m_gueltig] = 0;
      return m_puffer;
   }

   /* gebe einen NICHT Nullterminierten "String" zurueck */
   const char* zk() const
   {
      return m_puffer;
   }

   void leere(bool gebeSpeicherFrei=false)
   {
       m_gueltig = 0;
       if( gebeSpeicherFrei )
       {
          delete[] m_puffer;
          m_puffer = NULL;
          m_kapazitaet = 0;
       }
   }
 
   static bool istLeerzeichen(char zeichen)
   {
      switch( zeichen )
      {
         case ' ':
         case '\t':return true;
      }
      return false;
   }

   /* suche die "linke" Zeichenkette bis zu einem Leerzeichen.
      Gebe false zurück, wenn Speicher nicht angefordert werden kann */
   bool linksBisLeerzeichen(Zeichenkette& links) const
   {
        uint64_t i=0;
        char zeichen = 1;
        char puffer[2];
        puffer[1] = 0;
        links.leere();
        while( i < m_gueltig )
        {
           zeichen = (*this)[i++];
           if( istLeerzeichen(zeichen) )
           {
              break;
           }
           else
           {
              puffer[0] = zeichen;
              if( !links.dazu(puffer) )
              {
                 return false;
              }              
           }
        }
        return true;
   }

   bool letztesZeichenIst(char zeichen) const
   {
      if( m_gueltig > 0 )
      {
         return m_puffer[m_gueltig-1] == zeichen;
      }
      return false;
   }

   /* spalte die ZK nach einem Trennzeichen auf. Maximalzahl Spalten gegeben durch
      die Groesse des Feldes spalten.
   */
   void spalteAuf(char trenner, Feld<Zeichenkette>& spalten)
   {
      uint64_t spaltenZeiger = 0;
      uint64_t zeichenZeiger = 0;
      while( (zeichenZeiger < m_gueltig) && (spaltenZeiger < spalten.size()) )
      {
         char zeichen = m_puffer[zeichenZeiger++];
         if( zeichen == trenner )
         {
            spaltenZeiger++;
         }
         else
         {
            spalten[spaltenZeiger].dazu(zeichen);  
         }
      }
   }

   static char upperCase(char zeichen)
   {
      if( ('a' <= zeichen) && (zeichen <= 'z') )
      {
         return zeichen - 'a' + 'A';
      }
      return zeichen;
   }

   static char lowerCase(char zeichen)
   {
      if( ('A' <= zeichen) && (zeichen <= 'Z') )
      {
         return zeichen - 'A' + 'a';
      }
      return zeichen;
   }

   bool endetMit(Zeichenkette& zk)
   {
       if( zk.laenge() <= laenge() )
       {
          uint32_t i= laenge() - zk.laenge();
          for(uint32_t j=0 ; i < laenge(); i++)
          {
             if( upperCase(zk[j++]) != upperCase( (*this)[i] ) )
             {
                return false;
             }
          }
          return true;
       }
       return false;
   }

   void makeLowerCase()
   {
      uint32_t l=laenge();
      for(uint32_t i=0; i < l; i++)
      {
         m_puffer[i] = lowerCase(m_puffer[i]);
      }
   }
 
   void makeUpperCase()
   {
      uint32_t l=laenge();
      for(uint32_t i=0; i < l; i++)
      {
         m_puffer[i] = upperCase(m_puffer[i]);
      }
   } 

   void rechtsBisZeichen(char zeichen,Zeichenkette& rechts)
   {
     rechts = "";
     int32_t l = laenge();
     if( l == 0 )
     {
        return;
     }
     int32_t i=l-1;
     while( (i >= 0) && (m_puffer[i] != zeichen))
     {
         i--;
     }
     if( i < 0 )
     {
         i=0;
     }
     if( m_puffer[i] == zeichen )
     {
        i++;
     }
     
     rechts.sichereKapazitaet(l - i);
     while( i < l )
     {
        rechts.dazu(m_puffer[i++]);
     }
   }

   

   
 
   ~Zeichenkette()
   {
      delete[] m_puffer;
      m_puffer = NULL;
   }
};



#endif
