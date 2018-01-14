char Version[] = "V3.7";
// V3.7a,
//    Temperatursensorausfall schaltet failsafe auf Frostschutz, 4°C
//    Uhrausfall failsafe Unterricht und blaue LED blinkt
// V3.6,
//    Lüftervorheizen ging in dem Fall Minute >= 45 verloren
// V3.5,
//    Vorheizen reduziert. Anpassung an höhere Heizleistung
//    Code umstrukturiert und besser kommentiert
//    Frostschutzhysterese fehlte
//    Stufenschaltung mehrfach rechnen damit im ersten Schritt die endgültige Heizstufe ausgegeben wird
//    Delay der Relaisschaltung entfernt
// V3.4, 
//    Tasterdelay = 100ms wg. Störungen im Kasten und langem Kabel
//    Temperaturprüfung und Wert einfrieren wenn Temperatursensorwert unplausibel, 
//    Init auf 22 falls der Temperatursensor noch nie einen plausiblen Wert geliefert hat
//    1°C Hysterese bei den Übergängen aller Heizungsstufen 
//    Ist die Temperatur beim Anfordern des 2h heizens kleiner 17°C wird auch gleich der Lüfter mit eingeschaltet
//    Bessere Simulationsmöglichkeit durch Vorgabe der Isttemperatur per Serial.Read
//      +, bzw. +++, oder -, bzw. -----, erhöht bzw. reduziert die Isttemperatur um 0,1°C per Zeichen und bleibt bis ein * gesendet wird
//      Die Zeitfenster für Lüften, Heizstufenerhöhung usw. können manipuliert werden und werden mit einem * wieder zurückgesetzt
//        lxx = Zeitfenster für die Heizungsabschaltung für die Lüftungserkennung auf xx Sekunden setzen
//        dxx = Deltatemperatur für die Erkennung eines offenen Fensters
//        hxx = Zeitfenster für die Heizstufenerhöhung auf xx Sekunden setzen
//        ixx = Isttemperatur auf xx°C setzen, nur ganzzahlige Werte
//        fxx = Zeitfernster für Heizanforderung
//        jxxxx = Jahr angeben für Zeiteinstellung mit dem Befehl 'z'
//        mxx = Monat angeben für Zeiteinstellung mit dem Befehl 'z'
//        txx = Tag angeben für Zeiteinstellung mit dem Befehl 'z'
//        sxx = Stunde angeben für Zeiteinstellung mit dem Befehl 'z'
//        nxx = Minute angeben für Zeiteinstellung mit dem Befehl 'z'
//        exx = Sekunde angeben für Zeiteinstellung mit dem Befehl 'z'
//        z   = angegebene Zeit und Datum aus letzter Zeila an RTC schicken
//        *   = Zurücksetzen der Werte

// Heizung ist auf 22° eingestellt
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

#define SERbaud     115200            // Baudrate der Seriellen Schnittstelle
#define HYSadd       0                // Hysteresetemperatur in °C für die Erhöhung der Heizstufe
#define HYSsub      -2                // Hysteresetemperatur in °C für die Senkung der Heizstufe
#define HYSplus     -1                // Hysteresetemperatur in °C für ds Löschen der Heizstufenerhöhung
#define ZLUEFTbeob  30                // Zeitfenster für das Beobachten der Isttemperatur zur Erkennung eines offenen Fensters
#define ZLUEFT      600               // Zeitfenster für Lüftbetrieb, währenddessen die Heizungen ausgeschaltet werden
#define LUEFTtemp    1                // Temperatur um die sich die Isttemperatur senken muss um ein offenes Fenster zu erkennen
#define TLUEFTinit  -100              // Initialisierungstemperatur für offene Fenster Erkennung
#define TLUEFTERvor 15                // Temperatur unter der der Heizlüfter bei der 2h Heizanforderung mit eingeschalter wird
#define ENTPRELLlow 10                // Entprellzeit für die Low Erkennung des Taster in ms
#define ENTPRELL    100               // Allgemeine Entprellung von 100ms
#define oneSECOND   1000              // Allgemeine Entprellung während der Initialisierung
#define twoSECONDS  2000              // Zeit für die Erkennung der Lüfterschaltanforderung
#define sollTEMP    22                // Grundsolltemperatur
#define ZHEIZ       7200              // Zeitfenster für Heizanforderung
#define ZSTUFE      600               // Zeitfenster für die Heizstufenüberwachung nach der falls Stufe < 3 und die Zieltemperatur nicht erreicht wurde die Stufe wieder erhöht wird
#define ZLED        300               // LED Blinkdelay
#define TFROST      5                 // Temperaturschwelle für Frostschutz

OneWire oneWire(ONE_WIRE_BUS);        // OneWire Netzwerk erstellen
DallasTemperature sensor(&oneWire);   // Sensor mit OneWire verbinden
DeviceAddress insideThermometer;      // Adressspeicher des Sensors

float temperatur=sollTEMP,SollTemperatur,lueftTemperatur=-100,regelAbweichung,vTIST;    // Temperaturen
signed char DIP=0;                                              // DIP Schalter für Temperaturkorrekturen
int relAn=0,SZ=0,heizungsStufe=0,heizungsStufePlus=0,relAnAlt=0;
uint32_t HeizanforderungsEnde=0, intervallZeit=0, lueftenBeginn, stufenReduktion, anforderungAnZeit, anforderungAusZeit;    // Zeitstempel 
uint8_t tasterAlt=true,tasterLow=false,luefterVorheizen=false,heizungsLuefterAnforderung=false,heizungsLuefter=false,angefordert=false,Unterricht=false,lueften=false,blinken=false,sehrKalt=0,inByte,frostSchutz=false,failSafeTemp=false,failSafeTime=false;
unsigned long loopzeit;                                         // millis() Ergbenis

