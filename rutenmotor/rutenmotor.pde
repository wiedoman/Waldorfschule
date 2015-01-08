int E1 = 5;     //M1 Speed Control
int M1 = 4;    //M1 Direction Control
int E2 = 6;     //M1 Speed Control
int M2 = 7;    //M1 Direction Control


void setup(void) 
{ 
    pinMode(4, OUTPUT);  
    pinMode(5, OUTPUT);  
    pinMode(6, OUTPUT);  
    pinMode(7, OUTPUT);  
    Serial.begin(115200);      //Set Baud Rate
} 

void loop(void) 
{ 
//     char val = Serial.read();
//     if(val!=-1)
//       {
//          switch(val)
//           {
//             case 'w'://Move Forward
                     Serial.println("w");
                     digitalWrite(M1,HIGH);
                     analogWrite(E1,45);
                     digitalWrite(M2,HIGH);
                     analogWrite(E2,45);
//                     break;
//             case 's'://Move Backward
//                     Serial.println("s");
//                     digitalWrite(M1,LOW);
//                     analogWrite(E1,0);
//                     digitalWrite(M2,LOW);
//                     analogWrite(E2,0);
//                     break;          
//            }     
//          delay(250); 
//       }
}

