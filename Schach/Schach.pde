/*
Schachakademie Umbau auf Reed-Holz-Brett mit 81 LEDs

Lars Danielis 
Version 1.0 
Mo. 10. Januar 2011

Anforderungen
1 Reed-Matrix abscannen und bei Änderung 2 Relais aktivieren
2 Schlagen erkennen und letztes Absetzen ignorieren wenn innerhalb 1 Sekunde (Abheben Abheben "Absetzen")
3 Korrektur - wenn letztes Figurhinstellen nicht erkannt wurde (Absetzen Anheben "Absetzen" alles auf selbem Feld)
4 Über einen Taster Relais Abschalten aktivieren - Modus Stellungswechsel
5 über einen Taster Drückenmodus aktivieren bei dem das Abheben ignoriert wird - Modus Stellungswechsel
6 Letzter Taster macht einen Startaufstellungscheck - ob auch alle Figuren erkannt wurden,
7 bzw. innerhalb von 5 Sekunden nach letzter Bewegung Relaisaktion wiederholen
8 bei schnellen Bewegungen wird manchmal das Absetzen an Mephisto gemeldet aber von ihm nicht erkannt (Siehe 3)
    wenn das Abheben erst innerhalb 1200ms war dann wird das Relais 750ms lang geschaltet anstelle von 350
 */

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
char AbhebenZeile = 10;
char AbhebenSpalte = 10;
char AbsetzenZeile = 10;
char AbsetzenSpalte = 10;

//---------------------------------------------------- Erkennung Korrektur (Anheben - Absetzen gleicher Figur auf gleichem Feld
bool Korrektur=false;
char LetztesAbsetzenSpalte = 10;
char LetztesAbsetzenZeile = 10;
char LetztesAbhebenSpalte = 10;
char LetztesAbhebenZeile = 10;

//---------------------------------------------------- Letzte Relaisbewegung speichern damit sie ggfls. wiederholt werden kann
char LetztesRelaisZeile = 10;
char LetztesRelaisSpalte = 10;
float LetztesAbhebenZeit = 0;
float LetztesAbsetzenZeit = 0;

//---------------------------------------------------- Tabellen der Schachfelder für Erkennung einer Änderung
bool Felder[8][8];
bool NeuFelder[8][8];

//---------------------------------------------------- Zeitstempel für die Ermittlung der Zeit für Ermittlung einer Aufstellung
float zeit1;
float zeit;

//---------------------------------------------------- Modus Stellungswechsel
bool Stellungswechsel = false;
float ZeitStellungswechsel = 0;

//---------------------------------------------------- Modus Drücken
bool Druecken = false;
float ZeitDruecken = 0;

//---------------------------------------------------- Positionskontrolle
char Figurfehlt = false;
float ZeitPos = 0;

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
          if (Tast2 == Tast3) /* evtl. auskommentieren !!*/ TastIO=true;
        }
      } 
      while (!TastIO); // , ansonsten wird wieder 3 mal abgetastet
      Felder[i][n]=Tast1;
    }
    digitalWrite((ReedA + (i*2)), HIGH); // Spalte wieder ausschalten
  }
  zeit=millis()-zeit1;//Zeitdifferenz seit Beginn Startaufstellung ermitteln
  Serial.print(zeit); //Ausgeben
  Serial.println("ms");
} 

