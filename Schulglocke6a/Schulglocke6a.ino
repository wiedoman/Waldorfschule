/* 
 Programm für die Schulglocke der Waldorfschule Geislingen 
 Lars Danielis
 Version 1.0 Erster Aufbau mit RTC von dfrobot ohne DCF77
 Version 2.0 Geänderter Aufbau mit RTC-DCF77 von ELV für genaue Atum-Uhrzeit per Funk
 Version 3.0 Code aufgeräumt und dokumentiert sowie um Kathodenreinigungsfunktion und Abschaltung in der Nacht ergänzt
 Version 4.0 DCF77 liefert manchmal ungültige Zeit, diese werden erkannt und es wird neu empfangen
 Version 6a  DCF77 entfernt und durch RTC von dfrobot ersetzt, Stellknopf: einmal drücken Stellmodus an und dann kann minutenweise die Uhr gestellt werden. Erneutes Drücken beendet den Stellmodus. Daten bis 2023 ergänzt.

 Debugmöglichkeit über serielle Schnittstelle
  Eingabe von z.B. 12:00:00 + Enter setzt die Zeit, es wird der : detektiert
  Eingabe von z.B. 01.01.14 + Enter setzt das Datum, es wird der . detektiert
  Eingabe von z.B. :0 setzt den Wochentag beginnend bei 0 mit Montag
  Eingabe von :: aktiviert den Debugmodus um Schaltzeiten zu prüfen ohne die Plausibilisierung zu aktivieren
  
 Verwendete Hardware:
 Für die Anzeige von Datum und Uhrzeit 
 Nixie Module for Arduino V2.0.0, geliefert von dfrobot
 Hardware Designer: Yan Zeyuan
 Blog: http://nixieclock.org
 Email: yanzeyuan@163.com
 Library Author: Weihong Guan (aGuegu)
 Blog: http://aguegu.net
 Email: weihong.guan@gmail.com
 Unterscheidung von Wochenende, Ferien, Unterricht, Uhrzeit, Datum, Pause und Gong über die Farbe der Hintergrundbeleuchtung der Nixies
 Uhrzeit am Wochende                      grün
 Uhrzeit in den Ferien                    Beleuchtung aus
 Uhrzeit in der Pause                     blau
 Uhrzeit während Unterricht               gelb
 Datum                                    weiss
 Uhrzeit während der Gong abgespielt wird grün/gelb im Sekundentakt abwechselnd
 
 EchtzeitUhr mit Empfang von Zeit und Datum per Funk 
 Sommer/Winterzeit und Schaltjahr sowie Schaltsekunden sind in den DCF77 Daten berücksichtigt und müssen nicht errechnet werden
 ELV RTC-DCF77, geliefert von ELV 
 
 MP3 Player für das Abspielen der Mp3-Datei mit dem 4fach Gong
 DFRduino Player V2.1 10/10/2013 for .NET Gadgeteer, geliefert von dfrobot 
 */

#include <SoftwareSerial.h> // für die Ansteuerung des MP3 Players (Rx/Tx Pin 4 und 5) ohne Einfluss auf das Terminal (Rx/Tx Pin 0 und 1)
#include "NixieTube.h"      // Bibliothek der Nixie Tube QS30-1 V2.0.0
#include <Wire.h>           // Ansteuerung der Nixie über SPI Pin 10-13
#include <stdio.h>          // Terminal und Pins
#include <ClickEncoder.h>   // Verstelldrehknopf
#include <TimerOne.h>       // Timer für die Auswertung des Drehknopfs

ClickEncoder *encoder;      // Drehknopf
int16_t last, value;

void timerIsr() {           // Timerinterrupt für Drehknopf-Auswertung
  encoder->service();
}

SoftwareSerial MP3Serial(4,5); //Serieller Port für MP3
 
#define RTC_Address   0x32  //RTC_Address
 

// im Setup muss das Setzen der Daten auskommentiert werden

unsigned char   date[7];
int Stunde,Minute,Sekunde,Wochentag,Tag,Monat,Jahr,Blink;            // Variablen für die Ausgelesene Zeit und das ausgelesene Datum
byte Unterricht;                                                      // kein Wochenende und keine Ferien
Color Farbe, AlteFarbe;                                               // Farbe der Hintergrundbeleuchtung der Nixie (alt für Blinken)
char clockString[30];                                                 // String für die Ausgabe auf das Terminal
int SekundenSeitEmpfang;                                              // Empfangsdauer
unsigned long millisAlt=0;

//  Unterrichtszeiten
//------------------------------------------------------------------------------------------------------------------------------
//                         Hauptunterricht
//                         |        1. Fachstunde
//                         |        |     2. Fachstunde
//                         |        |     |     3. Fachstunde
//                         |        |     |     |     4. Fachstunde
//                         |        |     |     |     |     5. Fachstunde
//                         |        |     |     |     |     |     6. Fachstunde
//                         |        |     |     |     |     |     |     7. Fachstunde
//                         |        |     |     |     |     |     |     |     8. Fachstunde
//                         |        |     |     |     |     |     |     |     |    
//                         -----    ----- ----- ----- ----- ----- ----- ----- -----
const byte StundeX[20]={ 7, 8, 9, 9,10,10,10,11,11,12,12,13,13,14,14,15,15,15,15,16};          // Daten für Unterrichtsstunden Stunde
const byte MinuteX[20]={55, 0,40,55, 0,45,50,35,45,30,35,20,25,10,15,00,05,50,55,40};          // Daten für Unterrichtsstunden Minute
//                      ----- -------- 
//                      |     |  ----- ----- ----- ----- ----- ----- ----- -----
//                      |     |  |     |     |     |     |     |     |     |
//                      |     |  |     |     |     |     |     |     |     7. Pause
//                      |     |  |     |     |     |     |     |     6. Pause
//                      |     |  |     |     |     |     |     5. Pause
//                      |     |  |     |     |     |     4. Pause
//                      |     |  |     |     |     3. Pause
//                      |     |  |     |     2. Pause
//                      |     |  |     1. Pause
//                      |     |  Ankündigung 1. Fachstunde 
//                      |     Grosse Pause
//                      Ankündigung Schulbeginn
//------------------------------------------------------------------------------------------------------------------------------

