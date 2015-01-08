/*
Schachakademie Umbau auf Reed-Holz-Brett mit 81 LEDs

Lars Danielis 
Version 1.0 
Mo. 10. Januar 2011

Version 1.0a
So. 23. Januar 2011
Neu Anf. 08
Relais Ansteuerung optimiert

Version 1.1
Fr. 28. Januar 2011
Neu Anf. 09

Anforderungen
1 Reed-Matrix abscannen und bei Änderung 2 Relais aktivieren
2 Schlagen erkennen und letztes Absetzen ignorieren wenn innerhalb x ms (Abheben Abheben "Absetzen")
3 Korrektur - wenn letztes Figurhinstellen nicht erkannt wurde (Absetzen Anheben "Absetzen" alles auf selbem Feld)
4 Über einen Taster Relais Abschalten aktivieren - Modus Stellungswechsel
5 über einen Taster Drückenmodus aktivieren bei dem das Abheben ignoriert wird - Modus Stellungswechsel
6 Letzter Taster macht einen Startaufstellungscheck - ob auch alle Figuren erkannt wurden,
7 bzw. innerhalb von 5 Sekunden nach letzter Bewegung Relaisaktion wiederholen
8 bei schnellen Bewegungen wird manchmal das Absetzen an Mephisto gemeldet aber von ihm nicht erkannt (Siehe 3)
    zwischen Relaisaktionen liegen 800ms min - Relais sind min. 300ms angezogen
9 Wird eine Figur zu nahe über dem Brett über mehrere Felder hinweg gezogen werden die dazwischenliegenden Felder auch aktivert
    Entprellung notwendig. Erste und letzte Aktion bei schnell aufeinanderfolgende Kontakte belassen aber dazwischenliegende ignorieren
*/

//---------------------FÜR DEBUG KOMMENTAR ENTFERNEN
//#define debug_ld
//---------------------FÜR DEBUG KOMMENTAR ENTFERNEN

//---------------------------------------------------- Entprellung der Reedkontakte
bool Tast1;
bool Tast2;
bool Tast3;
bool Tast4;
bool Tast5;
bool TastIO=false;

//---------------------------------------------------- Erkennung Schlagen und dafür vermeiden des 2. Klicks beim Absetzen
bool VorletzteBewegungAbheben=false;
bool LetzteBewegungAbheben=false;
bool FigurGeschlagen=false;

//---------------------------------------------------- Erkennung Figurenbewegung
char AbhebenZeile = 10;
char AbhebenSpalte = 10;
char AbsetzenZeile = 10;
char AbsetzenSpalte = 10;
unsigned long LetztesRelaisZeit = 0;
bool StatusAbheben = false;
bool StatusAbsetzen  = false;

//---------------------------------------------------- Erkennung Korrektur (Anheben - Absetzen gleicher Figur auf gleichem Feld
bool Korrektur=false;
char LetztesAbsetzenSpalte = 10;
char LetztesAbsetzenZeile = 10;
char LetztesAbhebenSpalte = 10;
char LetztesAbhebenZeile = 10;

//---------------------------------------------------- Letzte Relaisbewegung speichern damit sie ggfls. wiederholt werden kann
char LetztesRelaisZeile = 10;
char LetztesRelaisSpalte = 10;
unsigned long LetztesAbhebenZeit = 0;
unsigned long LetztesAbsetzenZeit = 0;

//---------------------------------------------------- Tabellen der Schachfelder für Erkennung einer Änderung
bool Felder[8][8];
bool NeuFelder[8][8];

//---------------------------------------------------- Zeitstempel für die Ermittlung der Zeit für Ermittlung einer Aufstellung
unsigned long zeit1;
unsigned long zeit;

//---------------------------------------------------- Modus Stellungswechsel
bool Stellungswechsel = false;
unsigned long ZeitStellungswechsel = 0;

//---------------------------------------------------- Modus Drücken
bool Druecken = false;
unsigned long ZeitDruecken = 0;

//---------------------------------------------------- Positionskontrolle

