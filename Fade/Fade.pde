/*
 Fade
 
 This example shows how to fade an LED on pin 9
 using the analogWrite() function.
 
 This example code is in the public domain.
 
 */
int zwerg;
char Zyklus = 10;

#define Relais1 33
#define Relais2 35
#define Relais3 39
#define Relais4 41
#define Relais5 43
#define Relais6 47
#define Relais7 51
#define Relais8 53

#define RelaisA 49
#define RelaisB 23
#define RelaisC 25
#define RelaisD 27
#define RelaisE 29
#define RelaisF 31
#define RelaisG 37
#define RelaisH 45

#define Reed1 22
#define Reed2 24
#define Reed3 26
#define Reed4 28
#define Reed5 30
#define Reed6 32
#define Reed7 34
#define Reed8 36

#define ReedA 38
#define ReedB 40
#define ReedC 42
#define ReedD 44
#define ReedE 46
#define ReedF 48
#define ReedG 50
#define ReedH 52

bool Felder[8][8];

void setup()  
{ 
  Serial.begin(57600);

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

  pinMode(Reed1, INPUT);
  pinMode(Reed2, INPUT);
  pinMode(Reed3, INPUT);
  pinMode(Reed4, INPUT);
  pinMode(Reed5, INPUT);
  pinMode(Reed6, INPUT);
  pinMode(Reed7, INPUT);
  pinMode(Reed8, INPUT);

  pinMode(ReedA, OUTPUT);
  pinMode(ReedB, OUTPUT);
  pinMode(ReedC, OUTPUT);
  pinMode(ReedD, OUTPUT);
  pinMode(ReedE, OUTPUT);
  pinMode(ReedF, OUTPUT);
  pinMode(ReedG, OUTPUT);
  pinMode(ReedH, OUTPUT);
  
  for (char i = 7; i; i--)
  {
    for (char n = 7; n; n--)
    {
      Felder[i][n] = 0;
    }
  }
} 

void loop()  
{
  // Startaufstellung ermitteln
  
  
  // Ausgabe des Feldinhalts
  Zyklus--;
  if (!Zyklus)
  {
    Serial.println("  ABCDEFGH");
    for (char i = 8; i; i--)
    {
      Serial.print(i,0);Serial.print(" ");
      Serial.print(Felder[0][i-1]);
      Serial.print(Felder[1][i-1]);
      Serial.print(Felder[2][i-1]);
      Serial.print(Felder[3][i-1]);
      Serial.print(Felder[4][i-1]);
      Serial.print(Felder[5][i-1]);
      Serial.print(Felder[6][i-1]);
      Serial.println(Felder[7][i-1]);
    }
    Serial.println("  ABCDEFGH");
    Zyklus = 10;
  } 
  delay(100);
}
