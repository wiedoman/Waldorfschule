char Version[] = "V3.3, Frostschutz, Fenster, Sensorfehler, Heizlüfter vor Unterricht";
// V3.3, 
//    Tasterdelay = 100ms wg. Störungen im Kasten und langem Kabel
//    Temperaturprüfung und Wert einfrieren wenn Temperatursensorwert unplausibel, 
//    Init auf 22 falls der Temperatursensor noch nie einen plausiblen Wert geliefert hat
//    1°C Hysterese bei den Übergängen aller Heizungsstufen 
//    Ist die Temperatur beim Anfordern des 2h heizens kleiner 17°C wird auch gleich der Lüfter mit eingeschaltet

// Heizung ist auf 22° eingestellt (unter der Decke) 
// Am Wochendene und in den Ferien wird nicht geheizt.

// Heizbeginn ist abhängig von der Temperatur nachts, je kälter umso früher.
// Heizen von 03:00 bis 17:00, wenn nicht kalt, dann kann sich der Heizbeginn bis 07:00 verzögern.

// Innerhalb der Heizperiode ist die rote Lampe an
// Laufen die Heizungen ist die gelbe Lampe an

// Ausserhalb der Heizperiode kann auf die rote Lampe gedrückt werden, solange bis sie anfängt zu blinken.
// Danach bleibt sie für zwei Stunden an.

// Sinkt die Temperatur innherhalb von 30s um 1°C, dann wird davon ausgegangen, 
// dass gelüftet wird und die Heizungen werden für 10min ausgeschaltet. Währenddessen blinkt die gelbe Lampe.

// Heizungsregelung über 3 Heizstufen:
// Stufe 1:  750 Watt, 1. Relais an,  2. Relais aus
// Stufe 2: 1250 Watt, 2. Relais aus, 2. Relais an
// Stufe 3: 2000 Watt, 1. Relais am,  2. Relais an
// Temperaturdelta > 3°: Stufe 3
// Temperaturdelta > 2°: Stufe 2, wenn nach 15min immer noch > 2°: Stufe + 1
// Temperaturdelta > 1°: Stufe 1, wenn nach je 10min immer noch > 1°: je Stufe + 1

// Wenn sehrKalt > 3: Stufe 1 und 2 verbieten
// Wenn sehrKalt > 2: Stufe 1 verbieten


// Die Heizungen verfügen über Lüfter die ausserhalb der Unterrichstzeit, also vor 8:00 laufen
// 2. Sekunden Taste betätigen schaltet die Lüfter an/aus
// Lüfter an: 3. Relais an


#include <OneWire.h>                  // OneWire Bibliothek
#include <DallasTemperature.h>        // Sensor Bibliothek
#include <Wire.h>                     // I2C Bibliothek
#include "RTClib.h"                   // Echtzeituhr Bibliothek

#define ONE_WIRE_BUS 10               // Sensor an Pin 10

OneWire oneWire(ONE_WIRE_BUS);        // OneWire Netzwerk erstellen
DallasTemperature sensor(&oneWire);   // Sensor mit OneWire verbinden
DeviceAddress insideThermometer;      // Adressspeicher des Sensors

float temperatur = 22, SollTemperatur, lueftTemperatur=-100, regelAbweichung;    // Temperaturen
signed char DIP=0;                                              // DIP Schalter für Temperaturkorrekturen
int relAn=0,  SZ=0,  heizungsStufe=0, heizungsStufePlus=0, relAnAlt=0;
uint32_t HeizanforderungsEnde=0, intervallZeit=0, lueftenBeginn, stufenReduktion, anforderungAnZeit, anforderungAusZeit;    // Zeitstempel 
uint8_t tasterAlt=1, tasterLow = 0, luefterVorheizen = 0,luefterVorheizenA = 0,heizungsLuefterAnforderung=0, heizungsLuefter=0,angefordert=0,Unterricht=0,lueften = 0,blinken=0,sehrKalt=0;
unsigned long loopzeit;                                         // millis() Ergbenis

RTC_DS1307 RTC;                       // RTC Object