//---------------------------------------------------- Entprellung
char VorbehaltAbsetzenSpalte=10;
char VorbehaltAbsetzenZeile=10;
bool VorbehaltAbsetzen=false;
char VorbehaltAbhebenSpalte=10;
char VorbehaltAbhebenZeile=10;
bool VorbehaltAbheben=false;

//---------------------------------------------------- Portdefinitionen
//------------------------Relais Zahlenreihen
#define Relais1 33
#define Relais2 35
#define Relais3 39
#define Relais4 41
#define Relais5 43
#define Relais6 47
#define Relais7 51
#define Relais8 53
//------------------------Relais Buchstabenspalten
#define RelaisA 49
#define RelaisB 23
#define RelaisC 25
#define RelaisD 27
#define RelaisE 29
#define RelaisF 31
#define RelaisG 37
#define RelaisH 45
//------------------------Reedkontakte Buchstabenspalten
#define ReedA 22
#define ReedB 24
#define ReedC 26
#define ReedD 28
#define ReedE 30
#define ReedF 32
#define ReedG 34
#define ReedH 36
//------------------------Reedkontakte Zahlenreihen
#define Reed1 38
#define Reed2 40
#define Reed3 42
#define Reed4 44
#define Reed5 46
#define Reed6 48
#define Reed7 50
#define Reed8 52
//------------------------ Tasten und LEDs Front
#define LEDDruecken 8
#define LEDStellungswechsel 9
#define Lautsprecher 7
#define TPos 10
#define TStellungswechsel 12
#define TDruecken 11
//------------------------------------------------------ Sonstiges
bool RelaisAusmachen=false;
int incomingByte=0;