uint8_t vTFROST; // Debugwerte
int vZLUEFT,vZSTUFE,vZHEIZ,vJahr,vMonat,vTag,vStunde,vMinute,vSekunde,vLUEFTtemp; // Debugwerte

RTC_DS1307 RTC;                       // RTC Object

#define RelaisAn  LOW                 // Relais Massegesteuert
#define RelaisAus HIGH
#define relais1 A0                    // Relais für 750W Ansteuerung
#define relais2 A1                    // Relais für 1250W Ansteuerung (Rel 1+2 = 2000W)
#define relais3 A2                    // Relais für Lüfteransteuerung in den Heizungen
#define dip1     9                    // Ports der 6 DIP Schalter
#define dip2     8
#define dip3     7
#define dip4     6
#define dip5     5
#define dip6     4
#define LEDblau  11                    // mittlerweise blaue LED am externen Bedienteil für Heizanforderungsbestätigung
#define LEDgelb 12                    // gelbe LED an der Steuerung für die Anzeige ob mindestens ein Releis angesteuert wurde, blinken = geöffnetes Fenster erkannt
#define Taster   2                    // Taster wg. Interruptsteuerung an PIN 2

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

void initValues(void);                  // Startwerte setzen ober Debugwerte zurücksetzen
float getValue(void);                   // Floatwert von dem seriellen Terminal holen und umwandeln
void Anforderung(void);                 // Interruptserviceroutine für Taster
void clearAndHome(uint8_t x, uint8_t y);// Cursor auf dem seriellen Terminal positionieren
boolean Sommerzeit(void);               // Prüfung auf Sommerzeit, das RTC Modul liefert immmer die Zeit in Winterzeit
byte Ferien(void);                      // Prüfung ob das aktuelle Datum innerhalb der Ferien ist
void serialDebug(void);                 // Prüfung ob am seriellen Terminal eingaben gemacht wurden
void taster(void);                      // Tasterauswertung, ob Heizanforderung oder Lüfterschaltung angefordert

void setup(void)
{
  pinMode(relais1, OUTPUT);            // Relais an Pin A0 
  digitalWrite(relais1, RelaisAus);
  pinMode(relais2, OUTPUT);            // Relais an Pin A1
  digitalWrite(relais2, RelaisAus);
  pinMode(relais3, OUTPUT);            // Relais an Pin A2
  digitalWrite(relais3, RelaisAus);
  pinMode(LEDblau, OUTPUT);             // rot LED an Pin 11
  digitalWrite(LEDblau, LOW);
  pinMode(LEDgelb, OUTPUT);            // gelb LED an Pin 12
  digitalWrite(LEDgelb, LOW);
  pinMode(dip1,   INPUT_PULLUP);       // DIP 1-6 an Pins 9-4
  pinMode(dip2,   INPUT_PULLUP);
  pinMode(dip3,   INPUT_PULLUP);
  pinMode(dip4,   INPUT_PULLUP);
  pinMode(dip5,   INPUT_PULLUP);
  pinMode(dip6,   INPUT_PULLUP);
  pinMode(Taster, INPUT_PULLUP);       // Taster an Pin 2
  Serial.begin(SERbaud);               // Für die Ausgabe auf dem Terminal
  sensor.begin();                      // Sensor Bibliothek starten
  sensor.getAddress(insideThermometer, 0); // Abdresse des Sensors ermitteln
  Wire.begin();                        // I2C anschalten
  RTC.begin();                         // Kommunikation zur RTC beginnen
  delay(oneSECOND);                    // Notwendiges delay wg. RTC
  sensor.requestTemperatures();        // onewire Protokoll für Dallas DS1820 Temperatursensor
  delay(oneSECOND);                    // Notwendiges delay wg. onewire
  loopzeit = millis();                 // Startzeit merken 
  attachInterrupt(digitalPinToInterrupt(Taster), Anforderung, FALLING); // Taster an PIN 2, löst Interruptgesteuert Unterfunktion Anforderung() aus
  // ### RTC.adjust(DateTime(__DATE__,__TIME__));
  // ### Uhr stellen - 2mal flashen, einmal mit der Zeit (im Sommer -1h von der aktuellen Zeit weil der Code +1 macht
  // ### danach unbedingt noch ein zweites mal flashen mit auskommentierter Zeile zum Zeit stellen
  // ### damit die RTC frei läuft
  // RTC.adjust(DateTime(/*J*/2017,/*M*/8,/*T*/1,/*H*/17,/*M*/40,/*S*/30));// Sommerzeit !! im Sommer -1h weil der Code +1 macht
  initValues(); // Startwerte setzen
}