#define RelaisAn LOW                  // Relais Massegesteuert
#define RelaisAus HIGH
#define relais1 A0                    // Relais für 750W Ansteuerung
#define relais2 A1                    // Relais für 1250W Ansteuerung (Rel 1+2 = 2000W)
#define relais3 A2                    // Relais für Lüfteransteuerung in den Heizungen
#define dip1 9                        // Ports der 6 DIP Schalter
#define dip2 8
#define dip3 7
#define dip4 6
#define dip5 5
#define dip6 4
#define LEDrot 11                     // mittlerweise blaue LED am externen Bedienteil für Heizanforderungsbestätigung
#define LEDgelb 12                    // gelbe LED an der Steuerung für die Anzeige ob mindestens ein Releis angesteuert wurde, blinken = geöffnetes Fenster erkannt
#define Taster 2                      // Taster wg. Interruptsteuerung an PIN 2

//  Ferienzeiten 2014 - 2023
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
const byte FerienTag18[20]    = { 1, 5,12,16,26, 6,30, 1,10,11,22,02,26, 8, 3, 4,29, 2,22,31}; // Feriendaten für 2018 Tag
const byte FerienMonat18[20]  = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 5, 5, 6, 7, 9,10,10,10,11,12,12}; // Feriendaten für 2018 Monat
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
  pinMode(Taster, INPUT_PULLUP);       // Taster an Pin 2
  Serial.begin(115200);                // Für die Ausgabe auf dem Terminal
  sensor.begin();                      // Sensor Bibliothek starten
  sensor.getAddress(insideThermometer, 0); // Abdresse des Sensors ermitteln
  Wire.begin();                        // I2C anschalten
  RTC.begin();                         // Kommunikation zur RTC beginnen
  delay(1000);                         // Notwendiges delay wg. RTC
  sensor.requestTemperatures();        // onewire Protokoll für Dallas DS1820 Temperatursensor
  delay(1000);                         // Notwendiges delay wg. onewire
  loopzeit = millis();                 // Startzeit merken 
  attachInterrupt(digitalPinToInterrupt(Taster), Anforderung, FALLING); // Taster an PIN 2, löst Interruptgesteuert Unterfunktion Anforderung() aus
  // ### RTC.adjust(DateTime(__DATE__,__TIME__));
  // ### Uhr stellen - 2mal flashen, einmal mit der Zeit (im Sommer -1h von der aktuellen Zeit weil der Code +1 macht
  // ### danach unbedingt noch ein zweites mal flashen mit auskommentierter Zeile zum Zeit stellen
  // ### damit die RTC frei läuft
  // RTC.adjust(DateTime("Oct 31 2017", "17:47:30")); // Sommerzeit !! im Sommer -1h weil der Code +1 macht
}