//------------------------------------------------------ INIT
void setup()  
{ 
  //--------------------------------------------piep - Hallo wach
  tone(Lautsprecher,440,100); 

  //--------------------------------------------Serielle Schnittstelle initialisieren
  Serial.begin(115200);
  //--------------------------------------------IO-Mode einstellen
  pinMode(LEDDruecken, OUTPUT);          //LED
  pinMode(LEDStellungswechsel, OUTPUT);  //LED
  pinMode(Lautsprecher, OUTPUT);         //LED
  pinMode(TPos, INPUT);                  //Taster
  pinMode(TStellungswechsel, INPUT);     //Taster 
  pinMode(TDruecken, INPUT);             //Taster 
  digitalWrite(TPos, HIGH);              //Pullup Widerstand aktivieren
  digitalWrite(TStellungswechsel, HIGH); //Pullup Widerstand aktivieren
  digitalWrite(TDruecken, HIGH);         //Pullup Widerstand aktivieren

  //--------------------------------------------IO-Mode für alle Relais AUSGANG
  pinMode(Relais1, OUTPUT);
  pinMode(Relais2, OUTPUT);
  pinMode(Relais3, OUTPUT);
  pinMode(Relais4, OUTPUT);
  pinMode(Relais5, OUTPUT);
  pinMode(Relais6, OUTPUT);
  pinMode(Relais7, OUTPUT);
  pinMode(Relais8, OUTPUT);

  pinMode(RelaisA, OUTPUT);
  pinMode(RelaisB, OUTPUT);
  pinMode(RelaisC, OUTPUT);
  pinMode(RelaisD, OUTPUT);
  pinMode(RelaisE, OUTPUT);
  pinMode(RelaisF, OUTPUT);
  pinMode(RelaisG, OUTPUT);
  pinMode(RelaisH, OUTPUT);

  //-------------------------------------------IO-Mode für Reedkontakte
  //--------------------------Zahlenreihen einlesen nachdem ... (s.u.)
  pinMode(Reed1, INPUT);
  pinMode(Reed2, INPUT);
  pinMode(Reed3, INPUT);
  pinMode(Reed4, INPUT);
  pinMode(Reed5, INPUT);
  pinMode(Reed6, INPUT);
  pinMode(Reed7, INPUT);
  pinMode(Reed8, INPUT);
  //--------------Pullups aktivieren
  digitalWrite(Reed1, HIGH);
  digitalWrite(Reed2, HIGH);
  digitalWrite(Reed3, HIGH);
  digitalWrite(Reed4, HIGH);
  digitalWrite(Reed5, HIGH);
  digitalWrite(Reed6, HIGH);
  digitalWrite(Reed7, HIGH);
  digitalWrite(Reed8, HIGH);
  //-------------------------- ... nachdem eine Reedkontaktspalte aktiviert wurde
  pinMode(ReedA, OUTPUT);
  pinMode(ReedB, OUTPUT);
  pinMode(ReedC, OUTPUT);
  pinMode(ReedD, OUTPUT);
  pinMode(ReedE, OUTPUT);
  pinMode(ReedF, OUTPUT);
  pinMode(ReedG, OUTPUT);
  pinMode(ReedH, OUTPUT);
  //-------------------------- alle Reedkontaktspalten initialisieren (Auschalten = High -> LOW-aktiv)
  digitalWrite(ReedA, HIGH);
  digitalWrite(ReedB, HIGH);
  digitalWrite(ReedC, HIGH);
  digitalWrite(ReedD, HIGH);
  digitalWrite(ReedE, HIGH);
  digitalWrite(ReedF, HIGH);
  digitalWrite(ReedG, HIGH);
  digitalWrite(ReedH, HIGH);
  
  //------------------------------------------PlatinenLED für Lebenszeichen initialisieren
  pinMode(13,OUTPUT);
  digitalWrite(13, LOW);

  //------------------------------------------Arrays Löschen
  for (char i = 7; i; i--)
  {
    for (char n = 7; n; n--)
    {
      Felder[i][n] = 0;
      NeuFelder[i][n] = 0;
    }
  }

  //------------------------------------------Zeit für Startaufstellung Start
  zeit1=millis();

  Serial.print("Startaufstellung ermitteln: ");
  //------------------------------------------Startaufstellung ermitteln
  for (char i = 0; i<8; i++) // spaltenweise beginnend bei A
  {
    digitalWrite((ReedA + (i*2)), LOW); // Spalte einschalten
    delay(1); // Einschaltentprellung
    for (char n = 0; n<8; n++) // Spalte Zeilenweise abfragen
    {
      TastIO=false;  // letztes Abtastergebnis löschen
      do // Zelle entprellt abfragen - hierzu über einen Zeitraum von 3ms 3 mal abtasten
      {
        Tast1=!(digitalRead(Reed1+(n*2)));
        delay(1);
        Tast2=!(digitalRead(Reed1+(n*2)));
        delay(1);
        Tast3=!(digitalRead(Reed1+(n*2)));
        if (Tast1 == Tast2) // erst wenn alle 3 Abtastwerte gleich sind gehts weiter
        {
          if (Tast2 == Tast3) TastIO=true; // alle 3 Werte identisch -> entprellt
        }
      } 
      while (!TastIO); // , ansonsten wird wieder 3 mal abgetastet
      Felder[i][n]=Tast1; // Wert im Array speichern
    }
    digitalWrite((ReedA + (i*2)), HIGH); // Spalte wieder ausschalten
  }
  zeit=millis()-zeit1;//Zeitdifferenz seit Beginn Startaufstellung ermitteln
  Serial.print(zeit); //Ausgeben
  Serial.println("ms");
  #if not defined debug_ld   // Debugeinstellung ausgeben
  Serial.println("Debug Aus");
  #else
  Serial.println("Debug An");
  #endif
} 

