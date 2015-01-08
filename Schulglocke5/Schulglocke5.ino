/* 
 Programm für die Schulglocke der Waldorfschule Geislingen 
 Lars Danielis
 Version 1.0 Erster Aufbau mit RTC von dfrobot ohne DCF77
 Version 2.0 Geänderter Aufbau mit RTC-DCF77 von ELV für genaue Atum-Uhrzeit per Funk
 Version 3.0 Code aufgeräumt und dokumentiert sowie um Kathodenreinigungsfunktion und Abschaltung in der Nacht ergänzt
 Version 4.0 DCF77 liefert manchmal ungültige Zeit, diese werden erkannt und es wird neu empfangen
 
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

/*
    Debugmöglichkeit über serielle Schnittstelle
 Eingabe von z.B. 12:00:00 + Enter setzt die Zeit, es wird der : detektiert
 Eingabe von z.B. 01.01.14 + Enter setzt das Datum, es wird der . detektiert
 Eingabe von z.B. :0 setzt den Wochentag beginnend bei 0 mit Montag
 Eingabe von :: aktiviert den Debugmodus um Schaltzeiten zu prüfen ohne die Plausibilisierung zu aktivieren
 */

#include <SoftwareSerial.h> // für die Ansteuerung des MP3 Players (Rx/Tx Pin 4 und 5) ohne Einfluss auf das Terminal (Rx/Tx Pin 0 und 1)
#include "NixieTube.h"      // Bibliothek der Nixie Tube QS30-1 V2.0.0
#include <Wire.h>           // Ansteuerung der Nixie über SPI Pin 10-13
#include <stdio.h>          // Terminal und Pins
#include "DateTime.h"       // ELV Bibliothek für Datums- und Uhrzeitoperationen
#include "RealTimeClock_DCF.h" // ELV Bibliothek für die Ansteuerung der RTC-DCF77 per I2C (I2C SDA/SCL Pin A4/A5)

SoftwareSerial MP3Serial(4,5); //Serieller Port für MP3

DateTime dateTime = DateTime(14, 1, 1, DateTime::MONDAY, 23, 30, 0); // Jahr,Monat,Tag,Wochentag,Stunde,Minute,Sekunde
// im Setup muss das Setzen der Daten auskommentiert werden

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
const byte FerienTag19[20]    = { 1, 6, 4, 8,15,26, 1, 2,30,31,11,21,29,10, 3, 4, 1, 2,21,31}; // Feriendaten für 2019 Tag
const byte FerienMonat19[20]  = { 1, 1, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 9,10,10,11,11,12,12}; // Feriendaten für 2019 Monat
//------------------------------------------------------------------------------------------------------------------------------

byte FerienTag[20],FerienMonat[20];             // Kopie der Feriendaten des aktuellen Jahres
byte Auswertung[20];                            // Hilfstabelle für die Auswertung der Unterrichtszeiten und der Ermittlung was aktuell ist, 
// Unterricht oder Pause und welcher Unterricht und welche Pause
// byte AuswertungFerien[20];                      // Hilfstabelle für die Auswertung der Ferien und der Ermittlung ob aktuell Ferien sind 
byte DebugAktiv;                                // Für Testzwecke, z.B. die Plausibilisierung umgehen
byte unplausibel = 99;                          // Letzter Zeitstempel war nicht plausibel, alten Wert behalten
byte ErsterEmpfangIO = 0;                       // warten auf den ersten erfolgreichen Empfang - dem ELV-RTC ist nicht zu trauen

#define Einfach 0 //Gongart
#define Doppelt 1 //Gongart

#define COUNT	4 // 4 Nixie Module vorhanden
NixieTube tube(11, 12, 13, 10, COUNT); // pin_ds, pin_st. pin_sh, pin_oe(pwm pin is preferred), COUNT

void setup()
{
  delay(1500);     // Warten auf DCF77
  RTC_DCF.begin(); // Kommunikation zu ELV-DCF77 initialiseren
  RTC_DCF.setPeriodicInterruptMode(RTC_PERIODIC_INT_PULSE_1_HZ); // Blinkfrequenz der roten LED auf ELF-DCF77, blinkt während EMpfang
  RTC_DCF.setDateTime(&dateTime); //Uhrzeit setzen
  RTC_DCF.enableDCF77Reception();       // DCF77-Empfang aktivieren
  RTC_DCF.enablePeriodicInterrupt();    // den periodischen Interrupt des RTC-DCF aktivieren um die rote LED auf dem ELV-DCF blinken zu lassen
  RTC_DCF.enablePeriodicInterruptLED(); // LED auf dem ELV-DCF einschalten und im Rhythmus von 1 Sekunde blinken lassen
  DebugAktiv=0;     // DebugSession inaktiv
  RTC_DCF.getDateTime(&dateTime);
  ZeitKopieren();

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

}