//  Ferienzeiten 2014 - 2020
//------------------------------------------------------------------------------------------------------------------------------
//                               1 Winterferien 2. Teil
//                               |     2 Fasching
//                               |     |     3 Ostern
//                               |     |     |     4 Tag der Arbeit
//                               |     |     |     |     5 Himmelfahrt
//                               |     |     |     |     |     6 Pfingsten
//                               |     |     |     |     |     |     7 Sommerferien 
//                               |     |     |     |     |     |     |     8 Tag der deutschen Einheit 
//                               |     |     |     |     |     |     |     |     9 Allerheiligen
//                               |     |     |     |     |     |     |     |     |     10 Winterferien 1. Teil
//                               |     |     |     |     |     |     |     |     |     |
//                               ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
const byte FerienTag14[20]    = { 1, 6, 3, 7,14,25, 1, 2,29,30, 9,20,31,12, 3, 4,27,31,22,31}; // Feriendaten für 2014 Tag 
const byte FerienMonat14[20]  = { 1, 1, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 9,10,10,10,10,12,12}; // Feriendaten für 2014 Monat
const byte FerienTag15[20]    = { 1, 6,16,20,30,10, 1, 2,14,15,25, 5,30,11, 3, 4, 2, 6,23,31}; // Feriendaten für 2015 Tag
const byte FerienMonat15[20]  = { 1, 1, 2, 2, 3, 4, 5, 5, 5, 5, 5, 6, 7, 9,10,10,11,11,12,12}; // Feriendaten für 2015 Monat
const byte FerienTag16[20]    = { 1, 8, 8,12,25, 1,30, 1, 5, 6,16,27,28, 9, 2, 3,31, 4,23,31}; // Feriendaten für 2016 Tag
const byte FerienMonat16[20]  = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 5, 5, 5, 7, 9,10,10,10,11,12,12}; // Feriendaten für 2016 Monat
const byte FerienTag17[20]    = { 1, 6,27, 3,10,21,30, 1,25,26, 5,16,27, 8, 2, 3,30, 3,25,31}; // Feriendaten für 2017 Tag
const byte FerienMonat17[20]  = { 1, 1, 2, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 9,10,10,10,11,12,12}; // Feriendaten für 2017 Monat
const byte FerienTag18[20]    = { 1, 7,12,16,26, 6,30, 1,10,11,21,02,26, 8, 3, 4, 1, 2,22,31}; // Feriendaten für 2018 Tag
const byte FerienMonat18[20]  = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 5, 5, 6, 7, 9,10,10,11,11,12,12}; // Feriendaten für 2018 Monat
const byte FerienTag19[20]    = { 1, 5, 4, 8,15,27, 1, 2,30,31,11,21,29,10, 3, 4,28,30,21,31}; // Feriendaten für 2019 Tag
const byte FerienMonat19[20]  = { 1, 1, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 9,10,10,10,10,12,12}; // Feriendaten für 2019 Monat
const byte FerienTag20[20]    = { 1, 4,24,28, 6,18, 1, 2,21,22, 2,13,30,12, 3, 4,26,30,23,31}; // Feriendaten für 2020 Tag
const byte FerienMonat20[20]  = { 1, 1, 2, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 9,10,10,10,10,12,12}; // Feriendaten für 2020 Monat
const byte FerienTag21[20]    = { 1, 9,15,19, 6,10, 1, 2,13,14,25, 5,29,11, 3, 4, 2, 6,23,31}; // Feriendaten für 2021 Tag
const byte FerienMonat21[20]  = { 1, 1, 2, 2, 4, 4, 5, 5, 5, 5, 5, 6, 7, 9,10,10,11,11,12,12}; // Feriendaten für 2021 Monat
const byte FerienTag22[20]    = { 1, 8,28, 4,19,23, 1, 2,26,27, 7,18,28,10, 3, 4, 2, 4,21,31}; // Feriendaten für 2022 Tag
const byte FerienMonat22[20]  = { 1, 1, 2, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 9,10,10,11,11,12,12}; // Feriendaten für 2022 Monat
const byte FerienTag23[20]    = { 1, 7,20,24,11,15, 1, 2,18,19,30, 9,28,10, 3, 4, 2, 4,21,31}; // Feriendaten für 2023 Tag
const byte FerienMonat23[20]  = { 1, 1, 2, 2, 4, 4, 5, 5, 5, 5, 5, 6, 7, 9,10,10,11,11,12,12}; // Feriendaten für 2023 Monat
//------------------------------------------------------------------------------------------------------------------------------

byte FerienTag[20],FerienMonat[20];             // Kopie der Feriendaten des aktuellen Jahres
byte Auswertung[20];                            // Hilfstabelle für die Auswertung der Unterrichtszeiten und der Ermittlung was aktuell ist, 
// Unterricht oder Pause und welcher Unterricht und welche Pause
// byte AuswertungFerien[20];                      // Hilfstabelle für die Auswertung der Ferien und der Ermittlung ob aktuell Ferien sind 
byte DebugAktiv;                                // Für Testzwecke, z.B. die Plausibilisierung umgehen
byte unplausibel = 99;                          // Letzter Zeitstempel war nicht plausibel, alten Wert behalten
byte ErsterEmpfangIO = 0;                       // warten auf den ersten erfolgreichen Empfang - dem ELV-RTC ist nicht zu trauen
byte StellModus = 0;                            // Stellmodus um über den Drehknopf die Uhrzeit zu stellen
byte FlashRichtung = 0;                         // Hintergrundbeleuchtung der Nixies atmen lassen damit der Stellmodus angezeigt wird
byte FlashHelligkeit = 144;
int KorrekturMinute, KorrekturStunde, KorrekturTimeout;

#define Einfach 0 //Gongart
#define Doppelt 1 //Gongart

#define COUNT	4 // 4 Nixie Module vorhanden
NixieTube tube(11, 12, 13, 10, COUNT); // pin_ds, pin_st. pin_sh, pin_oe(pwm pin is preferred), COUNT