//--------------------------------------------------------------------------Zyklischer Teil
void loop()  
{

  #if defined debug_ld // Für Debugzwecke kann über die Tastatur ein Zeichen eingegeben werden um die Ausgabe zu separieren
  if (Serial.available() > 0) // Im Buffer der seriellen Schnittstelle liegt ein Wert
  {
    incomingByte = Serial.read();       // Diesen Wert abholen
    Serial.println(byte(incomingByte)); // und wieder ausgeben um in der Ausgabe ein wenig zu separieren
  }
  #endif

  // Relais ausmachen?
  if (((millis() - LetztesRelaisZeit) > 300) && RelaisAusmachen) RelaisAus();
  // Aufstellung neu ermitteln
  for (char i = 0; i<8; i++) // spaltenweise beginnend bei A
  {
    if(!digitalRead(TDruecken)) ToggleDruecken();                  //Drücken Taset gedrückt?
    if(!digitalRead(TStellungswechsel)) ToggleStellunsgwechsel();  //Stellungswechsel Taset gedrückt?
    if(!digitalRead(TPos)) CheckPos();                             //Positionskontrolle bzw. letztes Relais Taste gedrückt?
    digitalWrite((ReedA + (i*2)), LOW); // Spalte einschalten
    digitalWrite(13, HIGH); // Lebenszeichen an buildin LED des Arduino geben
    delay(1); // Einschaltentprellung Spalteneinschaltung
    digitalWrite(13,LOW);   // genug Lebenszeichen
    for (char n = 0; n<8; n++) // Spalte Zeilenweise abfragen
    {
      TastIO=false;  // letztes io-Abtastergebnis löschen
      do // Zelle entprellt abfragen - hierzu 3 mal abtasten
      {
        Tast1=!(digitalRead(Reed1+(n*2))); // die drei Abtastwerte im Abstand
        delay(1);
        Tast2=!(digitalRead(Reed1+(n*2))); // von je einer Millisekunde
        delay(1);
        Tast3=!(digitalRead(Reed1+(n*2))); // merken
        if (Tast1 == Tast2) // erst wenn alle 3 Abtastwerte gleich sind gehts weiter
        {
          if (Tast2 == Tast3) TastIO=true;
        }
      } 
      while (!TastIO); // ansonsten wird wieder 3 mal abgetastet
      NeuFelder[i][n]=Tast1; // wenn io Wert in Array merken
    }
    digitalWrite((ReedA + (i*2)), HIGH);  //Spalte wieder ein schalten
  }

  // neue und alte Aufstellung vergleichen
  for (char i = 0; i<8; i++)
  {
    for (char n = 0; n<8; n++)
    {
      if (NeuFelder[i][n] < Felder[i][n]) //Differenz durch Abheben erkannt
      {
        Figurenton(1);  // akustische Rückmeldung
        AbhebenZeile = n; // neue Diff-Stelle merken
        AbhebenSpalte = i; // neue Diff-Stelle merken
        if (!Stellungswechsel) StatusAbheben = true; // Abheben ausführen
        Felder[i][n] = NeuFelder[i][n]; // Differenz löschen
        LetztesAbhebenZeit=millis();    // Aktionszeitstempel merken
        if (LetzteBewegungAbheben) VorletzteBewegungAbheben=true; // Für Erkennung Figur geschlagen
        LetzteBewegungAbheben=true;     // Für Erkennung Figur geschlagen
        #if defined debug_ld // Für Debugzwecke jede Bewegung auf dem Brett ausgeben
        Serial.print(byte(i+65));
        Serial.print(byte(n+49));
        Serial.println(char(94));
        #endif
      }

      if (NeuFelder[i][n] > Felder[i][n]) //Differenz durch Absetzen erkannt
      {
        Figurenton(2);  // akustische Rückmeldung
        AbsetzenZeile = n; // neue Diff-Stelle merken
        AbsetzenSpalte = i; // neue Diff-Stelle merken
        if (!Stellungswechsel) StatusAbsetzen = true; // Absetzen ausführen
        Felder[i][n] = NeuFelder[i][n]; // Differenz löschen
        LetztesAbsetzenZeit=millis();
        if (VorletzteBewegungAbheben && (millis()-LetztesAbhebenZeit) < 5000) FigurGeschlagen=true;
        LetzteBewegungAbheben=false;
        VorletzteBewegungAbheben=false;
        #if defined debug_ld
        Serial.print(byte(i+65));
        Serial.print(byte(n+49));
        Serial.println(char(95));
        #endif
      }
    }
  }
  
  //Relais ansteuern
  //Abheben zuerst prüfen
  if (StatusAbheben && !Stellungswechsel)
  {
    if(Druecken) // Während Lektionen wird man aufgefordert Felder zu drücken ohne Figur darauf, dann wird das Abheben ignoriert
    {
      Serial.println("Abheben ignoriert weil Drueckenmodus");
      StatusAbheben=false;
    }
    else
    {
      if((millis()-LetztesAbsetzenZeit) < 500)
      {
        VorbehaltAbhebenSpalte=AbhebenSpalte;
        VorbehaltAbhebenZeile=AbhebenZeile;
        VorbehaltAbheben = true;
        StatusAbheben = false;
      }
      else if (!RelaisAusmachen)
      {
        VorbehaltAbheben = false;
        Relais(AbhebenSpalte,AbhebenZeile,1);
        LetztesAbhebenZeile = AbhebenZeile;
        LetztesAbhebenSpalte = AbhebenSpalte;
      }
    }
  }

  //Absetzen danach
  if (StatusAbsetzen && !Stellungswechsel)
  {
    if (FigurGeschlagen && !Druecken)
    {
      Serial.println("Absetzen ignoriert weil Figur geschlagen");
      FigurGeschlagen=false;
      StatusAbsetzen=false;
    }
    else
    {
      if((LetztesAbhebenZeile == AbsetzenZeile) && (LetztesAbhebenSpalte == AbsetzenSpalte) && ((millis()-LetztesAbhebenZeit) < 700))
      {
        StatusAbsetzen=false;
        Serial.println("Absetzen ignoriert Abheben auf selbem Feld - korrigiert");
      }
      else
      {
        if((millis()-LetztesAbhebenZeit) < 500)
        {
          VorbehaltAbsetzenSpalte=AbsetzenSpalte;
          VorbehaltAbsetzenZeile=AbsetzenZeile;
          VorbehaltAbsetzen = true;
          StatusAbsetzen = false;
          FigurGeschlagen = false; // evtl. geschlagene Figur beim Prellen ignorieren
        }
        else if (!RelaisAusmachen)
        {
          VorbehaltAbsetzen = false;
          Relais(AbsetzenSpalte,AbsetzenZeile,2);
          LetztesAbsetzenZeile = AbsetzenZeile;
          LetztesAbsetzenSpalte = AbsetzenSpalte;
        }
      }
    }
  }
  
  //Vorbehalte?
  if(VorbehaltAbsetzen && (millis()-LetztesAbsetzenZeit > 1000) && (millis()-LetztesAbhebenZeit > 1000) && !RelaisAusmachen)
  {
    VorbehaltAbsetzen = false;
    VorbehaltAbheben = false;
    Relais(AbsetzenSpalte,AbsetzenZeile,2);
    LetztesAbsetzenZeile = AbsetzenZeile;
    LetztesAbsetzenSpalte = AbsetzenSpalte;
  }

  if(VorbehaltAbheben && (millis()-LetztesAbsetzenZeit > 1000) && (millis()-LetztesAbhebenZeit > 1000) && !RelaisAusmachen)
  {
    VorbehaltAbheben = false;
    Relais(AbhebenSpalte,AbhebenZeile,1);
    LetztesAbhebenZeile = AbhebenZeile;
    LetztesAbhebenSpalte = AbhebenSpalte;
  }

}

