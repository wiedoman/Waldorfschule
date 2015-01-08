#include <Wire.h>         

#define RTC_Address   0x32  //RTC_Address 

unsigned char   date[7];

void setup()
{
  Wire.begin();
  Serial.begin(19200); 
}

void loop()
{
  I2CWriteDate();//Write the Real-time Clock
  delay(100);

  while(1)
  {  
    I2CReadDate();  //Read the Real-time Clock     
    Data_process();//Process the data

    delay(1000);//延时1S
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

  //Wire.beginTransmission(RTC_Address);        
  //Wire.write(byte(0));//Set the address for writing       
  //Wire.write(0x00);//second:59     
  //Wire.write(0x54);//minute:1      
  //Wire.write(0x92);//hour:15:00(24-hour format Bit Nr.7)         
  //Wire.write(0x07);//weekday:Wednesday      
  //Wire.write(0x01);//day:26th      
  //Wire.write(0x06);//month:December     
  //Wire.write(0x14);//year:2012      
  //Wire.endTransmission();

  //Wire.beginTransmission(RTC_Address);      
  //Wire.write(0x12);   //Set the address for writing         
  //Wire.write(byte(0));            
  //Wire.endTransmission(); 

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
    }
  }

  Serial.print("   Zeit: ");          //Uhrzeit
  if (date[2]<10) Serial.print("0");
  Serial.print(date[2]);
  Serial.print(":");
  if (date[1]<10) Serial.print("0");
  Serial.print(date[1]);
  Serial.print(":");
  if (date[0]<10) Serial.print("0");
  Serial.print(date[0]);

  Serial.print("   Datum: ");        //Datum
  if (date[4]<10) Serial.print("0");
  Serial.print(date[4]);
  Serial.print(".");
  if (date[5]<10) Serial.print("0");
  Serial.print(date[5]);
  Serial.print(".");
  if (date[6]<10) Serial.print("0");
  Serial.print(date[6]);

  Serial.println();
}