void setup()
{
  DebugAktiv=0;     // DebugSession inaktiv

  Serial.begin(19200);  // Ausgaben auf das Terminal zu Prüf- und Entwicklungszwecken
  MP3Serial.begin(19200);// Ausgabe an MP3 zum Abspielen des 4fach Gongs

  Wire.begin();    // Kommunikation zu Dixie initialiseren
  Farbe = Black;          // erst mal keine Hintergrundbeleuchtung solange nicht klar ist ob Unterricht, Pause, Ferien oder Wochenende ist
  FarbeSetzen();          // Farbwahl an Nixies übertragen
  tube.setBrightness(0x90);	// Helligkeit der Nixies auf ca. 30% um Lebensdauer zu erhöhen (brightness control, 0x00(off)-0xff)
  tube.display();         // Daten an Nixies übertragen

  pinMode(6, OUTPUT);    // Pin für internen Lautsprecher für Relais um Brummen zu verhindern während der Gong nicht läuft
  pinMode(7, OUTPUT);    // Pin für externen Lautsprecher für Relais um Brummen zu verhindern während der Gong nicht läuft
  digitalWrite(6, LOW);  // Relais aus
  digitalWrite(7, LOW);  // Relais aus
  encoder = new ClickEncoder(A1, A0, A2, 4); // Drehknopf an A1 und A0 angeschlossen, Knopf an A2
  Timer1.initialize(1000);                   // Timer für Drehknopf
  Timer1.attachInterrupt(timerIsr); 
  last = -1;
  //I2CWriteDate();//Write the Real-time Clock

}

void loop()
{
  millisAlt=millis();
  I2CReadDate();  //Read the Real-time Clock    
  Data_process(); //Process the data
  printClock();                      // Und auf das Terminal ausgeben

  if (NixieLD())                   // Solange die Nixies nicht ausgeschaltet sind ...
  {
    if (Wochentag == 5 || Wochentag == 6)  // Prüfung der Wochentage auf Wochenender (Samstag oder Sonntag)
    {
      Serial.println(" WE");       // Wenn Samstag oder Sonntag der aktuelle Tag ist, dann Ausgabe von Wochende 
      Farbe = Blue;                        // und Hintergrundbeleuchtung auf blau setzen
      FarbeSetzen();                       // Farbwahl übertragen
    }
    else if (Ferien())                     // Falls kein Wochendende damm Prüfroutine starten um zu ermitteln ob Ferien sind
    {
      Farbe = Black;                       // und Hintergrundbeleuchtung ausschalten
      FarbeSetzen();                       // Farbwahl übertragen
    } 
    else                                   // Wenn kein Wochenden erkannt wurde und auch keine Ferien erkannt wurden, dann ist ein Unterrichtstag
    {
      Farbe = Green;                       // und Hintergrundbeleuchtung auf blau setzen für die Zeiten ausserhalb der Unterrichtszeiten
      FarbeSetzen();                       // Farbwahl übertragen
      Stundenplan();                       // Stundenplan prüfen, wenn aktuelle Zeit innerhalb einer Pause oder Unterrichtsstunde ist,
    }                                      // dann erfolgt die Ausgabe und das Setzen der Hintergrundbeleuchtung in der Stundenplan-Prüffunktion
    while (millis() - millisAlt < 1000)    // Während auf die nächste Sekunde gewartet wird schauen wir nach dem Drehknopf
    {
      ClickEncoder::Button b = encoder->getButton();  // Knopf Zustand holen
      if (b != ClickEncoder::Open)                    // Wurde der Knopf gedrückt?
      {
        Serial.println("Stellmodus an");                // Dann wechseln wir in den Stellmodus und sagen das auf dem Terminal
        StellModus=1;                                   // Stellmodus einschalten
        value = 0;                                      // Knopfwert auf 0
        FlashHelligkeit = 144;                          // Flash beginnt hell
        FlashRichtung = 1;                              // es folgt dunkel
        Farbe = White;                                  // hell einstellen
        FarbeSetzen();                                  // und anzeigen
        KorrekturTimeout = 30;                          // der hell/dunkel Wechsel nur 30 mal, danach wird aufgehört und der Stellmodus beendet sich 
      }    
    }  // Eine Sekunde warten bevor die Anzeige ausgegeben wird und die allgemeine Berechnung erneut erfolgt
    AusgabeAnzeige();// Ausgabe von aktueller Zeit oder Datum auf die Nixies, alle 20 Sekunden erfolgt 5 Sekunden lang die Anzeige von Datum, sonst Uhrzeit
  }
  else 
  {
    Serial.println();   // Damit auf dem Terminal die Anzeige der Uhrzeit und Datum richtig erscheint
    while (millis() - millisAlt < 1000)    {
    }        // Auch wenn Nixies ausgeschaltet sind wird eine Sekunde gewartet
  }

  if (Serial.available() > 0) EmpfangeSerial();  // Prüfen ob auf dem Terminal Eingaben gemacht wurden, dann serielle Schnittstelle auslesen und auswerten

  while(StellModus)                        // Ist der Stellmodus angemacht worden?
  {
    value -= encoder->getValue();          // Drehknopf Wert holen
    if (value != last)                     // Wurde ein neuer Wert eingestellt?
    {                    
      KorrekturTimeout = 30;               // Benutzer macht was, also Wartezeit von vorne beginnen lassen
      last = value;                        // letzten Wert für nächsten Vergleich merken
      if ((Minute + value) > 59)           // Minutenüberlauf nach oben
      {
        KorrekturMinute = value - 60;      // dann korrigieren wie eine Stunde und enstsprechend weniger Minuten
        KorrekturStunde = 1;               // eine Stunde korrigieren, weil an der Stundengrenzen Minuten addiert wurden
      }
      else if ((Minute + value) < 0)       // Minutenüberlauf nach unten
      {
        KorrekturMinute = value + 60;      // dann korrigieren wie eine Stunde und enstsprechend weniger Minuten
        KorrekturStunde = -1;              // eine Stunde korrigieren, weil an der Stundengrenzen Minuten addiert wurden
      }
      else
      {
        KorrekturMinute = value;           // ansonsten befindet sich die Korrektur innerhalb der darstellbaren Stunde
        KorrekturStunde = 0;               // und die Stunde bleibt ohne Koerrektur
      }
      printKorrekturClock();               // auf das Terminal ausgeben
      AusgabeKorrekturAnzeige();           // Und auf die Nixies auch
    }
    if(FlashRichtung)                      // dem Benutzer anzeigen, daß es sich nicht um die normalen Modus handelt
    {
      FlashHelligkeit--;                   // es ist hell und wir arbeiten auf das nächste Dunkel hin
      if(FlashHelligkeit == 0)             // ist hell vorbei?
      {
        FlashRichtung = 0;                 // dann Richtung umkehren 
        Farbe = Black;                     // dunkel einstellen 
        FarbeSetzen();                     // und anzeigen
        KorrekturTimeout--;                // macht der Benutzer nix?
      }
      delay(5);                            // nicht zu schnell
    }
    else                                   
    {
      FlashHelligkeit++;                   // es ist dunkel und wir arbeiten auf das nächste Hell hin
      if(FlashHelligkeit == 144)           // ist dunkel vorbei?
      {
        FlashRichtung = 1;                 // dann Richtung unkehren
        Farbe = White;                     // hell einstellen
        FarbeSetzen();                     // und anzeigen
        KorrekturTimeout--;                // macht der Benutzer nix?
      }
      delay(5);                            // nicht zu schnell
    }
    if(!KorrekturTimeout) StellModus = 0;  // hat der Benutzer lange Zeit nichts getan? dann Verstellmodus verlassen
    ClickEncoder::Button b = encoder->getButton();  // Beim Knopf nachschauen
    if (b != ClickEncoder::Open)                    // Wurde der Knopf als Bestätigung der Korrektur gedrückt?
    {
      I2CWriteKorrekturDate();                      // Dann schreiben wir die korrigierte Uhrzeit in die RTC
      Serial.println("Stellmodus aus");             // Dann zeigen wir an, dass der Verstellmodus beendet wurde
      StellModus=0;                                 // und beenden ihn auch tatsächlich
    }    
  }
}