void loop()
{
  millisAlt=millis();
  DCF77();  // Und wenn das Datum oder Uhrzeit schlecht empfangen wurden wird unplausibel gemeldet und die alte Zeit weiter verwendet
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
    while (millis() - millisAlt < 1000){
    }  // Eine Sekunde warten bevor die Anzeige ausgegeben wird und die allgemeine Berechnung erneut erfolgt
    AusgabeAnzeige();// Ausgabe von aktueller Zeit oder Datum auf die Nixies, alle 20 Sekunden erfolgt 5 Sekunden lang die Anzeige von Datum, sonst Uhrzeit
  }
  else 
  {
    Serial.println();   // Damit auf dem Terminal die Anzeige der Uhrzeit und Datum richtig erscheint
    while (millis() - millisAlt < 1000){
    }        // Auch wenn Nixies ausgeschaltet sind wird eine Sekunde gewartet
  }
  if (Serial.available() > 0) EmpfangeSerial();  // Prüfen ob auf dem Terminal Eingaben gemacht wurden, dann serielle Schnittstelle auslesen und auswerten
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
    delay(900);                                       // 5 Sekunden lang das Datum anzeigen
    ZeitManuell();         
    delay(900);               
    ZeitManuell();         
    delay(900);               
    ZeitManuell();         
    delay(900);               
    ZeitManuell();         
    delay(900);    
    if(SekundenSeitEmpfang) SekundenSeitEmpfang = SekundenSeitEmpfang + 5;
  }
  Blinken();
  tube.display();                                      // Gesendete Daten in den Nixies zur Anzeige bringen
}

