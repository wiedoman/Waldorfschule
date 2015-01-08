#include <NixieTube.h>

/* 
 Schulglocke Waldorfschule Geislingen V0.1
 Lars Danielis

 Nixie Module for Arduino V2.0.0
 Hardware Designer: Yan Zeyuan
 Blog: http://nixieclock.org
 Email: yanzeyuan@163.com
 Library Author: Weihong Guan (aGuegu)
 Blog: http://aguegu.net
 Email: weihong.guan@gmail.com
 
 EchtzeitUhr
 RTC SD2405 V1.0
 
 MP3 Player
 DFRduino Player V2.1 10/10/2013 for .NET Gadgeteer 
*/

#include "NixieTube.h"
#include <Wire.h>         

#define RTC_Address   0x32  //RTC_Address 

unsigned char date[7];
int Stunde,Minute,Sekunde,Wochentag,Tag,Monat,Jahr,Blink;
char Unterricht;
Color Farbe, AlteFarbe;
char Gong;
String Unterrichtsstunde,Ferienausgabe;
int StundeX[20]={ 7, 8, 9, 9,10,10,10,11,11,12,12,13,13,14,14,15,15,15,15,16};
int MinuteX[20]={55, 0,40,55, 0,45,50,35,45,30,35,20,25,10,15,00,05,50,55,40};
int FerienTag14[20]    = { 1, 6, 3, 7,14,25, 1, 2,29,30, 9,20,31,12, 8, 9,27,31,22,31};
int FerienMonat14[20]  = { 1, 1, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 9,10,10,10,10,12,12};
int FerienTag15[20]    = { 1, 6,16,20,30,10, 1, 2,14,15,25, 5,30,11, 3, 4, 2, 6,23,31};
int FerienMonat15[20]  = { 1, 1, 2, 2, 3, 4, 5, 5, 5, 5, 5, 6, 7, 9,10,10,11,11,12,12};
int FerienTag16[20]    = { 1, 8, 8,12,25, 1,30, 1, 5, 6,16,27,28, 9, 2, 3,31, 4,23,31};
int FerienMonat16[20]  = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 5, 5, 5, 7, 9,10,10,10,11,12,12};
int FerienTag17[20]    = { 1, 6,27, 3,10,21,30, 1,25,26, 5,16,27, 8, 2, 3,30, 3,25,31};
int FerienMonat17[20]  = { 1, 1, 2, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 9,10,10,10,11,12,12};
int FerienTag[20],FerienMonat[20];
int Auswertung[20],AuswertungFerien[20];

#define Einfach 0
#define Doppelt 1

int button = 3;//button in digital 3

#define COUNT	4 // define how many modules in serial
NixieTube tube(11, 12, 13, 10, COUNT); // pin_ds, pin_st. pin_sh, pin_oe(pwm pin is preferred), COUNT

void setup()
{
  Wire.begin();
  Serial.begin(19200);

  pinMode(3,INPUT);
  Farbe = Black;
  FarbeSetzen();
  tube.setBrightness(0x90);	// brightness control, 0x00(off)-0xff
  tube.display();

//  I2CWriteDate();//Write the Real-time Clock
  delay(100);
  WriteTimeOff();      
}