void GongSchlagen(int Anzahl) // Gong abspielen, je nach Pause einmal oder zweimal
{
  if(Anzahl == Einfach)       // Wenn der Gong einmal abgespielt werden soll
  {
    digitalWrite(6, HIGH);                // Relais für internen Lautsprecher einschalten
    digitalWrite(7, HIGH);                // Relais für internen Lautsprecher einschalten
    Serial.print("Gong "); // Ausgabe auf das Terminal, daß der Gong einmal abgespielt werden wird
    MP3Serial.println();                  // Ausgabe an MP3 beginnen
    MP3Serial.println("\\:v 245");        // Volle Lautstärke einstellen (set the volume, from 0 (minimum)-255 (maximum))
    delay(50);                            // Warten auf MP3, daß Einstellung übernommen wird 
    MP3Serial.println("\\:n");            // MP3 Datei mit Gong abspielen
    GongAbwarten();                       // Während der Gong abspielt, die Sekundenlämpchen der Nixies weiter blinken lassen
    MP3Serial.println("\\:v 0");          // Lautstärke aus weil Gong vorbei - brummt trotzdem, spart aber evtl. Strom (set the volume, from 0 (minimum)-255 (maximum))
    delay(50);                            // Warten auf MP3, daß Einstellung übernommen wird
    digitalWrite(6, LOW);                 // Relais  für internen Lautsprecher wieder ausschalten um Brummen zu verhindern
    digitalWrite(7, LOW);                 // Relais  für externen Lautsprecher wieder ausschalten um Brummen zu verhindern
  }
  else                        // Wenn nicht einmal dann zweimal abspielen
  {
    digitalWrite(6, HIGH);                // Relais für internen Lautsprecher einschalten
    digitalWrite(7, HIGH);                // Relais für externen Lautsprecher einschalten
    Serial.print("Gong doppelt "); // Ausgabe auf das Terminal, daß der Gong einmal abgespielt werden wird
    MP3Serial.println();                  // Ausgabe an MP3 beginnen
    MP3Serial.println("\\:v 245");        // Volle Lautstärke einstellen (set the volume, from 0 (minimum)-255 (maximum))
    delay(50);                            // Warten auf MP3, daß Einstellung übernommen wird 
    MP3Serial.println("\\:n");            // MP3 Datei mit Gong das erste mal abspielen
    GongAbwarten();                       // Während der Gong abspielt, die Sekundenlämpchen der Nixies weiter blinken lassen 
    MP3Serial.println("\\:n");            // MP3 Datei mit Gong das zweite mal abspielen
    GongAbwarten();                       // Während der Gong abspielt, die Sekundenlämpchen der Nixies weiter blinken lassen
    MP3Serial.println("\\:v 0");          // Lautstärke aus weil Gong vorbei - brummt trotzdem, spart aber evtl. Strom (set the volume, from 0 (minimum)-255 (maximum))
    delay(50);                            // Warten auf MP3, daß Einstellung übernommen wird
    digitalWrite(6, LOW);                 // Relais  für internen Lautsprecher wieder ausschalten um Brummen zu verhindern
    digitalWrite(7, LOW);                 // Relais  für externen Lautsprecher wieder ausschalten um Brummen zu verhindern
  }
}

/* in dieser Reihenfolge liegen die Feriendaten in zwei Tabellen vor. Eine Tabelle für die Angabe des Tages und die Zweite für den Monat
 Winter		0,1
 Fasching	2,3
 Ostern		4,5
 1.Mai		6,7
 Himmelfahrt	8,9
 Pfingsten      10,11	
 Sommer	       12,13
 TDE            14,15
 Herbst         16,17 
 Winter         18,19
 */