//--------------------------------------------------------------------------Zyklischer Teil
void loop()  
{

  // Aufstellung neu ermitteln
  for (char i = 0; i<8; i++) // spaltenweise beginnend bei A
  {
    digitalWrite((ReedA + (i*2)), LOW); // Spalte einschalten
    digitalWrite(13, HIGH); // Lebenszeichen geben
    delay(1); // Einschaltentprellung
    digitalWrite(13,LOW);
    for (char n = 0; n<8; n++) // Spalte Zeilenweise abfragen
    {
      TastIO=false;  // letztes Abtastergebnis löschen
      do // Zelle entprellt abfragen - hierzu über einen Zeitraum von 12ms 5 mal abtasten
      {
        if(!digitalRead(TDruecken)) ToggleDruecken();
        if(!digitalRead(TStellungswechsel)) ToggleStellunsgwechsel();
        if(!digitalRead(TPos)) CheckPos();
        Tast1=!(digitalRead(Reed1+(n*2)));
        delay(1);
        Tast2=!(digitalRead(Reed1+(n*2)));
        delay(1);
        Tast3=!(digitalRead(Reed1+(n*2)));
        if (Tast1 == Tast2) // erst wenn alle 5 Abtastwerte gleich sind gehts weiter
        {
          if (Tast2 == Tast3)  /* evtl. auskommentieren !!*/ TastIO=true;
        }
      } 
      while (!TastIO); // , ansonsten wird wieder 5 mal abgetastet
      NeuFelder[i][n]=Tast1;
    }
    digitalWrite((ReedA + (i*2)), HIGH);
  }

  // neue und alte Aufstellung vergleichen
  AbhebenZeile = 10; // alte Diff-Stelle löschen
  AbhebenSpalte = 10; // alte Diff-Stelle löschen
  AbsetzenZeile = 10; // alte Diff-Stelle löschen
  AbsetzenSpalte = 10; // alte Diff-Stelle löschen
  for (char i = 0; i<8; i++)
  {
    for (char n = 0; n<8; n++)
    {
      if (NeuFelder[i][n] < Felder[i][n]) //Abheben
      {
        AbhebenZeile = n; // neue Diff-Stelle merken
        AbhebenSpalte = i; // neue Diff-Stelle merken
        Felder[i][n] = NeuFelder[i][n]; // Differenz löschen
        LetztesAbhebenZeit=millis();
        if (LetzteBewegungAbheben) VorletzteBewegungAbheben=true;
        LetzteBewegungAbheben=true;
        Serial.print(byte(i+65));
        Serial.print(byte(n+49));
        Serial.print(char(94));
        Serial.print(" ");
      }
      if (NeuFelder[i][n] > Felder[i][n]) //Absetzen
      {
        AbsetzenZeile = n; // neue Diff-Stelle merken
        AbsetzenSpalte = i; // neue Diff-Stelle merken
        Felder[i][n] = NeuFelder[i][n]; // Differenz löschen
        LetztesAbsetzenZeit=millis();
        if (VorletzteBewegungAbheben && (millis()-LetztesAbhebenZeit) < 1500) FigurGeschlagen=true;
        LetzteBewegungAbheben=false;
        VorletzteBewegungAbheben=false;
        Serial.print(byte(i+65));
        Serial.print(byte(n+49));
        Serial.print(char(95));
        Serial.print(" ");
      }
    }
  }
  
  //Relais ansteuern
  //Abheben zuerst prüfen
  if (((AbhebenZeile < 10) || (AbhebenSpalte < 10)) && !Stellungswechsel)
  {
    if(!Druecken) // Während Lektionen wird man aufgefordert Felder zu drücken ohne Figur darauf, dann wird das Abheben ignoriert
    {
      tone(Lautsprecher,1800,1);
      SpalteRelaisAn(AbhebenSpalte);
      ZeileRelaisAn(AbhebenZeile);
      delay(350);
      AlleRelaisAus();
      delay(50);
    }
    else
    {
      Serial.println("Abheben ignoriert weil Drueckenmodus = true");
    }
    LetztesAbhebenZeile = AbhebenZeile;
    LetztesAbhebenSpalte = AbhebenSpalte;
  }

  //Absetzen danach
  if (((AbsetzenZeile < 10) || (AbsetzenSpalte < 10)) && !Stellungswechsel)
  {
    LetzteBewegungAbheben=false;
    VorletzteBewegungAbheben=false;
    if (FigurGeschlagen && !Druecken)
    {
      Serial.println("geschlagen");
      FigurGeschlagen=false;
    }
    else
    {
      if ((LetztesAbsetzenSpalte == LetztesAbhebenSpalte) && (LetztesAbhebenSpalte == AbsetzenSpalte) \
       && (LetztesAbsetzenZeile == LetztesAbhebenZeile) && (LetztesAbhebenZeile == AbsetzenZeile) \
       && !Druecken && ((millis()-LetztesAbhebenZeit) < 1000)) // Drückenmodus inaktiv und Korrektur innerhalb der letzten Sekunde
      {
        Serial.println("korrigiert");
      }
      else
      {
        tone(Lautsprecher,1800,1);
        SpalteRelaisAn(AbsetzenSpalte);
        ZeileRelaisAn(AbsetzenZeile);
        if ((millis()-LetztesAbhebenZeit) < 1200) delay(750); else delay(350); //Anforderung 3 evtl. hiermit auch unterstützend
        AlleRelaisAus();
        delay(50);
        LetztesAbsetzenZeile = AbsetzenZeile;
        LetztesAbsetzenSpalte = AbsetzenSpalte;
      }
    }
  }
}


