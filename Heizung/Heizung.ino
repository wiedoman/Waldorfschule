// Heizung ist auf 25° eingestellt (unter der Decke)
// Am Wochendene und in den Ferien wird nicht geheizt.

// Heizbeginn ist abhängig von der Temperatur nachts, je kälter umso früher.
// Heizen von 03:00 bis 17:00, wenn nicht kalt, dann kann sich der Heizbeginn bis 07:00 verzögern.

// Innerhalb der Heizperiode ist die rote Lampe an
// Laufen die Heizungen ist die gelbe Lampe an

// Ausserhalb der Heizperiode kann auf die rote Lampe gedrückt werden, solange bis sie anfängt zu blinken.
// Danach bleibt sie für zwei Stunden an.

// Sinkt die Temperatur innherhalb von 30s um 1°C, dann wird davon ausgegangen, 
// dass gelüftet wird und die Heizungen werden für 10min ausgeschaltet. Währenddessen blinkt die gelbe Lampe.


#include <OneWire.h>                  // OneWire Bibliothek
#include <DallasTemperature.h>        // Sensor Bibliothek
#include <Wire.h>                     // I2C Bibliothek
#include "RTClib.h"                   // Echtzeituhr Bibliothek

#define ONE_WIRE_BUS 10               // Sensor an Pin 10

OneWire oneWire(ONE_WIRE_BUS);        // OneWire Netzwerk erstellen
DallasTemperature sensor(&oneWire);   // Sensor mit OneWire verbinden
DeviceAddress insideThermometer;      // Adressspeicher des Sensors

float temperatur, SollTemperatur, lueftTemperatur=-100;

signed char DIP=0;
char relAn=0, Unterricht=0, SZ=0;
uint32_t HeizanforderungsEnde=0;
uint32_t intervallZeit=0,lueftenBeginn;    // Zeitstempel für die 20 Sekunden Lüftenüberwachung
uint8_t lueften = 0,blinken=0,sehrKalt=0;

RTC_DS1307 RTC;                       // RTC Object

#define RelaisAn LOW
#define RelaisAus HIGH
#define relais1 A0
#define relais2 A1
#define relais3 A2
#define dip1 9
#define dip2 8
#define dip3 7
#define dip4 6
#define dip5 5
#define dip6 4
#define LEDrot 11
#define LEDgelb 12
#define Taster A3

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

void setup(void)
{
  pinMode(relais1, OUTPUT);            // Relais an Pin A0 
  digitalWrite(relais1, RelaisAus);
  pinMode(relais2, OUTPUT);            // Relais an Pin A1
  digitalWrite(relais2, RelaisAus);
  pinMode(relais3, OUTPUT);            // Relais an Pin A2
  digitalWrite(relais3, RelaisAus);
  pinMode(LEDrot, OUTPUT);             // rot LED an Pin 11
  digitalWrite(LEDrot, 0);
  pinMode(LEDgelb, OUTPUT);            // gelb LED an Pin 12
  digitalWrite(LEDgelb, 0);
  pinMode(dip1, INPUT_PULLUP);         // DIP 1-6 an Pins 9-4
  pinMode(dip2, INPUT_PULLUP);
  pinMode(dip3, INPUT_PULLUP);
  pinMode(dip4, INPUT_PULLUP);
  pinMode(dip5, INPUT_PULLUP);
  pinMode(dip6, INPUT_PULLUP);
  pinMode(Taster, INPUT_PULLUP);       // Taster an Pin 13
  Serial.begin(9600);                  // Für die Ausgabe auf dem Terminal
  sensor.begin();                      // Sensor Bibliothek starten
  sensor.getAddress(insideThermometer, 0); // Abdresse des Sensors ermitteln
  Wire.begin();                        // I2C anschalten
  RTC.begin();                         // Kommunikation zur RTC beginnen
  delay(1000);
  sensor.requestTemperatures();
  delay(1000);
  // RTC.adjust(DateTime(__DATE__,__TIME__));
  // Uhr stellen - 2mal flashen, einmal mit der Zeit (im Sommer -1h von der aktuellen Zeit weil der Code +1 macht
  // danach unbedingt noch ein zweites mal flashen mit auskommentierter Zeile zum Zeit stellen
  // damit die RTC frei läuft
  // RTC.adjust(DateTime("Nov 10 2014", "15:59:50")); // Sommerzeit !! im Sommer -1h weil der Code +1 macht
}