byte Ferien(void)
{
  byte b; // Hilfsvariable

  // Erst werden die Daten für das aktuelle Jahr kopiert, damit die Prüfroutine unabhängig vom Jahr aufgebaut werden kann
  if (Jahr == 14)                      // 2014
  {
    for(byte i=0;i<20;i++)              // Tabellen mit 20 Einträge für Feriendaten von 10 Ferien pro Jahr
    {
      FerienMonat[i]=FerienMonat14[i]; // Kopieren der Monatstabelle
      FerienTag[i]=FerienTag14[i];     // Kopieren der Tagestabelle
    }
  }
  if (Jahr == 15)                      // 2015
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat15[i];
      FerienTag[i]=FerienTag15[i];
    }
  }
  if (Jahr == 16)                      // 2016
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat16[i];
      FerienTag[i]=FerienTag16[i];
    }
  }
  if (Jahr == 17)                      // 2017
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat17[i];
      FerienTag[i]=FerienTag17[i];
    }
  }
  if (Jahr == 18)                      // 2018
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat18[i];
      FerienTag[i]=FerienTag18[i];
    }
  }
  if (Jahr == 19)                      // 2019
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat19[i];
      FerienTag[i]=FerienTag19[i];
    }
  }
  if (Jahr == 20)                      // 2020
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat20[i];
      FerienTag[i]=FerienTag20[i];
    }
  }
  if (Jahr == 21)                      // 2021
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat21[i];
      FerienTag[i]=FerienTag21[i];
    }
  }
  if (Jahr == 22)                      // 2022
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat22[i];
      FerienTag[i]=FerienTag22[i];
    }
  }
  if (Jahr == 23)                      // 2023
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat23[i];
      FerienTag[i]=FerienTag23[i];
    }
  }

  // Auswertung der Ferientabelle, es wird nach vergangenen und ausstehenden Daten gesucht
  // 1 = vergangen, 0 = ausstehend
  // Es wird eine Tabelle mit dem Ergebnis angelegt und anschliessen nach dem Übergang von 1 nach 0 gesucht
  // Dieser Übergang zeigt die Stelle in der Ferientabelle an welche dem aktuellen Datum entspricht
  // Anhand dieser Stelle kann ermittelt werden ob das aktuelle Datum innerhalb der Ferien liegt oder nicht
  // und wenn innerhalb der Ferien um welche Ferien es sich handelt
  for(byte i=0;i<20;i++)        // Tabellen mit Feriendaten nach aktuellem Datum absuchen. 
  {
    if(Monat > FerienMonat[i]) // Monat in Tabelle schon vorbei? Dann muss der Tag nicht geprüft werden
    {
      Auswertung[i] = 1;  // Dann in Auswertetabelle mit 1 merken
    }
    else if(i%2 && Monat == FerienMonat[i] && Tag > FerienTag[i]) // Der Tag für den Ferienanfang ist immer an geradadzahligen Stellen in der Tabelle und wird ausgeschlossen
    {                                                             // Wenn der Monat noch nicht vorbei ist, dann prüfen ob der Monat in der Tabelle der aktuelle Monat ist und Tag prüfen
      Auswertung[i] = 1;                                    // Dann mit 1 merken
    }
    else if(!(i%2) && Monat == FerienMonat[i] && Tag >= FerienTag[i]) // Der Tag für das Ferienende ist immer an ungeradadzahligen Stellen in der Tabelle und wird mit berücksichtigt
    {                                                                 // Wenn der Monat noch nicht vorbei ist, dann prüfen ob der Monat in der Tabelle der aktuelle Monat ist und Tag prüfen
      Auswertung[i] = 1;                                        // Dann mit 1 merken 
    }
    else                                             // wenn beides nicht der Fall ist, dann ist es ein noch ausstehendes Datum
    {
      Auswertung[i] = 0;                       // das dann mit 0 merken                 
    }
    b=i-1; // Vorherige Stelle in Auswertetabelle
    if (i && (Auswertung[i] < Auswertung[b])) // Gab es in der Tabelle im Vergleich zur alten Stelle einen Übergang von 1 nach 0?
    {                                                     // Wenn ja, dann wurde die Stelle gefunden und jetzt wird geprüft ob es sich um eine Stelle innerhalb der Ferien handelt und um welche
      if (b==0)                                           // An der Stelle 0 = 1 und 1 = 0 handelt es sich um die Winterferien 2. Teil
      {
        Serial.println("Winterf.");          // Prüfung beenden und Ergebnis ausgeben
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==2)                                           // An der Stelle 2 = 1 und 3 = 0 handelt es sich um die Faschingsferien 
      {
        Serial.println("Fasching");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==4)                                           // An der Stelle 4 = 1 und 5 = 0 handelt es sich um die Osterferien
      {
        Serial.println("Ostern");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==6)                                           // An der Stelle 6 = 1 und 7 = 0 handelt es sich um den 1. Mai
      {
        Serial.println("1. Mai");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==8)                                           // An der Stelle 8 = 1 und 9 = 0 handelt es sich um Himmelfahrt
      {
        Serial.println("Himmelfahrt");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==10)                                          // An der Stelle 10 = 1 und 11 = 0 handelt es sich um die Pfingstferien  
      {
        Serial.println("Pfingsten");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==12)                                          // An der Stelle 12 = 1 und 13 = 0 handelt es sich um die Sommerferien  
      {
        Serial.println("Sommerf.");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==14)                                          // An der Stelle 14 = 1 und 15 = 0 handelt es sich um den Tag der deutschen Einheit
      {
        Serial.println("TdE");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==16)                                          // An der Stelle 16 = 1 und 17 = 0 handelt es sich um die Herbstsferien  
      {
        Serial.println("Herbstf.");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==18)                                          // An der Stelle 18 = 1 und 19 = 0 handelt es sich um die Winterferiensferien 1. Teil
      {
        Serial.println("Winterf.");
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
    }
  }
  return 0; // Aktuelles Datum war NICHT innerhalb der Ferien, damit Rückgabewert 0 = keine Ferien
}


/* in dieser Reihenfolge liegen die Unterrichtszeiten in zwei Tabellen vor. Eine Tabelle für die Angabe des Stunde und die Zweite für die Minuten
 7:55			         0   
 8:00 –  9:40   Hauptunterricht	 1,2
 9:55				 3
 10:00 – 10:45   1. Fachstunde   4,5
 10:50 – 11:35   2. Fachstunde   6,7
 11:45 – 12:30   3. Fachstunde   8,9
 12:35 – 13:20   4. Fachstunde  10,11
 13:25 – 14:10   5. Fachstunde  12,13
 14:15 – 15:00   6. Fachstunde  14,15
 15:05 – 15:50   7. Fachstunde  16,16
 15:55 – 16:40   8. Fachstunde  18,19
 */