void AlleRelaisAus()
{
//  Serial.println("R-");
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
}

void ZeileRelaisAn(int Nr)
{
    Serial.print("+");
    Serial.println(char(Nr+49));
    digitalWrite(13,HIGH);
    LetztesRelaisZeile = Nr;
    switch(Nr+1)
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
}

void SpalteRelaisAn(int Nr)
{
    Serial.print("R");
    Serial.print(char(Nr+65));
    digitalWrite(13,HIGH);
    LetztesRelaisSpalte = Nr;
    switch(Nr+1)
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
}

void ToggleDruecken(void)
{
  if((millis()-ZeitDruecken) > 300)
  {
    ZeitDruecken = millis();
    if(Druecken)
    {
      LetztesAbhebenZeile = 10;
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Druecken = false;
      digitalWrite(LEDDruecken, LOW);
      Serial.println("Drueckenmodus AUS");
      tone(Lautsprecher,440,100);
      delay(100);
      tone(Lautsprecher,220,100);
    }
    else
    {
      LetztesAbhebenZeile = 10;
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Druecken = true;
      digitalWrite(LEDDruecken, HIGH);
      Serial.println("Drueckenmodus AN");
      tone(Lautsprecher,440,100);
      delay(100);
      tone(Lautsprecher,880,100);
    }
  }
}

void ToggleStellunsgwechsel(void)
{
  if((millis()-ZeitStellungswechsel) > 300)
  {
    ZeitStellungswechsel = millis();
    if(Stellungswechsel)
    {
      LetztesAbhebenZeile = 10;
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Stellungswechsel = false;
      digitalWrite(LEDStellungswechsel, LOW);
      Serial.println("Stellungswechselmodus AUS");
      tone(Lautsprecher,220,100);
    }
    else
    {
      LetztesAbhebenZeile = 10;
      LetztesAbhebenSpalte = 10;
      LetzteBewegungAbheben=false;
      VorletzteBewegungAbheben=false;
      Stellungswechsel = true;
      digitalWrite(LEDStellungswechsel, HIGH);
      Serial.println("Stellungswechsel AN");
      tone(Lautsprecher,880,100);
    }
  }
}

void CheckPos(void)
{
  char z;
  if( ( ((millis()-LetztesAbhebenZeit) < 5000) || ((millis()-LetztesAbsetzenZeit) < 5000) ) && !Stellungswechsel )
  {
      tone(Lautsprecher,1800,1);
      SpalteRelaisAn(LetztesRelaisSpalte);
      ZeileRelaisAn(LetztesRelaisZeile);
      delay(350);
      AlleRelaisAus();
      delay(50);
  }
  else
  {
    tone(Lautsprecher,1600,20);
    z=7; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) Figurfehlt = 4;//fehlende Figur gefunden in Reihe 1
    }
    z=6; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) Figurfehlt = 3;//fehlende Figur gefunden in Reihe 2
    }
    z=1; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) Figurfehlt = 2;//fehlende Figur gefunden in Reihe 7
    }
    z=0; for (char b = 0; b<8; b++)
    {
      if (NeuFelder[b][z]==false) Figurfehlt = 1;//fehlende Figur gefunden in Reihe 8
    }
    if(!Figurfehlt) 
    {
      tone(Lautsprecher,1760,50);
      delay(100);
      tone(Lautsprecher,1760,50);
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
      for(char y = Figurfehlt; y; y--)
      {
        tone(Lautsprecher,220,100);
        delay(250);
      }
    }
    Figurfehlt = 0;
    delay(500);
  }
}