void Blinken(void)                                     // Sekundendoppelpunkt jeweils an oder ausschalten, je nachdem ob er aus oder an ist
{
  if(Blink) Blink=0; 
  else Blink = 1;                   // Wechsel des Sekundendoppelpunktes
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
    else Farbe = Magenta;  // Die Hintergrundfarbe abwechselnd grün oder gelb setzen
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

void readRegister(RTCRegister_t regAddress, uint8_t length, uint8_t *data)  // Status von ELV-DCF holen um zu erkennen ob der Empfang der Uhrzeit und des Datums erfolgreich waren
{
  // set the register pointer to the given position, but do not stop the communication
  // after writing the pointer
  Wire.beginTransmission(RTC_DCF_I2C_ADDRESS);
  Wire.write(regAddress);
  Wire.endTransmission(false); 
  // read out the given amount of data from the registers
  Wire.requestFrom((uint8_t)RTC_DCF_I2C_ADDRESS, length);
  while( Wire.available() )
  {
    *data = Wire.read();
    data++;
  }    
}

void writeRegister(RTCRegister_t regAddress, uint8_t *data, uint8_t datalength)  // Register von ELV-DCF beschreiben um den Empfang zu aktivieren und die rote LED auf dem ELV-DCF zu aktivieren 
{
  Wire.beginTransmission(RTC_DCF_I2C_ADDRESS);
  Wire.write(regAddress);
  while(datalength--)
  {
    Wire.write(*data);
    data++;
  }
  Wire.endTransmission();
}


void DCF77(void)                         // Prüfung ob Datum und Uhrzeit gültig sind 
{
  uint8_t dcf77Register,dcf77Reg;        // Variable mit den Daten des ELV-DCF Registers welches mit readRegister ausgelesen wurde
  RTC_DCF.getDateTime(&dateTime);
  SekundenSeitEmpfang++;                                          // Sekunden für die Empfangsdauer erhöhen
  readRegister(RTC_DCF_REG_STATUS, 1, &dcf77Register);            // beim ELV-DCF das Statusregister auslesen    
  dcf77Reg = dcf77Register & 0x01;                                // aus dem Register das unterste Bit ausmaskieren
  if (dcf77Reg)                                                   // ist das ausmaskierte Bit = 1 dann bedeutet das, daß der Empfang der Uhrzeit vom DCF77 erfolgreich war 
  {
    writeRegister(RTC_DCF_REG_STATUS, &dcf77Reg, 1);              // Das Bit beschreiben damit es vom ELV-DCF selbständig gelöscht wird, als Lesebestätigung
    SekundenSeitEmpfang = 0;                                      // Für die Ausgabe der Empfangsdauer in Sekunden
    unplausibel = 0;
    // Sind folgende Daten im gültigen Bereich? Jahr innerhalb 2014 und 2020, Monat kleiner 13, Tag kleiner 24, Minute kleiner 60 ?
    if ( (dateTime.getYear() < 14) || (dateTime.getYear() > 20) || (dateTime.getMonth() > 12) || (dateTime.getDay() > 31) || (dateTime.getHour() > 23) || (dateTime.getMinute() > 59) || (dateTime.getWeekday() > 6) )
    {
      unplausibel = 16;
    }
    else if (ErsterEmpfangIO) 
    {
      if (Wochentag == 6)
      { 
        if (!(dateTime.getWeekday() == 6 || dateTime.getWeekday() == 0)) unplausibel = 1;
      }
      else if(!(dateTime.getWeekday() == Wochentag || dateTime.getWeekday() == Wochentag + 1)) unplausibel = 2;
  
      if (!(dateTime.getYear() == Jahr || dateTime.getYear() == Jahr + 1)) unplausibel = 3;
  
      if (Monat == 12)
      {
        if (!(dateTime.getMonth() == 12 || dateTime.getMonth() == 12)) unplausibel = 4;
      }
      else if(!(dateTime.getMonth() == Monat || dateTime.getMonth() == Monat + 1)) unplausibel = 5;
  
      if (Tag > 27)
      {
        if ((Tag == 28) && (!(dateTime.getDay() == 28 || dateTime.getDay() == 0))) unplausibel = 6;
        if ((Tag == 29) && (!(dateTime.getDay() == 29 || dateTime.getDay() == 0))) unplausibel = 7;
        if ((Tag == 30) && (!(dateTime.getDay() == 30 || dateTime.getDay() == 0))) unplausibel = 8;
        if ((Tag == 31) && (!(dateTime.getDay() == 31 || dateTime.getDay() == 0))) unplausibel = 9;
      }
      else if (!(dateTime.getDay() == Tag || dateTime.getDay() == Tag + 1)) unplausibel = 10;
  
      if (Stunde == 23)
      {
        if (!(dateTime.getHour() == 23 || dateTime.getHour() == 0)) unplausibel = 11;
      }
      else if(!(dateTime.getHour() == Stunde || dateTime.getHour() == Stunde + 1 || dateTime.getHour() == Stunde + 2 || dateTime.getHour() == Stunde - 1)) unplausibel = 12;
  
      if ((Minute + 5) > 59)
      {
        if (!(dateTime.getMinute() > (Minute - 5) || dateTime.getMinute() < (Minute - 54)))  unplausibel = 13;
      }
      else if ((Minute - 5) < 0) 
      {
        if (!(dateTime.getMinute() > (Minute + 54) || dateTime.getMinute() < (Minute + 5)))  unplausibel = 14;
      }
      else if (!(dateTime.getMinute() > (Minute - 5) && dateTime.getMinute() < (Minute +5))) unplausibel = 15;
    }
  }

  if(!unplausibel || DebugAktiv)
  {
    ZeitKopieren();
    ErsterEmpfangIO=1;
  }
  else
  {
    Serial.print("unplausibel:");
    Serial.print(int(unplausibel));
    Serial.print(" ");
    sprintf(clockString, "%02u:%02u:%02u %02u.%02u.%02u ", dateTime.getHour(), dateTime.getMinute(), dateTime.getSecond(), dateTime.getDay(), dateTime.getMonth(), dateTime.getYear());
    if (dateTime.getWeekday() == 0) Serial.print("Mo "); // Wochentag auswählen
    if (dateTime.getWeekday() == 1) Serial.print("Di ");
    if (dateTime.getWeekday() == 2) Serial.print("Mi ");
    if (dateTime.getWeekday() == 3) Serial.print("Do ");
    if (dateTime.getWeekday() == 4) Serial.print("Fr ");
    if (dateTime.getWeekday() == 5) Serial.print("Sa ");
    if (dateTime.getWeekday() == 6) Serial.print("So ");
    Serial.println(clockString);   // Uhrzeit und Datum und Wochentag auf Terminal schreiben
    ZeitManuell();
  }

  Serial.print("Empfang ");       // Solange der Empfang aktiv ist soll dies auf dem Terminal ausgegeben werden
  Serial.print(SekundenSeitEmpfang);                                   // dann die Zeit in Sekunden seit der Empfang aktiviert wurde
  Serial.print("s ");
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
  byte SetzeStunde,SetzeMinute,SetzeSekunde,SetzeTag,SetzeMonat,SetzeJahr,SetzeWochentag;
  byte AnzahlBytes = Serial.available(); // Anzahl der Bytes im Buffer der seriellen Schnittstellen ermitteln
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
    SetzeTag = Buffer[0] * 10 + Buffer[1];
    SetzeMonat = Buffer[3] * 10 + Buffer[4];
    SetzeJahr = Buffer[6] * 10 + Buffer[7];
    SetzeStunde = Stunde;
    SetzeMinute = Minute;
    SetzeSekunde = Sekunde;
    SetzeWochentag = Wochentag;
    dateTime = DateTime(SetzeJahr, SetzeMonat, SetzeTag, SetzeWochentag, SetzeStunde, SetzeMinute, SetzeSekunde); // Jahr,Monat,Tag,Wochentag,Stunde,Minute,Sekunde
    RTC_DCF.setDateTime(&dateTime); //Uhrzeit setzen
  }
  if (Buffer[2] == 10)                  // In der seriellen Eingabe wurde an der dritten Stelle ein Doppelpunkt erkannt, also handelt es sich um eine Uhrzeit z.B.: 07:54.50
  {
    SetzeTag = Tag;
    SetzeMonat = Monat;
    SetzeJahr = Jahr;
    SetzeStunde = Buffer[0] * 10 + Buffer[1];
    SetzeMinute = Buffer[3] * 10 + Buffer[4];
    SetzeSekunde = Buffer[6] * 10 + Buffer[7];
    SetzeWochentag = Wochentag;
    dateTime = DateTime(SetzeJahr, SetzeMonat, SetzeTag, SetzeWochentag, SetzeStunde, SetzeMinute, SetzeSekunde); // Jahr,Monat,Tag,Wochentag,Stunde,Minute,Sekunde
    RTC_DCF.setDateTime(&dateTime); //Uhrzeit setzen
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
    SetzeTag = Tag;
    SetzeMonat = Monat;
    SetzeJahr = Jahr;
    SetzeStunde = Stunde;
    SetzeMinute = Minute;
    SetzeSekunde = Sekunde;
    SetzeWochentag = Buffer[1];
    dateTime = DateTime(SetzeJahr, SetzeMonat, SetzeTag, SetzeWochentag, SetzeStunde, SetzeMinute, SetzeSekunde); // Jahr,Monat,Tag,Wochentag,Stunde,Minute,Sekunde
    RTC_DCF.setDateTime(&dateTime); //Uhrzeit setzen
  }

}