void Stundenplan(void)
{
  byte b,c,UnterrichtGefunden = 0; // Hilfsvariablen

  // Auswertung der Unterrichtszeiten, es wird nach vergangener und ausstehender Uhrzeit gesucht
  // 1 = vergangen, 0 = ausstehend
  // Es wird eine Tabelle mit dem Ergebnis angelegt und anschliessen nach dem Übergang von 1 nach 0 gesucht
  // Dieser Übergang zeigt die Stelle in der Unterrichstzeiten-Tabelle an welche der aktuellen Uhrzeit entspricht
  // Anhand dieser Stelle kann ermittelt innerhalb wovon die Uhrzeit liegt
  for(byte i=0;i<20;i++)        // Tabellen mit Unterrichtszeiten nach aktueller Uhrzeit absuchen. 
  {
    if(Stunde > StundeX[i]) // Stunde in Tabelle schon vorbei? Dann muss die Minute nicht geprüft werden
    {
      Auswertung[i] = 1;    // Dann in Auswertetabelle mit 1 merken
    }
    else if(Stunde == StundeX[i] && Minute >= MinuteX[i]) // Wenn Stunde noch nicht vorbei dann prüfen ob es die aktuelle Sunde ist und ob die Minute schon vorbei ist
      // oder es die aktuelle Minute aus der Tabelle ist
    { 
      Auswertung[i] = 1;    // Dann in Auswertetabelle mit 1 merken
      if(Minute == MinuteX[i] && Sekunde < 10)            // Wenn es die aktuelle Minute aus der Tabelle ist
      {
        if(i==1 || i==4) GongSchlagen(Doppelt); 
        else GongSchlagen(Einfach); // Dann den Gong abspielen lassen. 
        // Zwei Ausnahmen für Ankündigungen: doppelter Gong sonst Einfacher
        // 1 Ankündigung zum Hauptunterricht 5 Minuten vor Schulbeginn
        // 4 Ankündigung zur 1. Fachstunde während der großen Pause 5 Minuten vorher
      }
    }
    else Auswertung[i] = 0; // Wenn die Zeit in der Tabelle nicht schon vorbei ist oder sie nicht genau entspricht dann ist sie noch ausstehend, dann mit 0 merken

    b=i-1;  // Vorherige Stelle in Auswertetabelle
    if (i && (Auswertung[i] < Auswertung[b])) // Gab es in der Tabelle im Vergleich zur alten Stelle einen Übergang von 1 nach 0?
      // Wenn ja, dann wurde die Stelle gefunden und jetzt wird geprüft welche Stelle das ist
    {
      c=((b-4)/2+1);                          // Errechnen des Zählers der Pausen und Fachsstunden
      UnterrichtGefunden = 1;
      if (b==0)                               // An der Stelle 0 = 1 und 1 = 0 handelt es sich um die Ankündigung zum Hauptunterricht
      {
        Serial.println("Ank. HU"); // String für die Ausgabe auf Terminal
        Farbe = Green;                                              // grüne Farbe der Hintergrundbeleuchtung für Pause wählen
        FarbeSetzen();                                              // Farbauswahl an Nixies übermitteln 
      }
      if (b==1)                               // An der Stelle 1 = 1 und 2 = 0 handelt es sich um den Hauptunterricht
      {
        Serial.println("HU"); // String für die Ausgabe auf Terminal
        Farbe = Magenta;                      // gelbe Farbe der Hintergrundbeleuchtung für Unterricht wählen 
        FarbeSetzen();                        // Farbauswahl an Nixies übermitteln
      }
      if (b==2)                               // An der Stelle 2 = 1 und 3 = 0 handelt es sich um die grosse Pause
      {
        Serial.println("Gr. P."); // String für die Ausgabe auf Terminal
        Farbe = Green;                        // grüne Farbe der Hintergrundbeleuchtung für Pause wählen
        FarbeSetzen();                        // Farbauswahl an Nixies übermitteln
      }
      if (b==3)                               // An der Stelle 3 = 1 und 4 = 0 handelt es sich um die Ankündigung zur 1. Fachstunde während der grossen Pause
      {
        Serial.println("Ank. 1. FS"); // String für die Ausgabe auf Terminal
        Farbe = Green;                        // grüne Farbe der Hintergrundbeleuchtung für Pause wählen
        FarbeSetzen();                        // Farbauswahl an Nixies übermitteln
      }
      if (b>3 && b%2)                         // An den geradzahligen Stellen größer 3 handelt es sich um 'c.' Pausen 1-7
      {
        Serial.print(int(c));
        Serial.println(".Pause");           // String für die Ausgabe auf Terminal
        Farbe = Green;                       // grüne Farbe der Hintergrundbeleuchtung für Pause wählen
        FarbeSetzen();                       // Farbauswahl an Nixies übermitteln
      }
      if (b>3 && !(b%2))                     // An den ungeradzahligen Stellen größer 4 handelt es sich um 'c.' Fachstunden 1-8
      {
        Serial.print(int(c));
        Serial.println( ".FS");     // String für die Ausgabe auf Terminal
        Farbe = Magenta;                     // gelbe Farbe der Hintergrundbeleuchtung für Unterricht wählen    
        FarbeSetzen();                       // Farbauswahl an Nixies übermitteln
      }
    }
  }
  if (!UnterrichtGefunden) Serial.println(); // Um die Ausgabe im Terminal zu bereinigen
}


void AusgabeAnzeige(void)
{
  if ((Sekunde==10) || (Sekunde==30) || (Sekunde==50)) // alle 20 Sekunden wechselt die Anzeige der Uhrzeit auf die Anzeige des Datums mit weisser Hintergrundbeleuchtung
  {
    Farbe = White;                                     // Farbe auf weiss setzen 
    tube.printf("%02d'%02d'", Tag, Monat);  //Datum    // Anzeige des Datums an Nixies senden
    FarbeSetzen();                                     // gewählte Farbe an Nixies senden 
    delay(5000);                                       // 5 Sekunden lang das Datum anzeigen
  }
  Blinken();
  tube.display();                                      // Gesendete Daten in den Nixies zur Anzeige bringen
}

void AusgabeKorrekturAnzeige(void)
{
  tube.printf("%02d%02d", Stunde+KorrekturStunde, Minute+KorrekturMinute); // Anzeige der korrigierten Zeit an Nixies senden
  tube.display();                                      // Gesendete Daten in den Nixies zur Anzeige bringen
}

void Blinken(void)                                     // Sekundendoppelpunkt jeweils an oder ausschalten, je nachdem ob er aus oder an ist
{
  if(Blink) Blink=0; 
  else Blink = 1;                                     // Wechsel des Sekundendoppelpunktes
  if(Blink)                                            // Je nachdem ob eine Sekunde mit oder ohne Doppeltpunkt aktuell ist
  {
    tube.printf("%02d:%02d", Stunde, Minute);          // wird die Uhrzeit mit dem Doppelpunkt an die Nixies gesendet
  }
  else                                                 // oder   
  {
    tube.printf("%02d%02d", Stunde, Minute);           // es wird die Uhrzeit ohne den Doppelpunkt an die Nixies gesendet
  }
}