void loop(void)
{ 
  serialDebug();                                // Prüfen ob eine Debugeingabe auf dem seriellen Terminal vorliegt
  if(tasterLow)                                 // Taster gedrückt? Für die Erkennung der 2s Betätigung für die Lüfteransteuerung in den Heizungen
  {
    taster();                                   // Taster auswerten
  }
  if (millis() - loopzeit > oneSECOND)          // Hauptprogramm einmal pro Sekunde bzw. nach Ablauf einer Sekunde berechnen
  {
    clearAndHome(0,0);                          // Terminalfenster Cursorsteuerung auf Pos 0,0, oben links
    Serial.println(Version);                    // Programmversion ausgeben
    loopzeit = millis();                        // Zeitstempel für nächtste Programmerechnung, siehe oben, 1 Sekunde
    
    // ++++++++++++++++++++++++++++++ Zeit
    DateTime now = RTC.now();                   // Zeit vom RTC holen
    SZ = Sommerzeit();                          // Prüfen auf Sommerzeit
    if (SZ) now = (now.unixtime() + 3600);      // Wenn Sommerzeit, dann plus eine Stunde 
    Serial.print(F("               Zeit = "));     // und Datum und die Uhrzeit ausgeben
    Serial.print(now.day(), DEC);               
    Serial.print(F("."));
    Serial.print(now.month(), DEC);
    Serial.print(F("."));
    Serial.print(now.year(), DEC); 
    if (now.year() < 2017) failSafeTime = true; else failSafeTime = false;     
    Serial.print(F(" "));
    Serial.print(now.hour(), DEC);
    Serial.print(F(":"));
    Serial.print(now.minute(), DEC);
    Serial.print(F(":"));
    Serial.print(now.second(), DEC);
    if (SZ) Serial.print(F(" Sommerzeit")); else Serial.print(F(" Winterzeit"));Serial.println(F("              ")); 
    Serial.print(F("          Wochentag = ")); // auch den Wochentag
    Serial.print(now.dayOfTheWeek());
    Serial.print(F(" = "));
    if(now.dayOfTheWeek() == 0) Serial.println(F("Sonntag         "));
    if(now.dayOfTheWeek() == 1) Serial.println(F("Montag          "));
    if(now.dayOfTheWeek() == 2) Serial.println(F("Dienstag        "));
    if(now.dayOfTheWeek() == 3) Serial.println(F("Mittwoch        "));
    if(now.dayOfTheWeek() == 4) Serial.println(F("Donnerstag      "));
    if(now.dayOfTheWeek() == 5) Serial.println(F("Freitag         "));
    if(now.dayOfTheWeek() == 6) Serial.println(F("Samstag         "));
    Serial.print(F("          UNIX-Zeit = ")); // und die Zeit im UNIX Format
    Serial.print(now.unixtime());Serial.println(F("s              "));
  
    // ++++++++++++++++++++++++++++++ Solltemperatur
    Serial.print(F("   Normaltemperatur = ")); // Normaltemperatur ausgeben 
    Serial.print(sollTEMP);Serial.println(F("°C "));
    
    // ++++++++++++++++++++++++++++++ DIP Schalter auswerten
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
    Serial.print(DIP);Serial.println(F("°C              "));
    
    // ++++++++++++++++++++++++++++++ Solltemperatur Korrektur
    SollTemperatur = sollTEMP;                 // Solltemperatur (StartWert 22°)
    SollTemperatur += DIP;                     // Korrekturwert aus DIP addieren
    Serial.print(F("     Solltemperatur = ")); // und resultierende Solltemperatur ausgeben
    Serial.print(SollTemperatur);Serial.println(F("°C "));

    // ++++++++++++++++++++++++++++++ Isttemperatur
    sensor.requestTemperatures();                        // Temperatur von Sensor holen
    if (sensor.getTempC(insideThermometer) < TLUEFTinit) // Ist der Sensor fehlerhaft, dann ist der Wert -127°C
    {
      temperatur = 4;                                    // Frostschutz failsafe
      failSafeTemp = true;
      Serial.print(F("      Isttemperatur = "));         // und ausgeben
      Serial.print(temperatur);Serial.println(F("°C Frostschutz wg. fehlerhaftem Sensor")); 
    }
    else
    {
      temperatur = sensor.getTempC(insideThermometer);  // bei nicht fehlerhaftem Sensorwert wird er übernommen
      if (vTIST) temperatur = vTIST;                    // DEBUG
      failSafeTemp = false;
      Serial.print(F("      Isttemperatur = "));        // und ausgegeben
      Serial.print(temperatur);Serial.println(F("°C                                 "));
    }

    // ++++++++++++++++++++++++++++++ Luefttemperatur mit Isttemperatur initialisieren
    if(lueftTemperatur == TLUEFTinit) lueftTemperatur = temperatur;  // Initwert der Lüfttemperatur für die Erkennung eines offenen Fensters
  
    // ++++++++++++++++++++++++++++++ Prüfung auf offenes Fenster und Lüften starten, währenddessen werden die Heizungen ausgeschaltet
    if ( (now.unixtime() - intervallZeit) > ZLUEFTbeob){// Innerhalb eines Zeitfensters von 30 Sekunden wird die Temperatur überwacht
      lueftTemperatur = temperatur;                     // Zu Beginn des Zeitfensters wird der Temperaturwert eingefroren 
      intervallZeit = now.unixtime();                   // Zeitstempel für nächstes 30s Fenster 
    }
    Serial.print(F("          Deltazeit = "));          // Zeitfenster
    Serial.print(now.unixtime()-intervallZeit);Serial.println(F("s (max 30s)             "));
    Serial.print(F("    Deltatemperatur = "));          // Temperaturänderung innerhalb des Zeitfensters
    Serial.print(temperatur - lueftTemperatur);Serial.print(F("°C (max -"));Serial.print(vLUEFTtemp);Serial.println(F("°C)             "));
    if(!lueften && ((lueftTemperatur - temperatur) > vLUEFTtemp)) {  // Hat sich die Temperatur innerhalb des Zeitfernsters um 1°C gesenkt
      lueften = true;                                          // Wird Lüften erkannt
      lueftenBeginn = now.unixtime();                          // Zeitstempel für die 10min Lüftungsfunktion, während dessen, die Heizung ausgeschaltet wird
    }
    Serial.print(F("            Lueften = "));                    // und ausgeben
    if(lueften) {                                                 // Ausgabe des Lüftungsstatus
      if(now.unixtime()-lueftenBeginn > vZLUEFT) lueften = false; // Nach Ablauf der 10min wird der Lüftungsstatus zurück genommen
      Serial.print(now.unixtime() - lueftenBeginn);Serial.print(F("s (max "));Serial.print(vZLUEFT);Serial.println(F("s)              "));
    }
    else {
      Serial.println(F("Fenster geschlossen             "));
    }
  
    // ++++++++++++++++++++++++++++++ Heizanforderung vom Taster auswerten
    if(angefordert)                 // Taster gedrückt?
    {                            
      angefordert = false;              // Anforderungsstatus zurücknehmen für nächste Anforderung, dann wird das 2h Fenster neu begonnen
      HeizanforderungsEnde = now.unixtime()+vZHEIZ; // Heizanforderungszeit setzen auf 2 Stunden
      Serial.println(F("             Taster = 1       "));     // Heizanforderungstaste ausgeben
      digitalWrite(LEDblau, false);      // Bestätigung für den Benutzer durch mehrmaliges Blinken der blauen LED
      delay(ENTPRELL);
      digitalWrite(LEDblau, true);
      delay(ENTPRELL);
      digitalWrite(LEDblau, false);
      delay(ENTPRELL);
      digitalWrite(LEDblau, true);
      delay(ENTPRELL);
      digitalWrite(LEDblau, false);
      delay(ENTPRELL);
    }
    else
    {
      Serial.println(F("             Taster = 0          "));     // Heizanforderungstaste ausgeben
    }

    // ++++++++++++++++++++++++++++++ Anforderung für Lüfterschaltung auswerten
    if(heizungsLuefterAnforderung)  // Wurde eine 2s Tasterbetätigung erkannt, dann werden die Lüfter der Heizungen eingeschaltet
    {
      heizungsLuefterAnforderung=false; // Anforderung für die nächste Anforderung löschen
      if(luefterVorheizen)          // Falls der Benutzer die Lüfter nicht angefordert hatte, dann liegt eine Anforderung
      {                             // für die Lüfter vom Vorheizen vor, dann wird die zurück genommen
        luefterVorheizen = false;
      }
      else if(heizungsLuefter)      // Falls keine Anforderung durch das Vorheizen vorlag, dann wird die Anforderung des Benutzer
      {                             // zurück genommen falls eine vorlag
        heizungsLuefter = false; 
      }
      else
      {
        heizungsLuefter = true;        // lag gar keine Anforderung vor, dann werden die Lüfter in den Heizungen eingeschaltet
      }
    }
    
    // ++++++++++++++++++++++++++++++ Prüfung auf Ferien, wenn keine Ferien nachts prüfen wie kalt es ist und entsprechend vorheizen
    // Unterrichtszeit ? Dann heizen von 05:00 bis 17:00, wenn nicht kalt kann sich der Heizbeginn bis 07:00 verzögern
    Serial.print(F("         Auswertung = "));     // Ausgabe Ferien oder Unterricht
    if(now.hour() >= 8) sehrKalt = 0;              // Die Vorheizanforderung wird nach Unterrichtsbeginn zurück genommen
    if(now.hour() == 7 && now.minute() >= 45) luefterVorheizen = false;   // Die Lüfteranforderung durch das Vorheizen wird 15min vor Unterrichtsbeginn zurück genommen
    if(!sehrKalt && now.hour() == 5 && temperatur <  7) {sehrKalt = 3; luefterVorheizen = true; heizungsStufePlus = 2;}
    if(!sehrKalt && now.hour() == 6 && temperatur < 13) {sehrKalt = 2; luefterVorheizen = true; heizungsStufePlus = 1;}
    if(!sehrKalt && now.hour() == 7 && temperatur < 22) {sehrKalt = 1; luefterVorheizen = true; heizungsStufePlus = 0;}
    if (failSafeTemp)
    {
      Serial.println(F("Failsafe Frostschutz Temperatursensor def."));
    }
    else if(failSafeTime)
    {
      Serial.println(F("Failsafe Unterricht Uhr def."));
      Unterricht = true;
      HeizanforderungsEnde = now.unixtime();
    }
    else if(now.dayOfTheWeek() == 0 || now.dayOfTheWeek() == 6)       // Samstag oder Sonntag = Wochenende
    {
      Serial.println(F("Wochenende          "));           // Ausgabe des Status ob Wochende oder Ferien oder Unterricht 
      Unterricht = false;                                  // Nur wenn Unterricht erkannt wird, wird automatisch geheizt, bzw. bei sehr tiefen Temperaturen entsprechend vorgeheizt
    }
    else if(Ferien()) Unterricht = false;                  // Prüfung ob Ferien (siehe Unterfunktion Ferien()) 
    else if(now.hour() >= (8-sehrKalt) && now.hour() < 16) // Heizungsbeginn falls keine der vorherigen Bedingungen eintraf
    {
      Unterricht = true;                                   // automatisches heizen während der Unterrichtszeit 
      Serial.print(F("Unterricht Heizungsbeginn: 0"));     // Heizungsstart ausgeben
      Serial.print(8-sehrKalt);                            // Vorheizzeiz berücksichtigen 
      Serial.println(F(":00 Uhr     "));
    }
    else 
    {
      Unterricht = false;                                // falls nach 16:00 oder vor Vorheizbeginn werden die Heizungen ausgeschaltet
      Serial.println(F("Unterrichtstag aber ausserhalb Unterrichtszeit    "));
    }
                   
    // ++++++++++++++++++++++++++++++ Heizanforderung für zwei Stunden oder Unterricht
    Serial.print(F("  Anforderung Zeit  = "));              // Heizanforderung
    if(HeizanforderungsEnde > now.unixtime() || Unterricht) // Während der Heizanforderung oder Unterrichtszeit
    {
      digitalWrite(LEDblau, HIGH);                          // rote LED an
      if(HeizanforderungsEnde > now.unixtime())             // wenn Anforderung aufgrund Taster
      {                                                     // verbleibende Zeit berechnen      
        int z,h,m;
        z = HeizanforderungsEnde - now.unixtime();
        h = z/3600;
        m = (z/60)-(z/3600*60);
        Serial.print(h);                                    // und ausgeben    
        Serial.print(F(":"));                               
        Serial.print(m);                                    
        Serial.print(F(":"));                               
        Serial.print(z-(3600*h)-(60*m));                    
        Serial.print(F(" (max "));Serial.print(vZHEIZ);Serial.println(F("s)           "));                    
      }
      else
      {
        Serial.println(F("0s           "));                 // sonst 0s ausgeben
      }

      // ++++++++++++++++++++++++++++++ Heizregler
      regelAbweichung = SollTemperatur - temperatur;                      // Temperaturdelta des Reglers (Ist - Soll)
      Serial.print(F("   Reglerabweichung = ")); 
      Serial.print(regelAbweichung); Serial.println(F("°C   "));
      Serial.print(F("    Heizanforderung = Stufe "));                    // Heizanforderungsstufe ausgeben 0=aus, 1=750W, 2=1250W, 3=2000W
      if(regelAbweichung < HYSplus) heizungsStufePlus = 0;                // bei Heizende die Stufenerhöhung zurück nehmenh3
      for (int i=0; i <= 2; i++)                                          // um geklacker der Relais zu vermeiden, mehrfach rechnen
      {                                                                   // damit endgültige Stufe erreicht wird
        if(regelAbweichung - float(heizungsStufe) > HYSadd) heizungsStufe++;// Stufe erhöhen?
        if(regelAbweichung - float(heizungsStufe) < HYSsub) heizungsStufe--;// Stufe erniedrigen?
      }
      Serial.print(F("roh:"));Serial.print(heizungsStufe);
      if(heizungsStufe < 0) heizungsStufe = 0; if(heizungsStufe > 3) heizungsStufe = 3;       // Stufe nur zwichen 0 und 3 erlaubt
      Serial.print(F(", begrenzt:"));Serial.print(heizungsStufe);
      relAn = heizungsStufe + heizungsStufePlus;           // Stufenerhöhung berücksichtigen
      if(relAn < 0) relAn = 0; if(relAn > 3) relAn = 3;    // Stufe nur zwichen 0 und 3 erlaubt
      Serial.print(F(", final:"));
      Serial.print(relAn);Serial.println(F("              "));
      if(relAnAlt > relAn || relAn == 3) stufenReduktion = now.unixtime(); // Nach Stufenreduktion 10min Fenster starten
      if(relAn && (now.unixtime() - stufenReduktion) > vZSTUFE)// wenn die Heizungsstufe in den letzten 10min nicht reduziert wurde, wieder erhöhen
      {
         if (heizungsStufePlus < 2) heizungsStufePlus++;   // Heizstufenerhöhung erhöhen
         stufenReduktion = now.unixtime();                 // 10 min Fenster neu beginnen
      }
      relAnAlt = relAn;                                    // Altwert für die Erkennung der Stüfenänderung 
    }
    else                                                   // wenn keine Heizanforderung 
    {
      digitalWrite(LEDblau, LOW);                           // dann blaue LED aus   
      Serial.println(F("0s           "));                  // Ausgeben
      relAn = 0;                                           // Und Relais aus
      heizungsLuefter = false;luefterVorheizen = false;    // Heizlüfter aus
      heizungsStufePlus = 0;                               // eine evtl. vorhandene Heizstufenerhöhung zurück nehmen
      Serial.println(F("    Heizanforderung = 0                             "));// Heizanforderung ausgeben
    } 
    Serial.print(F(" Heizstufenerhöhung = "));      // Ausgabe der relsutierenden Erhöhung der Heizstufe wg. zu langer Heizdauer
    Serial.print(heizungsStufePlus);Serial.println(F("              "));
    if(!relAn) stufenReduktion = now.unixtime();    // bei ausgeschalteter Heizung läuft das 10min Fenster nicht
    Serial.print(F("seit ltz. Reduktion = "));      // Ausgabe des 10min Zeitfensters
    Serial.print(now.unixtime() - stufenReduktion);Serial.print(F("s (max "));Serial.print(vZSTUFE);Serial.println(F("s)             "));
    
    // ++++++++++++++++++++++++++++++ Blaue LED blinken lassen wenn Uhr defekt
    if (failSafeTime)
    {
      if (blinken)blinken=false;else blinken = true;
      digitalWrite(LEDblau, blinken);
    }

    // ++++++++++++++++++++++++++++++ Heizrelais schalten
    Serial.print(F(" Heizungsrelais 1,2 = "));  // Status der Relais ausgeben und Relais schalten ...
    if(relAn)
    {
      if(lueften){                             // Falls ein offenes Fenster erkannt wurde
        Serial.println(F("aus wegen geoeffnetem Fenster"));
        stufenReduktion = now.unixtime();      // 10min Fenster während dem Lüften zurück setzen
        digitalWrite(relais1, RelaisAus);      // Heizung ausschalten
        digitalWrite(relais2, RelaisAus); 
        if(blinken) blinken = false; else blinken=true; // LED blinken lassen 
        digitalWrite(LEDgelb, blinken);          // gelbe LED aus = Heizungen aus
      }
      else{
        if(relAn == 1)                           // Relais entsprechend der Stufe schalten
        {                                        // Stufe eins 750W Relais 1 an und Relais 2 aus  
          Serial.println(F("1=an, 2=aus, 750W              "));
          digitalWrite(relais1, RelaisAn);       // Heizung anschalten
          digitalWrite(relais2, RelaisAus);       
          digitalWrite(LEDgelb, HIGH);           // gelbe LED an = Heizungen an
        }
        if(relAn == 2)                           // Stufe zwei 1250W Relais 1 aus und Relais 1 an
        {
          Serial.println(F("1=aus, 2=an, 1250W                   "));
          digitalWrite(relais1, RelaisAus);      // Heizung anschalten
          digitalWrite(relais2, RelaisAn);       
          digitalWrite(LEDgelb, HIGH);           // gelbe LED an = Heizungen an
        }
        if(relAn == 3)                           // Stufe drei 2000W Relais 1 und Relais 2 an
        {
          Serial.println(F("1=an, 2=an, 2000W                     "));
          digitalWrite(relais1, RelaisAn);       // Heizung anschalten
          digitalWrite(relais2, RelaisAn);       
          digitalWrite(LEDgelb, HIGH);           // gelbe LED an = Heizungen an
        }
      }
    }

    // ++++++++++++++++++++++++++++++ Frostschutz wenn nicht geheizt wird
    else if (!frostSchutz && temperatur < vTFROST)                // Bei Temperatur unter 5°C wird die Heizung auf Stufe 750W eingeschaltet
    {
      Serial.println(F("1=an, 2=aus, 750W  Frostschutz   "));
      digitalWrite(relais1, RelaisAn);            // Relais 1 an 
      digitalWrite(relais2, RelaisAus);           // Relais 2 aus
      digitalWrite(LEDgelb, HIGH);                // gelbe LED an = Heizungen an
      frostSchutz = true;
    }
    else if (frostSchutz && temperatur > vTFROST + 1)
    {
      frostSchutz = false;
    }

    // ++++++++++++++++++++++++++++++ Relais aus wenn nicht geheizt wird und kein Frostschutz
    else if (!frostSchutz)
    {
      Serial.println(F("aus                                          ")); // Stufe 0 null 0W beide Relais aus
      digitalWrite(relais1, RelaisAus);           // Heizung ausschalten
      digitalWrite(relais2, RelaisAus); 
      digitalWrite(LEDgelb, LOW);                 // gelbe LED aus = Heizungen aus
      heizungsLuefter = false;luefterVorheizen = false;    // Heizlüfter aus
    }
    else // Sonst inherhalb der Frostschutzhysterese
    {
      Serial.println(F("1=an, 2=aus, 750W  Frostschutz   "));
      digitalWrite(relais1, RelaisAn);            // Relais 1 an 
      digitalWrite(relais2, RelaisAus);           // Relais 2 aus
      digitalWrite(LEDgelb, HIGH);                // gelbe LED an = Heizungen an
    }

    // ++++++++++++++++++++++++++++++ Heizlüftrelais schalten
    Serial.print(F("   Heizlüfterrelais = "));// Status der Relais ausgeben und Relais schalten ...
    if(luefterVorheizen)                      // Anforderung der Heizungslüfter aus Vorheizanforderung
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

    // ++++++++++++++++++++++++++++++ Debugausgabe der Zeitverstellung, nach Einstellen der Zait kann sie mit 'z' an den RTC geschickt werden
    Serial.print(F("   Zeit einstellen? = "));// Zeitstring um die Zeit einstellen zu können, an RTC senden mit 'z' + Enter im Terminal
    Serial.print(vTag);Serial.print(F("."));Serial.print(vMonat);Serial.print(F("."));Serial.print(vJahr);Serial.print(F(" "));
    Serial.print(vStunde);Serial.print(F(":"));Serial.print(vMinute);Serial.print(F(":"));Serial.print(vSekunde);Serial.println(F("    "));
    Serial.println(F("                                                       "));
  }
}