void loop(void)
{ 
  Serial.println();
//clearAndHome();
  DateTime now = RTC.now();               // Zeit holen
  SZ = Sommerzeit();  
  if (SZ) now = (now.unixtime() + 3600);   // Wenn Sommerzeit, dann plus eine Stunde 
  Serial.print("               Zeit = "); // und ausgeben
  Serial.print(now.day(), DEC);
  Serial.print('.');
  Serial.print(now.month(), DEC);
  Serial.print('.');
  Serial.print(now.year(), DEC);      
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  if (SZ) Serial.println(F(" Sommerzeit")); else Serial.println(F(" Winterzeit"));
  Serial.print(F("          Wochentag = ")); // auch den Wochentag
  Serial.print(now.dayOfWeek());
  Serial.print(" = ");
  if(now.dayOfWeek() == 0) Serial.println(F("Sonntag"));
  if(now.dayOfWeek() == 1) Serial.println(F("Montag"));
  if(now.dayOfWeek() == 2) Serial.println(F("Dienstag"));
  if(now.dayOfWeek() == 3) Serial.println(F("Mittwoch"));
  if(now.dayOfWeek() == 4) Serial.println(F("Donnerstag"));
  if(now.dayOfWeek() == 5) Serial.println(F("Freitag"));
  if(now.dayOfWeek() == 6) Serial.println(F("Samstag"));
  Serial.print(F("          UNIX-Zeit = ")); // und die Zeit im UNIX Format
  Serial.println(now.unixtime());

  Serial.println(F("   Normaltemperatur = 22")); // Normaltemperatur ausgeben (fix 22°)

  DIP=0;                                 // DIP auslesen
  if(!digitalRead(dip6)) DIP += 1; 
  if(!digitalRead(dip5)) DIP += 2; 
  if(!digitalRead(dip4)) DIP += 4; 
  if(!digitalRead(dip3)) DIP += 8; 
  if(!digitalRead(dip2)) DIP += 16; 
  if(!digitalRead(dip1)) DIP *= -1; 
  Serial.print(F("                DIP = "));// und DIP Stellungn ausgeben
  Serial.print(digitalRead(dip1));
  Serial.print(digitalRead(dip2));
  Serial.print(digitalRead(dip3));
  Serial.print(digitalRead(dip4));
  Serial.print(digitalRead(dip5));
  Serial.println(digitalRead(dip6));
  Serial.print(F("Temperaturkorrektur = "));// und Korrekturwert von DIP Schalter ausgeben 
  Serial.println(DIP);             
  
  SollTemperatur = 22;                    // Solltemperatur (StartWert 22°)
  SollTemperatur += DIP;                  // ausrechnen
  Serial.print(F("     Solltemperatur = ")); // und ausgeben
  Serial.println(SollTemperatur);             

  sensor.requestTemperatures();                    // Temperatur von Sensor holen
  temperatur = sensor.getTempC(insideThermometer); // nach °C umrechnen
  if(lueftTemperatur == -100) lueftTemperatur = temperatur;
  Serial.print(F("      Isttemperatur = "));          // und ausgeben
  Serial.println(temperatur);         

  if ( (now.unixtime() - intervallZeit) > 30){
    lueftTemperatur = temperatur;
    intervallZeit = now.unixtime();
  }
  Serial.print(F("          Deltazeit = "));          // und ausgeben
  Serial.println(now.unixtime()-intervallZeit);
  Serial.print(F("    Deltatemperatur = "));          // und ausgeben
  Serial.println(temperatur - lueftTemperatur);
  if(!lueften && ((lueftTemperatur - temperatur) > 1)) {
    lueften = 1;
    lueftenBeginn = now.unixtime();
  }
  Serial.print(F("            Lueften = "));          // und ausgeben
  if(lueften) {
    if(now.unixtime()-lueftenBeginn > 600) lueften = 0;
    Serial.println(now.unixtime() - lueftenBeginn); 
  }
  else {
    Serial.println(F("Fenster geschlossen"));
  }

  while(!digitalRead(Taster))                 // Taster gedrückt?
  {                            
    HeizanforderungsEnde = now.unixtime()+7200; // Heiszanforderungszeit setzen auf 2 Stunden
    digitalWrite(LEDrot, 0);                    // und zur Bestätigung blinken solange der Taster gedrückt 
    delay(100);
    digitalWrite(LEDrot, 1);
    delay(100);
  }
  Serial.print(F("             Taster = "));     // Heizanforderungstaste einlesen 
  Serial.println(digitalRead(Taster));        // und ausgeben

  // Unterrichtszeit ? Dann heizen von 03:00 bis 17:00, wenn nicht kalt kann Heizbeginn bis 07:00 verzögern
  Serial.print(F("         Auswertung = "));     // Ausgabe Ferien oder Unterricht
  if(now.hour() >= 8) sehrKalt = 0;
  if(!sehrKalt && now.hour() == 3 && temperatur <  8) sehrKalt = 5;
  if(!sehrKalt && now.hour() == 4 && temperatur < 12) sehrKalt = 4;
  if(!sehrKalt && now.hour() == 5 && temperatur < 16) sehrKalt = 3;
  if(!sehrKalt && now.hour() == 6 && temperatur < 19) sehrKalt = 2;
  if(!sehrKalt && now.hour() == 7 && temperatur < 22) sehrKalt = 1;
  if(now.dayOfWeek() == 0 || now.dayOfWeek() == 6) 
  {
    Serial.println(F("Wochenende"));
    Unterricht = 0;
  }
  else if(Ferien()) Unterricht = 0;
  else if(now.hour() >= (8-sehrKalt) && now.hour() < 16) 
  {
    Unterricht = 1; 
    Serial.print(F("Unterricht Heizungsbeginn: 0"));
    Serial.print(8-sehrKalt);
    Serial.println(F(":00 Uhr"));
  }
  else 
  {
    Unterricht = 0;
    Serial.println(F("Unterrichtstag ausserhalb Unterrichtszeit"));
  }

  Serial.print(F("Taster verbleinde s = "));     // Heizanforderung
  if(HeizanforderungsEnde > now.unixtime() || Unterricht) // Während der Heizanforderung 
  {
    digitalWrite(LEDrot, 1);                              // rote LED an
    if(HeizanforderungsEnde > now.unixtime())             // wenn Anforderung aufgrund Taster
    {
      int z,h,m;
      z = HeizanforderungsEnde - now.unixtime();
      h = z/3600;
      m = (z/60)-(z/3600*60);
      Serial.print(h);                                   // verbleibende Zeit berechnen
      Serial.print("h ");                                // und ausgeben  
      Serial.print(m);                                   // verbleibende Zeit berechnen
      Serial.print("m ");                                // und ausgeben  
      Serial.print(z-(3600*h)-(60*m));                   // verbleibende Zeit berechnen
      Serial.println("s");                               // und ausgeben  
    }
    else
    {
      Serial.println("0s");                               // sonst 0s ausgeben
    }
    Serial.println(F("    Heizanforderung = 1"));    // Heizanforderung ausgeben
    if(temperatur < SollTemperatur) relAn = 1;    // Temperatur kleiner Soll? dann Relais an
    if(temperatur > SollTemperatur + 1) relAn = 0;// Ausschalten erst wenn Soll + 1°
  }
  else                                            // wenn keine Heizanforderung 
  {
    digitalWrite(LEDrot, 0);                      // dann rote LED aus   
    Serial.println("0s");                         // Ausgeben
    relAn = 0;                                    // Und Relais aus
    Serial.println(F("    Heizanforderung = 0"));    // Heizanforderung ausgeben
  }
  
  Serial.print(F(" Heizungsrelais 1-3 = "));  // Status der Relais ausgeben und Relais schalten ...
  if(relAn)
  {
    if(lueften){
      Serial.println(F("aus wegen geoeffnetem Fenster"));
      digitalWrite(relais1, RelaisAus);      // Heizung ausschalten
      delay(300);
      digitalWrite(relais2, RelaisAus); 
      delay(300);
      digitalWrite(relais3, RelaisAus); 
      if(blinken) blinken = 0; else blinken=1;
      digitalWrite(LEDgelb, blinken);               // gelbe LED aus = Heizungen aus
    }
    else{
      Serial.println(F("an"));
      digitalWrite(relais1, RelaisAn);       // Heizung anschalten
      delay(300);
      digitalWrite(relais2, RelaisAn);       
      delay(300);
      digitalWrite(relais3, RelaisAn);       
      digitalWrite(LEDgelb, 1);              // gelbe LED an = Heizungen an
    }
  }
  else
  {
    Serial.println(F("aus"));
    digitalWrite(relais1, RelaisAus);      // Heizung ausschalten
    delay(300);
    digitalWrite(relais2, RelaisAus); 
    delay(300);
    digitalWrite(relais3, RelaisAus); 
   digitalWrite(LEDgelb, 0);               // gelbe LED aus = Heizungen aus
  }

  delay(300);
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
  DateTime now1 = RTC.now();               // Zeit holen

  // Erst werden die Daten für das aktuelle Jahr kopiert, damit die Prüfroutine unabhängig vom Jahr aufgebaut werden kann
  if (now1.year() == 2014)                      // 2014
  {
    for(byte i=0;i<20;i++)              // Tabellen mit 20 Einträge für Feriendaten von 10 Ferien pro Jahr
    {
      FerienMonat[i]=FerienMonat14[i]; // Kopieren der Monatstabelle
      FerienTag[i]=FerienTag14[i];     // Kopieren der Tagestabelle
    }
  }
  if (now1.year() == 2015)                      // 2015
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat15[i];
      FerienTag[i]=FerienTag15[i];
    }
  }
  if (now1.year() == 2016)                      // 2016
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat16[i];
      FerienTag[i]=FerienTag16[i];
    }
  }
  if (now1.year() == 2017)                      // 2017
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat17[i];
      FerienTag[i]=FerienTag17[i];
    }
  }
  if (now1.year() == 2018)                      // 2018
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat18[i];
      FerienTag[i]=FerienTag18[i];
    }
  }
  if (now1.year() == 2019)                      // 2019
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
    if(now1.month() > FerienMonat[i]) // Monat in Tabelle schon vorbei? Dann muss der Tag nicht geprüft werden
    {
      Auswertung[i] = 1;  // Dann in Auswertetabelle mit 1 merken
    }
    else if(i%2 && now1.month() == FerienMonat[i] && now1.day() > FerienTag[i]) // Der Tag für den Ferienanfang ist immer an geradadzahligen Stellen in der Tabelle und wird ausgeschlossen
    {                                                             // Wenn der Monat noch nicht vorbei ist, dann prüfen ob der Monat in der Tabelle der aktuelle Monat ist und Tag prüfen
      Auswertung[i] = 1;                                    // Dann mit 1 merken
    }
    else if(!(i%2) && now1.month() == FerienMonat[i] && now1.day() >= FerienTag[i]) // Der Tag für das Ferienende ist immer an ungeradadzahligen Stellen in der Tabelle und wird mit berücksichtigt
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
        Serial.println(F("Winterf."));          // Prüfung beenden und Ergebnis ausgeben
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==2)                                           // An der Stelle 2 = 1 und 3 = 0 handelt es sich um die Faschingsferien 
      {
        Serial.println(F("Fasching"));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==4)                                           // An der Stelle 4 = 1 und 5 = 0 handelt es sich um die Osterferien
      {
        Serial.println(F("Ostern"));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==6)                                           // An der Stelle 6 = 1 und 7 = 0 handelt es sich um den 1. Mai
      {
        Serial.println(F("1. Mai"));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==8)                                           // An der Stelle 8 = 1 und 9 = 0 handelt es sich um Himmelfahrt
      {
        Serial.println(F("Himmelfahrt"));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==10)                                          // An der Stelle 10 = 1 und 11 = 0 handelt es sich um die Pfingstferien  
      {
        Serial.println(F("Pfingsten"));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==12)                                          // An der Stelle 12 = 1 und 13 = 0 handelt es sich um die Sommerferien  
      {
        Serial.println(F("Sommerf."));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==14)                                          // An der Stelle 14 = 1 und 15 = 0 handelt es sich um den Tag der deutschen Einheit
      {
        Serial.println(F("TdE"));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==16)                                          // An der Stelle 16 = 1 und 17 = 0 handelt es sich um die Herbstsferien  
      {
        Serial.println(F("Herbstf."));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
      if (b==18)                                          // An der Stelle 18 = 1 und 19 = 0 handelt es sich um die Winterferiensferien 1. Teil
      {
        Serial.println(F("Winterf."));
        return 1;                                         // Aktuelles Datum war innerhalb der Ferien, damit Rückgabewert 1 = Ferien
      }
    }
  }
  return 0; // Aktuelles Datum war NICHT innerhalb der Ferien, damit Rückgabewert 0 = keine Ferien
}


boolean Sommerzeit(void)
{
  DateTime now2 = RTC.now();               // Zeit holen
  if ((now2.month() < 3) || (now2.month() > 10)) return 0; // Wenn nicht innerhalb der Sommerzeitmonate, dann gleich abbrechen
  if ( ((now2.day() - now2.dayOfWeek()) >= 25) && (now2.dayOfWeek() || (now2.hour() >= 2)) ) // Wenn nach dem letzten Sonntag und nach 2:00 Uhr
  {
    if (now2.month() == 10 ) return 0;                                                       // dann wenn Oktober, dann gleich abbrechen
  }
  else                                                                                       // Wenn nicht nach dem letzten Sonntag bzw. vor 2:00 Uhr 
  {
    if (now2.month() == 3) return 0;                                                         // und es ist März, dann gleich abbrechen
  }
  return 1;        // ansonsten innerhalb der SZMonate, nach dem letzten Sonntag und nicht Oktober, vor dem letzten Sonntag und nicht März, dann ist SZ           
}


void clearAndHome()
{
  Serial.write(27);
  Serial.print("[2J"); // clear screen
  Serial.write(27); // ESC
  Serial.print("[H"); // cursor to home
} 