void RelaisAus()
{
  #if defined debug_ld
  tone(Lautsprecher,1000,5); //klick für Relaisimitation
  #endif
  digitalWrite(Relais1,LOW);
  digitalWrite(Relais2,LOW);
  digitalWrite(Relais3,LOW);
  digitalWrite(Relais4,LOW);
  digitalWrite(Relais5,LOW);
  digitalWrite(Relais6,LOW);
  digitalWrite(Relais7,LOW);
  digitalWrite(Relais8,LOW);
  digitalWrite(RelaisA,LOW);
  digitalWrite(RelaisB,LOW);
  digitalWrite(RelaisC,LOW);
  digitalWrite(RelaisD,LOW);
  digitalWrite(RelaisE,LOW);
  digitalWrite(RelaisF,LOW);
  digitalWrite(RelaisG,LOW);
  digitalWrite(RelaisH,LOW);
  digitalWrite(13,LOW);
  RelaisAusmachen = false;
}

void Relais(int NrS, int NrZ, int Vorgang)
{
  if ((millis()-LetztesRelaisZeit) > 800)//Anforderung 3 evtl. hiermit auch unterstützend
  {  
    switch(NrZ+1)
    {
      case 1:
        digitalWrite(Relais1,HIGH);
        break;
      case 2:
        digitalWrite(Relais2,HIGH);
        break;
      case 3:
        digitalWrite(Relais3,HIGH);
        break;
      case 4:
        digitalWrite(Relais4,HIGH);
        break;
      case 5:
        digitalWrite(Relais5,HIGH);
        break;
      case 6:
        digitalWrite(Relais6,HIGH);
        break;
      case 7:
        digitalWrite(Relais7,HIGH);
        break;
      case 8:
        digitalWrite(Relais8,HIGH);
        break;
      default:
        break;
    }
    switch(NrS+1)
    {
      case 1: //A
        digitalWrite(RelaisA,HIGH);
        break;
      case 2: //B
        digitalWrite(RelaisB,HIGH);
        break;
      case 3: //C
        digitalWrite(RelaisC,HIGH);
        break;
      case 4: //D
        digitalWrite(RelaisD,HIGH);
        break;
      case 5: //E
        digitalWrite(RelaisE,HIGH);
        break;
      case 6: //F
        digitalWrite(RelaisF,HIGH);
        break;
      case 7: //F
        digitalWrite(RelaisG,HIGH);
        break;
      case 8: //G
        digitalWrite(RelaisH,HIGH);
        break;
      default:
        break;
    }
    LetztesRelaisZeit = millis();
    RelaisAusmachen = true;

    #if defined debug_ld
    Serial.print("R");
    Serial.print(char(NrS+65));
    Serial.print("+");
    Serial.println(char(NrZ+49));
    #endif

    digitalWrite(13,HIGH);
    LetztesRelaisSpalte = NrS;
    LetztesRelaisZeile = NrZ;
    if(Vorgang == 1) StatusAbheben=false;
    if(Vorgang == 2) StatusAbsetzen=false;
  }  
}