/* in dieser Reihenfolge liegen die Feriendaten in zwei Tabellen vor. Eine Tabelle für die Angabe des Tages und die Zweite für den Monat
 Winter    0,1
 Fasching 2,3
 Ostern   4,5
 1.Mai    6,7
 Himmelfahrt  8,9
 Pfingsten      10,11 
 Sommer        12,13
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
    {                                                     // Wenn der Monat noch nicht vorbei ist, dann prüfen ob der Monat in der Tabelle der aktuelle Monat ist und Tag prüfen
      Auswertung[i] = 1;                                  // Dann mit 1 merken
    }
    else if(!(i%2) && now1.month() == FerienMonat[i] && now1.day() >= FerienTag[i]) // Der Tag für das Ferienende ist immer an ungeradadzahligen Stellen in der Tabelle und wird mit berücksichtigt
    {                                                     // Wenn der Monat noch nicht vorbei ist, dann prüfen ob der Monat in der Tabelle der aktuelle Monat ist und Tag prüfen
      Auswertung[i] = 1;                                  // Dann mit 1 merken 
    }
    else                                                  // wenn beides nicht der Fall ist, dann ist es ein noch ausstehendes Datum
    {
      Auswertung[i] = 0;                                  // das dann mit 0 merken                 
    }
    b=i-1; // Vorherige Stelle in Auswertetabelle
    if (i && (Auswertung[i] < Auswertung[b]))             // Gab es in der Tabelle im Vergleich zur alten Stelle einen Übergang von 1 nach 0?
    {                                                     // Wenn ja, dann wurde die Stelle gefunden und jetzt wird geprüft ob es sich um eine Stelle innerhalb der Ferien handelt und um welche
      if (b==0)                                           // An der Stelle 0 = 1 und 1 = 0 handelt es sich um die Winterferien 2. Teil
      {
        Serial.println(F("Winterf."));                    // Prüfung beenden und Ergebnis ausgeben
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
  DateTime now2 = RTC.now();                               // Zeit holen
  if ((now2.month() < 3) || (now2.month() > 10)) return 0; // Wenn nicht innerhalb der Sommerzeitmonate, dann gleich abbrechen
  if ( ((now2.day() - now2.dayOfTheWeek()) >= 25) && (now2.dayOfTheWeek() || (now2.hour() >= 2)) ) // Wenn nach dem letzten Sonntag und nach 2:00 Uhr
  {
    if (now2.month() == 10 ) return 0;                     // dann wenn Oktober, dann gleich abbrechen
  }
  else                                                     // Wenn nicht nach dem letzten Sonntag bzw. vor 2:00 Uhr 
  {
    if (now2.month() == 3) return 0;                       // und es ist März, dann gleich abbrechen
  }
  return 1;        // ansonsten innerhalb der SZMonate, nach dem letzten Sonntag und nicht Oktober, vor dem letzten Sonntag und nicht März, dann ist SZ           
}


void clearAndHome(uint8_t x, uint8_t y)
{
  Serial.write(27);      // ESC
  Serial.print(F("[")); Serial.print(x); Serial.print(F(";")); Serial.print(y); Serial.print(F("H")); 
} 

void Anforderung()       // Interruptservice Routine
{
  tasterLow = 1;         // Tasterbetätigung merken, Hauptprogramm frägt den Status ab und löscht ihn nach der Bearbeitung
}


void initValues()        // Werte zurücksetzen beim Start ober beim Debuggen mit '*'
{
  // SERbaud     115200            // Baudrate der Seriellen Schnittstelle
  // HYSadd       1                // Hysteresetemperatur in °C für dier Erhöhung der Heizstufe
  // HYSsub      -1                // Hysteresetemperatur in °C für dier Senkung der Heizstufe
  // ZLUEFTbeob  30                // Zeitfenster für das Beobachten der Isttemperatur zur Erkennung eines offenen Fensters
  // ZLUEFT      600               // Zeitfenster für Lüftbetrieb, währenddessen die Heizungen ausgeschaltet werden
  // LUEFTtemp    1                // Temperatur um die sich die Isttemperatur senken muss um ein offenes Fenster zu erkennen
  // TLUEFTinit  -100              // Initialisierungstemperatur für offene Fenster Erkennung
  // TLUEFTERvor 17                // Temperatur unter der der Heizlüfter bei der 2h Heizanforderung mit eingeschalter wird
  // ENTPRELLlow 10                // Entprellzeit für die Low Erkennung des Taster in ms
  // ENTPRELL    100               // Allgemeine Entprellung von 100ms
  // oneSECOND   1000              // Allgemeine Entprellung während der Initialisierung
  // twoSECONDS  2000              // Zeit für die Erkennung der Lüfterschaltanforderung
  // sollTEMP    22                // Grundsolltemperatur
  // ZHEIZ       7200              // Zeitfenster für Heizanforderung
  // ZSTUFE      600               // Zeitfenster für die Heizstufenüberwachung nach der falls Stufe < 3 und die Zieltemperatur nicht erreicht wurde die Stufe wieder erhöht wird
  // ZLED        300               // LED Blinkdelay
  // TFROST      5                 // Temperaturschwelle für Frostschutz
  
  // ++++++++++++++++++++++++++++++ Startwerte für die Variablen welche ber Debug verändert werden können
  vTFROST = TFROST;             
  vTIST   = 0;
  vZLUEFT = ZLUEFT;
  vZHEIZ  = ZHEIZ;
  vZSTUFE = ZSTUFE;
  vLUEFTtemp = LUEFTtemp;
  DateTime now = RTC.now();                   // Zeit vom RTC holen
  vJahr=now.year();
  vMonat=now.month();
  vTag=now.day();
  vStunde=now.hour();
  vMinute=now.minute();
  vSekunde=now.second();
}

float getValue() // Eingabe vom Serial Terminal holen, in float umwandeln und zurück geben
{
    uint8_t a=0,b; // a: Komma erkannt, b: Byte vom Terminal
    int d=0,e=10;  // d: Merker für erste Stelle links vom Komma, e: Divisor für Werte rechts vom Komma
    float c=0;     // c: Endergebnis 
    while(Serial.available())
    {
      b = Serial.read();
      if(b==13){} else if(b==44 || b==46) // CR ausblenden, Komma oder Punkt auch und merken
      {
        a=1; // Kommastelle erkannt
      }
      else
      {
        if(a)// rechts vom Komma
        {
          c=c+float(b-48)/e; e=e*10; //Summand vor Addition erst dividieren und Divisor für nächste Division um eine Kommastelle nach rechts erhöhen
        }
        else// links vom Komma
        {
          if(d)c=c*10; d=1; c=c+float(b-48); // Summe erst multipilzieren und dann Summand addieren und Multiplikator um eine Kommastelle nach links erhöhen
        }
      }
    }
    return c; // Summe als float zurückgeben
}

void serialDebug(void)
{
    if(Serial.available())
  {
    clearAndHome(10,58);                      // Terminalfenster Cursorsteuerung auf Pos 0,0, oben links
    Serial.print(F("sim:          "));
    clearAndHome(10,62);
    while(Serial.available()) 
    {
       inByte = Serial.read();
       Serial.print(char(inByte));
       if(inByte==42) initValues();           // * erkannt und Debugwerte löschen
       if(inByte==105)                        // i erkannt und Isttemperatur setzen
       {
        vTIST = getValue();
        Serial.print(vTIST);
       }
       if(inByte==43)                         // + erkannt und Isttemperatur um 0,1°C erhöhen
       {
        vTIST = vTIST + 0.1;
       }
       if(inByte==45)                         // - erkannt und Isttemperatur um 0,1°C senken
       {
        vTIST = vTIST - 0.1;
       }
       if(inByte==108)                        // l erkannt und Zeitfenster für Lüftbetrieb setzen
       {
        vZLUEFT = getValue();
        Serial.print(vZLUEFT);
       }
       if(inByte==100)                        // d erkannt und Deltatemperatur für Erkennung offenes Fenster setzen
       {
        vLUEFTtemp = getValue();
        Serial.print(vLUEFTtemp);
       }
       if(inByte==104)                        // h erkannt und Zeitfenster für Heizstufenschaltung setzen
       {
        vZSTUFE = getValue();
        Serial.print(vZSTUFE);
       }
       if(inByte==102)                        // f erkannt und Zeitfenster für Heizanforderung setzen
       {
        vZHEIZ = getValue();
        Serial.print(vZHEIZ);
       }
       if(inByte==106)                        // j erkannt und Jahr für Zeiteinstellung setzen
       {
        vJahr = getValue();
        Serial.print(vJahr);
       }
       if(inByte==109)                        // m erkannt und Monat für Zeiteinstellung setzen
       {
        vMonat = getValue();
        Serial.print(vMonat);
       }
       if(inByte==116)                        // t erkannt und Tag für Zeiteinstellung setzen
       {
        vTag = getValue();
        Serial.print(vTag);
       }
       if(inByte==115)                        // s erkannt und Stunde für Zeiteinstellung setzen
       {
        vStunde = getValue();
        Serial.print(vStunde);
       }
       if(inByte==110)                        // n erkannt und Minute für Zeiteinstellung setzen
       {
        vMinute = getValue();
        Serial.print(vMinute);
       }
       if(inByte==101)                        // e erkannt und Sekunde für Zeiteinstellung setzen
       {
        vSekunde = getValue();
        Serial.print(vSekunde);
       }
       if(inByte==122)                        // z erkannt, Zeit wird an RTC geschickt
       {
         RTC.adjust(DateTime(/*Jahr*/vJahr,/*Monat*/vMonat,/*Tag*/vTag,/*Stunde*/vStunde,/*Minute*/vMinute,/*Sekunde*/vSekunde));
       }
    }
  } 
}