void loop(void)
{ 
  clearAndHome();                               // Terminalfenster Cursorsteuerung auf Pos 0,0, oben links
  if(tasterLow)                                 // Taster gedrückt? Für die Erkennung der 2s Betätigung für die Lüfteransteuerung in den Heizungen
  {
    tasterLow=0;                                // Trigger aus Interruptserviceroutine löschen
    if(temperatur < 17) luefterVorheizenA = 1;  // Ist die Temperatur beim Anfordern des 2h heizens, dann auch den Lüfter anschalten.
    anforderungAnZeit=millis();                 // Zeitstempel für die 2s Erkennung
    delay(10);                                  // Tasterentprellung
    while(digitalRead(Taster)==0){delay(100);}  // Warten bis der Taster losgelassen wird
    anforderungAusZeit = millis();              // zweiter Zeitstempel für die 2s Erkennung
    Serial.println("              ");           // Zeitstempel und Delta ausgeben
    Serial.println("              ");
    Serial.println("              ");
    Serial.print("                                                        an:");
    Serial.print(anforderungAnZeit);Serial.println("ms              ");
    Serial.print("                                                        aus:");
    Serial.print(anforderungAusZeit);Serial.println("ms              ");
    Serial.print("                                                        diff:");
    Serial.print(anforderungAusZeit-anforderungAnZeit);Serial.println("ms              ");
    
    if(anforderungAusZeit - anforderungAnZeit > 2000) heizungsLuefterAnforderung = 1; else if(anforderungAusZeit - anforderungAnZeit > 100) angefordert = 1;
  }
  if (millis() - loopzeit > 1000)               // Hauptprogramm einmal pro Sekunde bzw. nach Ablauf einer Sekunde berechnen
  {
    clearAndHome();                             // Terminalfenster Cursorsteuerung auf Pos 0,0, oben links
    Serial.println(Version);                    // Programmversion ausgeben
    Serial.println();                       
    loopzeit = millis();                        // Zeitstempel für nächtste Programmerechnung, siehe oben, 1 Sekunde
    DateTime now = RTC.now();                   // Zeit vom RTC holen
    SZ = Sommerzeit();                          // Prüfen auf Sommerzeit
    if (SZ) now = (now.unixtime() + 3600);      // Wenn Sommerzeit, dann plus eine Stunde 
    Serial.print("               Zeit = ");     // und Datum und die Uhrzeit ausgeben
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
    if (SZ) Serial.print(F(" Sommerzeit")); else Serial.print(F(" Winterzeit"));Serial.println("              "); 
    Serial.print(F("          Wochentag = ")); // auch den Wochentag
    Serial.print(now.dayOfWeek());
    Serial.print(" = ");
    if(now.dayOfWeek() == 0) Serial.println(F("Sonntag         "));
    if(now.dayOfWeek() == 1) Serial.println(F("Montag          "));
    if(now.dayOfWeek() == 2) Serial.println(F("Dienstag        "));
    if(now.dayOfWeek() == 3) Serial.println(F("Mittwoch        "));
    if(now.dayOfWeek() == 4) Serial.println(F("Donnerstag      "));
    if(now.dayOfWeek() == 5) Serial.println(F("Freitag         "));
    if(now.dayOfWeek() == 6) Serial.println(F("Samstag         "));
    Serial.print(F("          UNIX-Zeit = ")); // und die Zeit im UNIX Format
    Serial.print(now.unixtime());Serial.println("s              ");
  
    Serial.println(F("   Normaltemperatur = 22°C")); // Normaltemperatur ausgeben (fix 22°)
  
    DIP=0;                                 // DIP auslesen
    if(!digitalRead(dip6)) DIP += 1; 
    if(!digitalRead(dip5)) DIP += 2; 
    if(!digitalRead(dip4)) DIP += 4; 
    if(!digitalRead(dip3)) DIP += 8; 
    if(!digitalRead(dip2)) DIP += 16; 
    if(!digitalRead(dip1)) DIP *= -1; 
    Serial.print(F("                DIP = "));// und DIP Stellungen ausgeben
    Serial.print(digitalRead(dip1));
    Serial.print(digitalRead(dip2));
    Serial.print(digitalRead(dip3));
    Serial.print(digitalRead(dip4));
    Serial.print(digitalRead(dip5));
    Serial.println(digitalRead(dip6));
    Serial.print(F("Temperaturkorrektur = "));// und Korrekturwert von DIP Schalter ausgeben 
    Serial.print(DIP);Serial.println("°C              ");
    
    SollTemperatur = 22;                    // Solltemperatur (StartWert 22°)
    SollTemperatur += DIP;                  // Korrekturwert aus DIP addieren
    Serial.print(F("     Solltemperatur = ")); // und resultierende Solltemperatur ausgeben
    Serial.print(SollTemperatur);Serial.println("°C              ");
  
    sensor.requestTemperatures();                    // Temperatur von Sensor holen
    if (sensor.getTempC(insideThermometer) < -100)   // Ist der Sensor fehlerhaft, dann ist der Wert -127°C
    {
      Serial.print(F("      Isttemperatur = "));     // und ausgeben
      Serial.print(temperatur);Serial.println("°C Altwert wg. fehlerhaftem Sensor      "); 
    }
    else
    {
      temperatur = sensor.getTempC(insideThermometer);  // bei nicht fehlerhaftem Sensorwert wird er übernommen
      Serial.print(F("      Isttemperatur = "));        // und ausgegeben
      Serial.print(temperatur);Serial.println("°C                                     ");
    }
    if(lueftTemperatur == -100) lueftTemperatur = temperatur;  // Initwert der Lüfttemperatur für die Erkennung eines offenen Fensters
  
    if ( (now.unixtime() - intervallZeit) > 30){       // Innerhalb eines Zeitfensters von 30 Sekunden wird die Temperatur überwacht
      lueftTemperatur = temperatur;                    // Zu Beginn des Zeitfensters wird der Temperaturwert eingefroren 
      intervallZeit = now.unixtime();                  // Zeitstempel für nächstes 30s Fenster 
    }
    Serial.print(F("          Deltazeit = "));          // Zeitfenster
    Serial.print(now.unixtime()-intervallZeit);Serial.println("s (max 30s)             ");
    Serial.print(F("    Deltatemperatur = "));          // Temperaturänderung innerhalb des Zeitfensters
    Serial.print(temperatur - lueftTemperatur);Serial.println("°C              ");
    if(!lueften && ((lueftTemperatur - temperatur) > 1)) {  // Hat sich die Temperatur innerhalb des Zeitfernsters um 1°C gesenkt
      lueften = 1;                                          // Wird Lüften erkannt
      lueftenBeginn = now.unixtime();                       // Zeitstempel für die 10min Lüftungsfunktion, während dessen, die Heizung ausgeschaltet wird
    }
    Serial.print(F("            Lueften = "));          // und ausgeben
    if(lueften) {                                       // Ausgabe des Lüftungsstatus
      if(now.unixtime()-lueftenBeginn > 600) lueften = 0; // Nach Ablauf der 10min wird der Lüftungsstatus zurück genommen
      Serial.print(now.unixtime() - lueftenBeginn);Serial.println("s (max 600s)              ");
    }
    else {
      Serial.println(F("Fenster geschlossen             "));
    }
  
    if(angefordert)                 // Taster gedrückt?
    {                            
      angefordert = 0;              // Anforderungsstatus zurücknehmen für nächste Anforderung, dann wird das 2h Fenster neu begonnen
      HeizanforderungsEnde = now.unixtime()+7200; // Heizanforderungszeit setzen auf 2 Stunden
      Serial.println(F("             Taster = 1       "));     // Heizanforderungstaste ausgeben
      digitalWrite(LEDrot, 0);      // Bestätigung für den Benutzer durch mehrmaliges Blinken der blauen LED
      delay(100);
      digitalWrite(LEDrot, 1);
      delay(100);
      digitalWrite(LEDrot, 0);
      delay(100);
      digitalWrite(LEDrot, 1);
      delay(100);
      digitalWrite(LEDrot, 0);
      delay(100);
    }
    else
    {
      Serial.println(F("             Taster = 0          "));     // Heizanforderungstaste ausgeben
    }

    if(heizungsLuefterAnforderung)  // Wurde eine 2s Tasterbetätigung erkannt, dann werden die Lüfter der Heizungen eingeschaltet
    {
      heizungsLuefterAnforderung=0; // Anforderung für die nächste Anforderung löschen
      if(luefterVorheizen)          // Falls der Benutzer die Lüfter nicht angefordert hatte, dann liegt eine Anforderung
      {                             // für die Lüfter vom Vorheizen vor, dann wird die zurück genommen
        luefterVorheizen = 0;
      }
      if(luefterVorheizenA)          // Falls der Benutzer die Lüfter nicht angefordert hatte, dann liegt eine Anforderung
      {                             // für die Lüfter vom Vorheizen vor, dann wird die zurück genommen
        luefterVorheizenA = 0;
      }
      if(heizungsLuefter)      // Falls keine Anforderung durch das Vorheizen vorlag, dann wird die Anforderung des Benutzer
      {                             // zurück genommen falls eine vorlag
        heizungsLuefter = 0; 
      }
      else
      {
        heizungsLuefter = 1;        // lag gar keine Anforderung vor, dann werden die Lüfter in den Heizungen eingeschaltet
      }
    }
    
    // Unterrichtszeit ? Dann heizen von 03:00 bis 17:00, wenn nicht kalt kann Heizbeginn bis 07:00 verzögern
    Serial.print(F("         Auswertung = "));     // Ausgabe Ferien oder Unterricht
    if(now.hour() >= 8) sehrKalt = 0;              // Die Vorheizanforderung wird nach Unterrichtsbeginn zurück genommen
    if(now.hour() >= 7 && now.minute() >= 45) luefterVorheizen = 0;   // Die Lüfteranforderung durch das Vorheizen wird 15min vor Unterrichtsbeginn zurück genommen
    if(!sehrKalt && now.hour() == 3 && temperatur <  8) {sehrKalt = 5; luefterVorheizen = 1; heizungsStufePlus = 3;}
    if(!sehrKalt && now.hour() == 4 && temperatur < 12) {sehrKalt = 4; luefterVorheizen = 1; heizungsStufePlus = 3;}
    if(!sehrKalt && now.hour() == 5 && temperatur < 16) {sehrKalt = 3; luefterVorheizen = 1; heizungsStufePlus = 2;}
    if(!sehrKalt && now.hour() == 6 && temperatur < 19) {sehrKalt = 2; luefterVorheizen = 1; heizungsStufePlus = 1;}
    if(!sehrKalt && now.hour() == 7 && temperatur < 22) {sehrKalt = 1; luefterVorheizen = 1; heizungsStufePlus = 1;}
    if(now.dayOfWeek() == 0 || now.dayOfWeek() == 6) // Samstag oder Sonntag = Wochenende
    {
      Serial.println(F("Wochenende          "));     // Ausgabe des Status ob Wochende oder Ferien oder Unterricht 
      Unterricht = 0;                                // Nur wenn Unterricht erkannt wird, wird automatisch geheizt, bzw. bei sehr tiefen Temperaturen entsprechend vorgeheizt
    }
    else if(Ferien()) Unterricht = 0;                // Prüfung ob Ferien (siehe Unterfunktion Ferien()) 
    else if(now.hour() >= (8-sehrKalt) && now.hour() < 16) // Heizungsbeginn falls keine der vorherigen Bedingungen eintraf
    {
      Unterricht = 1;                                // automatisches heizen während dem Unterricht 
      Serial.print(F("Unterricht Heizungsbeginn: 0")); // Heizungsstart ausgeben
      Serial.print(8-sehrKalt);
      Serial.println(F(":00 Uhr     "));
    }
    else 
    {
      Unterricht = 0;                                // falls nach 16:00 oder vor Vorheizbeginn werden die Heizungen ausgeschaltet
      Serial.println(F("Unterrichtstag aber ausserhalb Unterrichtszeit    "));
    }
                   
    Serial.print(F("  Anforderung Zeit  = "));              // Heizanforderung
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
        Serial.print(":");                                 // und ausgeben  
        Serial.print(m);                                   // verbleibende Zeit berechnen
        Serial.print(":");                                 // und ausgeben  
        Serial.print(z-(3600*h)-(60*m));                   // verbleibende Zeit berechnen
        Serial.println("            ");                    
      }
      else
      {
        Serial.println("0s           ");                   // sonst 0s ausgeben
      }
      regelAbweichung = SollTemperatur - temperatur;       // Temperaturdelta des Reglers (Ist - Soll)
      Serial.print(F("   Reglerabweichung = ")); Serial.print(regelAbweichung); Serial.println("°C   ");
      Serial.print(F("    Heizanforderung = Stufe "));     // Heizanforderungsstufe ausgeben 0=aus, 1=750W, 2=1250W, 3=2000W
      if(regelAbweichung < -1) heizungsStufePlus = 0;      // bei Heizende die Stufenerhöhung zurück nehmen
      if(regelAbweichung - float(heizungsStufe) >  1) heizungsStufe++;// Stufe erhöhen?
      if(regelAbweichung - float(heizungsStufe) < -1) heizungsStufe--;// Stufe erniedrigen?
      Serial.print("roh:");Serial.print(heizungsStufe);
      if(heizungsStufe < 0) heizungsStufe = 0; if(heizungsStufe > 3) heizungsStufe = 3;       // Stufe nur zwichen 0 und 3 erlaubt
      Serial.print(", begrenzt:");Serial.print(heizungsStufe);
      relAn = heizungsStufe + heizungsStufePlus;           // Stufenerhöhung berücksichtigen
      if(relAn < 0) relAn = 0; if(relAn > 3) relAn = 3;    // Stufe nur zwichen 0 und 3 erlaubt
      Serial.print(", final:");
      Serial.print(relAn);Serial.println("              ");
      if(relAnAlt > relAn || relAn == 3) stufenReduktion = now.unixtime(); // Nach Stufenreduktion 10min Fenster starten
      if(relAn && (now.unixtime() - stufenReduktion) > 600)// wenn die Heizungsstufe in den letzten 10min nicht reduziert wurde, wieder erhöhen
      {
         if (heizungsStufePlus < 2) heizungsStufePlus++;   // Heizstufenerhöhung erhöhen
         stufenReduktion = now.unixtime();                 // 10 min Fenster neu beginnen
      }
      relAnAlt = relAn;
    }
    else                                                   // wenn keine Heizanforderung 
    {
      digitalWrite(LEDrot, 0);                             // dann rote LED aus   
      Serial.println("0s           ");                     // Ausgeben
      relAn = 0;                                           // Und Relais aus
      Serial.println(F("    Heizanforderung = 0             "));// Heizanforderung ausgeben
    }
    Serial.print(F(" Heizstufenerhöhung = "));      // Ausgabe der relsutierenden Erhöhung der Heizstufe wg. zu langer Heizdauer
    Serial.print(heizungsStufePlus);Serial.println("              ");
    if(!relAn) stufenReduktion = now.unixtime();    // bei ausgeschalteter Heizung läuft das 10min Fenster nicht
    Serial.print(F("seit ltz. Reduktion = "));      // Ausgabe des 10min Zeitfensters
    Serial.print(now.unixtime() - stufenReduktion);Serial.println("s (max 600s)             ");
    
    Serial.print(F(" Heizungsrelais 1,2 = "));  // Status der Relais ausgeben und Relais schalten ...
    if(relAn)
    {
      if(lueften){                             // Falls ein offenes Fenster erkannt wurde
        Serial.println(F("aus wegen geoeffnetem Fenster"));
        stufenReduktion = now.unixtime();      // 10min Fenster während dem Lüften zurück setzen
        digitalWrite(relais1, RelaisAus);      // Heizung ausschalten
        delay(300);
        digitalWrite(relais2, RelaisAus); 
        if(blinken) blinken = 0; else blinken=1; // LED blinken lassen 
        digitalWrite(LEDgelb, blinken);          // gelbe LED aus = Heizungen aus
      }
      else{
        if(relAn == 1)                           // Relais entsprechend der Stufe schalten
        {                                        // Stufe eins 750W Relais 1 an und Relais 2 aus  
          Serial.println(F("1=an, 2=aus, 750W              "));
          digitalWrite(relais1, RelaisAn);       // Heizung anschalten
          delay(300);
          digitalWrite(relais2, RelaisAus);       
          digitalWrite(LEDgelb, 1);              // gelbe LED an = Heizungen an
        }
        if(relAn == 2)                           // Stufe zwei 1250W Relais 1 aus und Relais 1 an
        {
          Serial.println(F("1=aus, 2=an, 1250W                   "));
          digitalWrite(relais1, RelaisAus);      // Heizung anschalten
          delay(300);
          digitalWrite(relais2, RelaisAn);       
          digitalWrite(LEDgelb, 1);              // gelbe LED an = Heizungen an
        }
        if(relAn == 3)                           // Stufe drei 2000W Relais 1 und Relais 2 an
        {
          Serial.println(F("1=an, 2=an, 2000W                     "));
          digitalWrite(relais1, RelaisAn);       // Heizung anschalten
          delay(300);
          digitalWrite(relais2, RelaisAn);       
          digitalWrite(LEDgelb, 1);              // gelbe LED an = Heizungen an
        }
      }
    }
    else if (temperatur < 5)                     // Bei Temperatur unter 5°C wird die Heizung auf Stufe 750W eingeschaltet
    {
      Serial.println(F("1=an, 2=aus, 750W  Frostschutz   "));
      digitalWrite(relais1, RelaisAn);           // Relais 1 an 
      delay(300);
      digitalWrite(relais2, RelaisAus);          // Relais 2 aus
      digitalWrite(LEDgelb, 1);                  // gelbe LED an = Heizungen an
    }
    else
    {
      Serial.println(F("aus                                          ")); // Stufe 0 null 0W beide Relais aus
      digitalWrite(relais1, RelaisAus);      // Heizung ausschalten
      delay(300);
      digitalWrite(relais2, RelaisAus); 
     digitalWrite(LEDgelb, 0);               // gelbe LED aus = Heizungen aus
    }

    Serial.print(F("   Heizlüfterrelais = "));// Status der Relais ausgeben und Relais schalten ...
    if(luefterVorheizen || luefterVorheizenA) // Anforderung der Heizungslüfter aus Vorheizanforderung
    {
      Serial.println(F("an vorheizen    "));
      digitalWrite(relais3, RelaisAn);        // Heizlüfter anschalten
    }
    else
    {
      if(heizungsLuefter)                     // Anforderung des Benutzers
      {
        Serial.println(F("an              "));
        digitalWrite(relais3, RelaisAn);      // Heizlüfter anschalten
      }
      else
      {                                       // keine Anforderung falls keine vorherige Bedingung zutrifft
        Serial.println(F("aus             "));
        digitalWrite(relais3, RelaisAus);     // Heizlüfter anschalten
      }
    }
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
  if (now1.year() == 2020)                      // 2020
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat20[i];
      FerienTag[i]=FerienTag20[i];
    }
  }
  if (now1.year() == 2021)                      // 2021
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat21[i];
      FerienTag[i]=FerienTag21[i];
    }
  }
  if (now1.year() == 2022)                      // 2022
  {
    for(byte i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat22[i];
      FerienTag[i]=FerienTag22[i];
    }
  }
  if (now1.year() == 2023)                      // 2023
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
  Serial.write(27);      // ESC
  Serial.print("[1;1H"); // cursor to home
} 

void Anforderung()       // Interruptservice Routine
{
  tasterLow = 1;         // Tasterbetätigung merken, Hauptprogramm frägt den Status ab und löscht ihn nach der Bearbeitung
}