void ToggleDruecken(void)
{
  if((millis()-ZeitDruecken) > 300) //Tastenentprellung
  {
    ZeitDruecken = millis(); // Zeit merken fü Tastenentprellung
    if(Druecken)  // Zustand toggeln
    {
      LetztesAbhebenZeile = 10;  // einige Flags löschen  weil anderer Betriebszustand
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Druecken = false;          // Drückenmodus aus weil vorher AN gewesen
      digitalWrite(LEDDruecken, LOW);  // Status anzeigen
      Serial.println("Drueckenmodus AUS"); // und schreiben
      tone(Lautsprecher,440,100);          // und sagen
      delay(100);
      tone(Lautsprecher,220,100);
    }
    else
    {
      LetztesAbhebenZeile = 10;  // einige Flags löschen weil anderer Betriebszustand
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Druecken = true;          // Drückenmodujs AN weil vorher AUS gewesen
      digitalWrite(LEDDruecken, HIGH);     // Status anzeigen
      Serial.println("Drueckenmodus AN");  // und schreiben
      tone(Lautsprecher,440,100);          // und sagen
      delay(100);
      tone(Lautsprecher,880,100);
    }
  }
}

void ToggleStellunsgwechsel(void)
{
  if((millis()-ZeitStellungswechsel) > 300) //Tastenentprellung
  {
    ZeitStellungswechsel = millis(); //Zeitstempel merken für Tastenentprellung
    if(Stellungswechsel) // Wenn bereits AN dann AUS machen
    {
      LetztesAbhebenZeile = 10;  // einige Flags löschen  weil anderer Betriebszustand
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Stellungswechsel = false;                   //Stellungswechselmodus AUS weil vorher AN gewesen
      digitalWrite(LEDStellungswechsel, LOW);     //anzeigen
      Serial.println("Stellungswechselmodus AUS");//schreiben
      tone(Lautsprecher,220,100);                 //und sagen
    }
    else // ansonsten AN machen
    {
      LetztesAbhebenZeile = 10;  // einige Flags löschen  weil anderer Betriebszustand
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Stellungswechsel = true;                  //Stellungswechselmodus AN weil vorher AUS gewesen
      digitalWrite(LEDStellungswechsel, HIGH);  //anzeigen
      Serial.println("Stellungswechsel AN");    //schreiben
      tone(Lautsprecher,880,100);               //und sagen
    }
  }
}