void loop()
{
 
  RTCAuslesen();

  if (digitalRead(3) == HIGH) // Test für Gong
  {
    Serial.print(" Gong");
    GongSchlagen(Einfach);     
  }
  
  if (Wochentag == 0 || Wochentag == 6)
  {
    Serial.println(" Wochenende");
    Farbe = Blue;
    FarbeSetzen();
  }
  else if (Ferien())
  {
    Serial.println(Ferienausgabe);
    Farbe = Black;
    FarbeSetzen();
  } 
  else 
  {
    Serial.print(" Unterricht ");
    Farbe = Green ;
    FarbeSetzen();
    Stundenplan();
  }

  delay(1000);     //1S

  AusgabeAnzeige();
 
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
  Wire.write(0x10);//second
  Wire.write(0x01);//minute
  Wire.write(0xa2);//hour(24-hour format Bit Nr.7)         
  Wire.write(0x00);//weekday
  Wire.write(0x08);//day
  Wire.write(0x06);//month
  Wire.write(0x14);//year
  Wire.endTransmission();

  Wire.beginTransmission(RTC_Address);      
  Wire.write(0x12);   //Set the address for writing         
  Wire.write(byte(0));            
  Wire.endTransmission(); 
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
void RTCAuslesen(void)
{
  unsigned char i;
  I2CReadDate();   //Read the Real-time Clock     

  for(i=0;i<7;i++)
  {
    if(i!=2)
      date[i]=(((date[i]&0xf0)>>4)*10)+(date[i]&0x0f);
    else
    {
      date[2]=(date[2]&0x7f);
      date[2]=(((date[2]&0xf0)>>4)*10)+(date[2]&0x0f);
    }
  }

  Stunde    = date[2];
  Minute    = date[1];
  Sekunde   = date[0];
  Tag       = date[4];
  Monat     = date[5];
  Jahr      = date[6];
  Wochentag = date[3];

  if (Sommerzeit()) 
  {
    Stunde++; // während Sommerzeit +1 Stunde
    if (Stunde == 24) Stunde = 0;
    Serial.print("Sommerzeit ");
  }
  else
  {
    Serial.print("Winterzeit ");
  }

  if (date[3] == 0) Serial.print("Sonntag    ");
  if (date[3] == 1) Serial.print("Montag     ");
  if (date[3] == 2) Serial.print("Dienstag   ");
  if (date[3] == 3) Serial.print("Mittwoch   ");
  if (date[3] == 4) Serial.print("Donnerstag ");
  if (date[3] == 5) Serial.print("Freitag    ");
  if (date[3] == 6) Serial.print("Samstag    ");

  if (Tag<10) Serial.print("0");
  Serial.print(Tag);
  Serial.print(".");
  if (Monat<10) Serial.print("0");
  Serial.print(Monat);
  Serial.print(".");
  if (Jahr<10) Serial.print("0");
  Serial.print(Jahr);
  Serial.print(" ");
  if (date[2]<10) Serial.print("0");
  Serial.print(date[2]);
  Serial.print(": ");
  if (Stunde<10) Serial.print("0");
  Serial.print(Stunde);
  Serial.print(":");
  if (Minute<10) Serial.print("0");
  Serial.print(Minute);
  Serial.print(":");
  if (Sekunde<10) Serial.print("0");
  Serial.print(Sekunde);
}


unsigned char Sommerzeit(void)
{
  if ((Monat < 3) || (Monat > 10)) return 0;
  if (((Tag - Wochentag) >= 25) && (Wochentag || (Stunde >= 2)))
  {
    if (Monat == 10 ) return 0;
  }
  else
  {
    if (Monat == 3) return 0;
  }
  return 1;
}

void GongSchlagen(int Anzahl)
{
  if(Anzahl == Einfach)
  {
    Serial.print(" ++ Gong einfach ++ ");
    Serial.println();
    Serial.println("\\:v 245"); // set the volume, from 0 (minimum)-255 (maximum)
    delay(50);
    Serial.println("\\:n");  // Gong abspielen
    GongAbwarten();
    Serial.println("\\:v 0"); // set the volume, from 0 (minimum)-255 (maximum)
    delay(50);
  }
  else
  {
    Serial.print(" ++ Gong doppelt ++ ");
    Serial.println();
    Serial.println("\\:v 245"); // set the volume, from 0 (minimum)-255 (maximum)
    delay(50);
    Serial.println("\\:n");  // Gong abspielen
    GongAbwarten();
    Serial.println("\\:n");  // Gong abspielen
    GongAbwarten();
    Serial.println("\\:v 0"); // set the volume, from 0 (minimum)-255 (maximum)
    delay(50);
  }
}

/*
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
int Ferien(void)
{
  int b;
  if (Jahr == 14)
  {
    for(int i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat14[i];
      FerienTag[i]=FerienTag14[i];
    }
  }
  if (Jahr == 15)
  {
    for(int i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat15[i];
      FerienTag[i]=FerienTag15[i];
    }
  }
  if (Jahr == 16)
  {
    for(int i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat16[i];
      FerienTag[i]=FerienTag16[i];
    }
  }
  if (Jahr == 17)
  {
    for(int i=0;i<20;i++)
    {
      FerienMonat[i]=FerienMonat17[i];
      FerienTag[i]=FerienTag17[i];
    }
  }
  for(int i=0;i<20;i++)
  {
    if(Monat > FerienMonat[i]) // Monat schon vorbei, dann 1
    {
     AuswertungFerien[i] = 1;
    }
    else if(i%2 && Monat == FerienMonat[i] && Tag > FerienTag[i]) //Ferienanfang (gerade)
    {
      AuswertungFerien[i] = 1;
    }
    else if(!(i%2) && Monat == FerienMonat[i] && Tag >= FerienTag[i]) //Ferienende (ungerade)
    {
      AuswertungFerien[i] = 1;
    }
    else 
    {
      AuswertungFerien[i] = 0;
    }
   
    b=i-1; 
    if (i && (AuswertungFerien[i] < AuswertungFerien[b]))
    {
      if (b==0) 
      {
        Ferienausgabe = String(" Winterferien");
        return 1;
      }
      if (b==2)
      {
        Ferienausgabe = String(" Faschingsferien");
        return 1;
      }
      if (b==4) 
      {
        Ferienausgabe = String(" Osterferien");
        return 1;
      }
      if (b==6) 
      {
        Ferienausgabe = String(" 1. Mai");
        return 1;
      }
      if (b==8) 
      {
        Ferienausgabe = String(" Himmelfahrt");
        return 1;
      }
      if (b==10) 
      {
        Ferienausgabe = String(" Pfingstferien");
        return 1;
      }
      if (b==12) 
      {
        Ferienausgabe = String(" Sommerferien");
        return 1;
      }
      if (b==14) 
      {
        Ferienausgabe = String(" Tag der deutschen Einheit");
        return 1;
      }
      if (b==16) 
      {
        Ferienausgabe = String(" Herbstferien");
        return 1;
      }
      if (b==18) 
      {
        Ferienausgabe = String(" Winterferien");
        return 1;
      }
    }
  }
  return 0;
}

/*
         7:55			0   
 8:00 –  9:40   Hauptunterricht	1,2
 9:55				3
10:00 – 10:45   1. Fachstunde   4,5
10:50 – 11:35   2. Fachstunde   6,7
11:45 – 12:30   3. Fachstunde   8,9
12:35 – 13:20   4. Fachstunde  10,11
13:25 – 14:10   5. Fachstunde  12,13
14:15 – 15:00   6. Fachstunde  14,15
15:05 – 15:50   7. Fachstunde  16,16
15:55 – 16:40   8. Fachstunde  18,19
*/
char Stundenplan(void)
{
  int b,c;
  for(int i=0;i<20;i++)
  {
    if(Stunde > StundeX[i]) 
    {
      Auswertung[i] = 1;
    }
    else if(Stunde == StundeX[i] && Minute >= MinuteX[i]) 
    { 
      Auswertung[i] = 1; 
      if(Minute == MinuteX[i] && Sekunde < 10)
      {
        if(i==1 || i==4) GongSchlagen(Doppelt); else GongSchlagen(Einfach);
      }
    }
    else Auswertung[i] = 0;
   
    b=i-1; 
    if (i && (Auswertung[i] < Auswertung[b]))
    {
      c=((b-4)/2+1);
      if (b==0) 
      {
        Unterrichtsstunde = String("Ankuendigung Hauptunterricht");
        Farbe = Green;
        FarbeSetzen();
      }
      if (b==1) 
      {
        Unterrichtsstunde = String("Hauptunterricht");
        Farbe = Magenta;
        FarbeSetzen();
      }
      if (b==2) 
      {
        Unterrichtsstunde = String("Grosse Pause");
        Farbe = Green;
        FarbeSetzen();
      }
      if (b==3) 
      {
        Unterrichtsstunde = String("Ankuendigung 1. Fachstunde");
        Farbe = Green;
        FarbeSetzen();
      }
      if (b>3 && b%2) 
      {
        Unterrichtsstunde = String( String(c) + ". Pause");
        Farbe = Green;
        FarbeSetzen();
      }
      if (b>4 && !(b%2))
      {
        Unterrichtsstunde = String( String(c) + ". Fachstunde");
        Farbe = Magenta;
        FarbeSetzen();
      }
    }
  }
  
  Serial.println(Unterrichtsstunde);
  
  return 0;
}


void AusgabeAnzeige(void)
{
  if ((Sekunde==10) || (Sekunde==30) || (Sekunde==50)) 
  {
    Farbe = White;
    tube.printf("%02d'%02d'", date[4], date[5]);  //Datum
    FarbeSetzen();
    delay(5000);     //5S das DAtum anzeigen
  }
  
  if(Blink) Blink=0; else Blink = 1;
  if(Blink)
  {
    tube.printf("%02d:%02d", Stunde, Minute);  //Uhrzeit
  }
  else
  {
    tube.printf("%02d%02d", Stunde, Minute);  //Uhrzeit
  }
  tube.display();
}

void Blinken(void)
{
    if(Blink) Blink=0; else Blink = 1;
    if(Blink)
    {
      tube.printf("%02d:%02d", Stunde, Minute);  //Uhrzeit
    }
    else
    {
      tube.printf("%02d%02d", Stunde, Minute);  //Uhrzeit
    }
}

void GongAbwarten(void)
{
  AlteFarbe=Farbe;
  for(int i=0;i<20;i++)
  {
    delay(1000);
    Blinken();
    if(Blink) Farbe = Blue; else Farbe = Magenta;
    FarbeSetzen();
  }
  Farbe = AlteFarbe;
}

void FarbeSetzen(void)
{
  tube.setBackgroundColor(1, Farbe); //Farbe
  tube.setBackgroundColor(2, Farbe); //Farbe
  tube.setBackgroundColor(3, Farbe); //Farbe
  tube.setBackgroundColor(4, Farbe); //Farbe
  tube.display();
}
