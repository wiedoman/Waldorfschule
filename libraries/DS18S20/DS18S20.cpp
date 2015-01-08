/*
By R. Plag, December 2009

This file implements the class DS18S20_List which provides easy access
to a set of DS18S20 1-Wire temperature sensors. The class is based
on the OneWire library by Jim Studt and Tom Pollard (see OneWire.cpp). 

Usage is explained in ds18s20_demo.pde.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <DS18S20.h>
#include <Arduino.h>


void print_temperature(float t) 
{ 
  int Tc_100=t*100; 
  int Whole = abs(Tc_100 / 100); // Vorzeichenlosen Werte erzeugen 
  int Fract = abs(Tc_100 % 100); // Vorzeichenlosen Werte erzeugen 
  if (abs(Tc_100) != Tc_100)     // Bei negativem Übergabewert Minuszeichen für Ausgabe setzen 
    Serial.print("-"); 
  Serial.print(Whole); 
  Serial.print("."); 
  if (Fract < 10)                // e.g. t=20.05 => Fract=5 => '0' missing
    Serial.print("0");
  Serial.print(Fract); 
  } 
// ------------------------------------------------------------------
// ------------------------------------------------------------------
// ------------------------------------------------------------------
// ------------------------------------------------------------------
DS18S20_List::DS18S20_List(unsigned char p) : OneWire(p)
{
  last_convert=0;
  count=0;
  int max_loops=255;
  while ((count<MAX_DS18S20) && (OneWire::search(ds18s20[count].addr) && (max_loops--))) 
    {
    if ( OneWire::crc8( ds18s20[count].addr, 7) != ds18s20[count].addr[7]) 
      {
      continue; //Serial.print("CRC is not valid!\n");
      }
  
    if ( ds18s20[count].addr[0] != 0x10) 
      {
      continue; //Serial.print("Device is not a DS18S20 family device.\n");
      }


    ds18s20[count].id=(ds18s20[count].addr[1]<<8)+ds18s20[count].addr[7];
    count++;
    }
  OneWire::reset_search();
  convert();
}
// ------------------------------------------------------------------
void DS18S20_List::update()
{
  unsigned long cur=millis(); // 32bit
  unsigned long elapsed=0;
  if (cur<last_convert)
    {
    elapsed=(0xffffffff-last_convert);
    elapsed+=cur;
    }
  else
    elapsed=cur-last_convert;
  
  if (elapsed<1000) return;

  read();
  convert();
}
// ------------------------------------------------------------------
void DS18S20_List::convert()
{
  for (int i=0;i<count;i++)
    {
    OneWire::reset();
    OneWire::select(ds18s20[i].addr);
    OneWire::write(0x44,0);    
    }
  last_convert=millis();
}
// ------------------------------------------------------------------
void DS18S20_List::read()
{
  for (int i=0;i<count;i++) read(i);
}
// ------------------------------------------------------------------
// read scratchpad and reconstruct hi precision temperature value in degrees celsius
void DS18S20_List::read(unsigned char no)
    {
    if (no>=count) return;
    
    int present = OneWire::reset();
    OneWire::select(ds18s20[no].addr);    
    OneWire::write(0xBE);         // read scratchpad
    if (present==0) {
      ds18s20[no].temp=85.0;
      return;
      }

    unsigned char *d=ds18s20[no].data;
    
    for ( int i=0;i<9;i++)  // read all 9 bytes from scratchpad
      d[i] = OneWire::read();
      
      
    
    if( d[1] == 0 )
    {
      ds18s20[no].temp = (int) d[0] >> 1;
    } else {
      ds18s20[no].temp = -1 * (int) (0x100-d[0]) >> 1;
    }
    ds18s20[no].temp -= 0.25;

    // improve resolution of temperature
    float hi_precision = (int) d[7] - (int) d[6];
    hi_precision = hi_precision / (int) d[7];
    ds18s20[no].temp += hi_precision;
    }
// ------------------------------------------------------------------
float DS18S20_List::get_temp(unsigned char i)
{
  if (i>=count) return 85.0;
  return ds18s20[i].temp;
}
// ------------------------------------------------------------------
float DS18S20_List::get_temp_by_id(unsigned int id)
{
  for (int i=0;i<count;i++)
    if (ds18s20[i].id==id)
      return ds18s20[i].temp;
  return 85.0;
}
// ------------------------------------------------------------------
unsigned int DS18S20_List::get_id(unsigned char no)
{
  if (no>=count) return 0;
  return ds18s20[no].id;
}
// ------------------------------------------------------------------