void GongAbwarten(void)                            // Während der Gong abgespielt wird, das dauert ca. 20 Sekunden, soll der Sekundendoppelpunkt weiter blinken
{
  AlteFarbe=Farbe;                                 // Alte Farbe merken, damit nach dem Blinken der Hintergrundbeleuchtung wieder die alte Farbe angezeigt wird
  for(byte i=0;i<15;i++)                            // 20 Durchläufe mit einer Wartezeit von je einer Sekunde damit die Wartezeit von 20 Sekunden erreicht wird
  {
    delay(1000);                                   // eine Sekunde warten
    Blinken();                                     // Sekundendoppelpunkt wechseln
    if(Blink) Farbe = Blue; 
    else Farbe = Magenta;                          // Die Hintergrundfarbe abwechselnd grün oder gelb setzen
    FarbeSetzen();                                 // Gewählte Farbe an Nixies senden
  }
  Farbe = AlteFarbe;                               // nach Ablauf der 20 Durchgänge wieder auf die vorherige Farbe wechseln für die Zeit bis sie neu berechnet wird
}

void FarbeSetzen(void)               // gewählte Farbe für die Hintergrundbeleuchtung an alle 4 Nixies schicken und zur Anzeige bringen
{
  tube.setBackgroundColor(1, Farbe); // Farbe für Nixie an der 1. Stelle an Nixie schicken
  tube.setBackgroundColor(2, Farbe); // Farbe für Nixie an der 2. Stelle an Nixie schicken
  tube.setBackgroundColor(3, Farbe); // Farbe für Nixie an der 3. Stelle an Nixie schicken
  tube.setBackgroundColor(4, Farbe); // Farbe für Nixie an der 4. Stelle an Nixie schicken
  tube.display();                    // Neue Farbe der Nixies zur Anzeige bringen
}

void printClock(void)                  // Ausgabe der Zeit und Datum vom ELV-CDF77 auf dem Terminal und Kopieren der Daten in globale Variablen
{
  sprintf(clockString, "%02u:%02u:%02u %02u.%02u.%02u ", Stunde, Minute, Sekunde, Tag, Monat, Jahr);
  if (Wochentag == 0) Serial.print("Mo "); // Wochentag auswählen
  if (Wochentag == 1) Serial.print("Di ");
  if (Wochentag == 2) Serial.print("Mi ");
  if (Wochentag == 3) Serial.print("Do ");
  if (Wochentag == 4) Serial.print("Fr ");
  if (Wochentag == 5) Serial.print("Sa ");
  if (Wochentag == 6) Serial.print("So ");
  Serial.print(clockString);   // Uhrzeit und Datum und Wochentag auf Terminal schreiben
}

void printKorrekturClock(void)                  // Ausgabe der Zeit und Datum vom ELV-CDF77 auf dem Terminal und Kopieren der Daten in globale Variablen
{
  sprintf(clockString, "%02u:%02u", Stunde+KorrekturStunde, Minute+KorrekturMinute);
  Serial.print(clockString);   // korrigierte Uhrzeit und Datum und Wochentag auf Terminal schreiben
  Serial.print(" ");Serial.print(KorrekturStunde);Serial.print(":");Serial.println(KorrekturMinute); // und Korrekturwert auch ausgeben
}


int NixieLD(void)
{
  if (Stunde == 1 && Minute == 0) // Um 01:00 Uhr werden die Nixies 5 Minuten lang gereinigt indem sie bei voller Helligkeit schnell durchwechseln damit sich alle Anzeigegitter erwärmen
    // Mit dieser Erwärmung werden Ablagerungen auf den Gittern verdampft
  {
    Serial.println("Wartung");
    tube.setBrightness(0xff);	   // Helligkeit der Nixies auf 100% um einen Reinigungseffekt zu erreichen
    tube.display();                // Daten an Nixies übertragen
    for (int i = 0; i < 600; i++)  // 5 Minuten * 60 Sekunden * 10 Anzeigen, Jede Anzeige leuchtet 100 ms, also ein Durchlauf aller Anzeigen dauert eine Sekunde
    {
      for (byte n = 0; n < 10; n++)       // Die Zahlengitter durchzählen
      {
        tube.printf("%02d%02d", (n*10+n), (n*10+n));  // Jedes Zahlengitter nacheinander einschalten 
        tube.display();                  // Gesendete Daten in den Nixies zur Anzeige bringen
        delay(50);                      // Und für eine zehntel Sekunde leuchten lassen
      }
    }
    tube.setBrightness(0x00);	   // Helligkeit der Nixies auf 0% um sie während der Nachtstunden auszuschalten
    tube.display();                // Daten an Nixies übertragen
  }
  if (Stunde < 7) // zwischen 00:00 und 06:59 Nixies auschalten
  {
    tube.setBrightness(0x00);	  // Helligkeit der Nixies auf 0% um sie während der Nachtstunden auszuschalten
    tube.display();               // Daten an Nixies übertragen
    return 0;                     // Nixies ausgeschaltet
  }
  else
  {
    tube.setBrightness(0x90);	  // Helligkeit der Nixies auf ca. 30% um Lebensdauer zu erhöhen (brightness control, 0x00(off)-0xff)
    tube.display();               // Daten an Nixies übertragen
    return 1;                     // Nixies eingeschaltet
  }
}

void EmpfangeSerial(void)               // Eingabe vom Terminale einlesen und auswerten
{
  byte Buffer[8],Rest[56];               // Tabellen für die Eingaben
  byte AnzahlBytes = Serial.available(); // Anzahl der Bytes im Buffer der seriellen Schnittstellen ermitteln
  Serial.println("Daten empfangen");
  for (byte i = 0; i < AnzahlBytes; i++) // Jedes Byte einzeln abholen
  {
    if (i<8)                            // Für die Auswertung werden nur die ersten 8 Byte verwendet 
    {
      Buffer[i] = Serial.read()-48;     // einzelnes Byte von der seriellen Schnittstelle holen
    }
    else 
    {
      Rest[i] = Serial.read();          // evtl. weiter vorhandene Bytes von der seriellen Schnittstelle holen, werden nicht weiter verwendet, nur um den Buffer zu leeren
    }
  }
  if (Buffer[2] == 254)                  // In der seriellen Eingabe wurde an der dritten Stelle ein Punkt erkannt, also handelt es sich um ein Datum z.B.: 01.01.90
  {
    Tag = Buffer[0] * 10 + Buffer[1];
    Monat = Buffer[3] * 10 + Buffer[4];
    Jahr = Buffer[6] * 10 + Buffer[7];
    KorrekturMinute = 0;
    KorrekturStunde = 0;
    I2CWriteKorrekturDate();
  }
  if (Buffer[2] == 10)                  // In der seriellen Eingabe wurde an der dritten Stelle ein Doppelpunkt erkannt, also handelt es sich um eine Uhrzeit z.B.: 07:54.50
  {
    Stunde = Buffer[0] * 10 + Buffer[1];
    Minute = Buffer[3] * 10 + Buffer[4];
    Sekunde = Buffer[6] * 10 + Buffer[7];
    KorrekturMinute = 0;
    KorrekturStunde = 0;
    I2CWriteKorrekturDate();
  }
  if (Buffer[0] == 10 && Buffer[1] == 10) // Debugsession aktivieren oder deaktivieren
  {
    if(DebugAktiv) DebugAktiv = 0; 
    else DebugAktiv = 1;
    Serial.print("Debug=");
    Serial.println(int(DebugAktiv));
  }
  else if (Buffer[0] == 10)                  // In der seriellen Eingabe wurde an der ersten Stelle ein Doppelpunkt erkannt, also handelt es sich um einen Wochentag
  {
    Wochentag = Buffer[1];
    KorrekturMinute = 0;
    KorrekturStunde = 0;
    I2CWriteKorrekturDate();
  }

}