void ZeitManuell()        // Uhr grob weiterrechnen solange eine unplausible Zeit vorliegt und die RTC-Zeit nicht genutzt wird
{
  Sekunde++;
  if (Sekunde == 60)      // Sekundenüberlauf              
  {
    Sekunde = 0;
    Minute++;
    if (Minute == 60)     // Minutenüberlauf
    {
      Minute = 0;
      Stunde++;
      if (Stunde == 24)   // Stundenüberlauf
        Stunde = 0;
    }
  }
}

void ZeitKopieren(void)
{
      Wochentag = dateTime.getWeekday();   // Wochentag aus dem Ergebnis von ELV-DCF77 holen
      Stunde = dateTime.getHour();         // Stunde aus dem Ergebnis von ELV-DCF77 holen
      Minute = dateTime.getMinute();       // Minute aus dem Ergebnis von ELV-DCF77 holen
      Sekunde = dateTime.getSecond();      // Sekunde aus dem Ergebnis von ELV-DCF77 holen
      Jahr = dateTime.getYear();           // Jahr aus dem Ergebnis von ELV-DCF77 holen
      Monat = dateTime.getMonth();         // Monat aus dem Ergebnis von ELV-DCF77 holen
      Tag = dateTime.getDay();             // Tag aus dem Ergebnis von ELV-DCF77 holen
}