void CheckPos(void)
{
  char z1;
  char z2;
  char z3;
  char z4;
  char z;
  if ( ((millis()-LetztesRelaisZeit) < 5000)  && !Stellungswechsel ) // Wenn innerhalb der letzten 5 sek sich eine Figur bewegt hat dann wiederholen
  {
      Serial.print("Relais wiederholt ");                //schreiben daß wiederholt
      Relais(LetztesRelaisSpalte,LetztesRelaisZeile,0);  //es tun
      delay(500);                                        //Taste entprellen
  }
  else  // ansonsten Grundstellungscheck
  {
    z1=false;z2=false;z3=false;z4=false;
    z=7; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) z4=true;//fehlende Figur gefunden in Reihe 8
    }
    z=6; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) z3=true;//fehlende Figur gefunden in Reihe 7
    }
    z=1; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) z2=true;//fehlende Figur gefunden in Reihe 2
    }
    z=0; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) z1=true;//fehlende Figur gefunden in Reihe 1
    }
    if(!z1 && !z2 && !z3 && !z4) 
    {
      tone(Lautsprecher,1760,50);
      delay(100);
      tone(Lautsprecher,1760,50);
      Serial.println("Grundaufstellung erkannt");
    }
    else
    {
      tone(Lautsprecher,1760,50);
      delay(100);
      tone(Lautsprecher,1760,50);
      delay(100);
      tone(Lautsprecher,1760,50);
      delay(100);
      tone(Lautsprecher,1760,50);
      delay(300);
      if(z1)
      {
      Serial.println("Figur fehlt in Reihe 1");
        tone(Lautsprecher,220,100);
        delay(250);
      }
      if(z2 && z1) delay(750);
      if(z2)
      {
      Serial.println("Figur fehlt in Reihe 2");
        tone(Lautsprecher,220,100);
        delay(250);
        tone(Lautsprecher,220,100);
      }
      if(z3 && (z1 || z2)) delay(750);
      if(z3)
      {
      Serial.println("Figur fehlt in Reihe 7");
        tone(Lautsprecher,220,100);
        delay(250);
        tone(Lautsprecher,220,100);
        delay(250);
        tone(Lautsprecher,220,100);
      }
      if(z4 && (z1 || z2 || z3)) delay(750);
      if(z4)
      {
      Serial.println("Figur fehlt in Reihe 8");
        tone(Lautsprecher,220,100);
        delay(250);
        tone(Lautsprecher,220,100);
        delay(250);
        tone(Lautsprecher,220,100);
        delay(250);
        tone(Lautsprecher,220,100);
      }
    }
    delay(500);
  }
}

void Figurenton(char Vorgang)
{
  if(Stellungswechsel) 
  {
    tone(Lautsprecher,880,100); // Lauter Ton bei Abheben und Absetzen
  }
  else if(Druecken) 
  {
    if (Vorgang == 2) tone(Lautsprecher,880,100); // Lauter Ton nur beim Absetzen
  }
  else 
  {
    tone(Lautsprecher,1800,1); //tick für Figurerkennung im normalen Spiel
  }
}