//Read the Real-time data register of SD2403
void I2CReadDate(void)
{
  unsigned char n=0;
  Wire.requestFrom(RTC_Address,7);
  while(Wire.available())
  { 
    date[n++]=Wire.read();
  }
  delayMicroseconds(1);
  Wire.endTransmission();
}
 
//Write the Real-time data register of SD2403
void I2CWriteDate(void)
{      
  WriteTimeOn();
  Wire.beginTransmission(RTC_Address);       
  Wire.write(byte(0));//Set the address for writing      
  Wire.write(0x40);//second:
  Wire.write(0x30);//minute:
  Wire.write(0x10 + 0x80);//hour:+0x80 für 24h statt 12h am/pm
  Wire.write(0x06);//weekday:0=Montag
  Wire.write(0x19);//day:
  Wire.write(0x10);//month:
  Wire.write(0x14);//year:
  Wire.endTransmission();
  Wire.beginTransmission(RTC_Address);     
  Wire.write(0x12);   //Set the address for writing        
  Wire.write(byte(0));           
  Wire.endTransmission();
  WriteTimeOff();     
}

void I2CWriteKorrekturDate(void)
{      
  if (Sommerzeit()) Stunde--;
  WriteTimeOn();
  Wire.beginTransmission(RTC_Address);       
  Wire.write(byte(0));//Set the address for writing      
  Wire.write(((Sekunde/10)<<4)+(Sekunde%10));//second:59    
  Wire.write((((Minute+KorrekturMinute)/10)<<4) + ((Minute+KorrekturMinute)%10));//minute:1     
  Wire.write((((Stunde+KorrekturStunde)/10)<<4) + ((Stunde+KorrekturStunde)%10) + 0x80);//hour:15:00(24-hour format)        
  Wire.write(Wochentag);//weekday:Wednesday     
  Wire.write(((Tag/10)<<4)+(Tag%10));//day:26th     
  Wire.write(((Monat/10)<<4)+(Monat%10));//month:December    
  Wire.write(((Jahr/10)<<4)+(Jahr%10));//year:2012     
  Wire.endTransmission();
  Wire.beginTransmission(RTC_Address);     
  Wire.write(0x12);   //Set the address for writing        
  Wire.write(byte(0));           
  Wire.endTransmission();
  WriteTimeOff();     
}
 
//The program for allowing to write to SD2400
void WriteTimeOn(void)
{      
  Wire.beginTransmission(RTC_Address);      
  Wire.write(0x10);//Set the address for writing as 10H        
  Wire.write(0x80);//Set WRTC1=1     
  Wire.endTransmission();
 
  Wire.beginTransmission(RTC_Address);   
  Wire.write(0x0F);//Set the address for writing as OFH        
  Wire.write(0x84);//Set WRTC2=1,WRTC3=1     
  Wire.endTransmission();  
}
 
//The program for forbidding writing to SD2400
void WriteTimeOff(void)
{      
  Wire.beginTransmission(RTC_Address);  
  Wire.write(0x0F);   //Set the address for writing as OFH         
  Wire.write(byte(0));//Set WRTC2=0,WRTC3=0     
  Wire.write(byte(0));//Set WRTC1=0 
  Wire.endTransmission();
}
 
//Process the time_data
void Data_process(void)
{
  unsigned char i;
  for(i=0;i<7;i++)
  {
    if(i!=2)
      date[i]=(((date[i]&0xf0)>>4)*10)+(date[i]&0x0f);
    else
    {
      date[2]=(date[2]&0x7f);
      date[2]=(((date[2]&0xf0)>>4)*10)+(date[2]&0x0f);
      //((in/10)<<4)+(in%10)
    }
  }
  Wochentag = date[3];   // Wochentag aus dem Ergebnis von DS2405 holen
  if (Sommerzeit()) Stunde = date[2] + 1; else Stunde = date[2];         // Stunde aus dem Ergebnis von DS2405 holen
  Minute = date[1];      // Minute aus dem Ergebnis von DS2405 holen
  Sekunde = date[0];     // Sekunde aus dem Ergebnis von DS2405 holen
  Jahr = date[6];        // Jahr aus dem Ergebnis von DS2405 holen
  Monat = date[5];       // Monat aus dem Ergebnis von DS2405 holen
  Tag = date[4];         // Tag aus dem Ergebnis von DS2405 holen
}

boolean Sommerzeit(void)
{
  if ((date[5] < 3) || (date[5] > 10)) return 0; // Wenn nicht innerhalb der Sommerzeitmonate, dann gleich abbrechen
  if ( ((date[4] - date[3]) >= 25) && (date[3] || (date[2] >= 2)) ) // Wenn nach dem letzten Sonntag und nach 2:00 Uhr
  {
    if (date[5] == 10 ) return 0;                                   // dann wenn Oktober, dann gleich abbrechen
  }
  else                                                              // Wenn nicht nach dem letzten Sonntag bzw. vor 2:00 Uhr 
  {
    if (date[5] == 3) return 0;                                     // und es ist März, dann gleich abbrechen
  }
  return 1;        // ansonsten innerhalb der SZMonate, nach dem letzten Sonntag und nicht Oktober, vor dem letzten Sonntag und nicht März, dann ist SZ           
}