void taster(void)
{
  clearAndHome(5,0);                          // Terminalfenster Cursorsteuerung auf Pos 0,0, oben links
  tasterLow=false;                            // Trigger aus Interruptserviceroutine löschen
  anforderungAnZeit=millis();                 // Zeitstempel für die 2s Erkennung
  delay(ENTPRELLlow);                         // Tasterentprellung
  while(digitalRead(Taster)==0){delay(ENTPRELL);}// Warten bis der Taster losgelassen wird
  anforderungAusZeit = millis();              // zweiter Zeitstempel für die 2s Erkennung
  Serial.print(F("                                                        an:"));
  Serial.print(anforderungAnZeit);Serial.println(F("ms              "));
  Serial.print(F("                                                        aus:"));
  Serial.print(anforderungAusZeit);Serial.println(F("ms              "));
  Serial.print(F("                                                        diff:"));
  Serial.print(anforderungAusZeit-anforderungAnZeit);Serial.println(F("ms              "));
  
  // ++++++++++++++++++++++++++++++ Taste länger als zwei Sekunden gedrückt: Heizlüfteranforderung setzen
  if(anforderungAusZeit - anforderungAnZeit > twoSECONDS) 
  {
    heizungsLuefterAnforderung = true; 
  }
  // ++++++++++++++++++++++++++++++ Taste zwar entprellt aber kürzer als zwei Sekunden, Heizanforderung für zwei Stunden setzen
  else if(anforderungAusZeit - anforderungAnZeit > ENTPRELL) 
  {
    angefordert = true;
    if(temperatur < TLUEFTERvor && !luefterVorheizen) luefterVorheizen = true;// Ist die Temperatur beim Anfordern des 2h heizens, dann auch den Lüfter anschalten.    
  }
}

