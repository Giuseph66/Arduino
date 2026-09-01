#include <VarSpeedServo.h>
#include <HCSR04.h> 


#define TRIGGER_PIN    2
#define ECHO_PIN       4

UltraSonicDistanceSensor distanceSensor(TRIGGER_PIN,ECHO_PIN);
const int servo1 = 3;  //Eye lid
const int servo2 = 5;  //Left eye X
const int servo3 = 9;  //Right eye X
const int servo4 = 6;  //Left eye Y 
const int servo5 = 10; //Right eye Y  

int servoVal; 

VarSpeedServo myservo1;
VarSpeedServo myservo2;
VarSpeedServo myservo3;
VarSpeedServo myservo4;
VarSpeedServo myservo5;

////     
int ledPin = 11;                // choose the pin for the LED

int inputPin = 2;               // choose the input pin (for PIR sensor)
int pirState = LOW;             // we start, assuming no motion detected
int val = 0;                    
////     
void setup() {

  Serial.println("Starting Ultasonic Test using standard deviation ...");

  
  myservo1.attach(servo1);  // attaches the servo eyelid
  myservo2.attach(servo2);  // attaches the servo left eye X
  myservo3.attach(servo3);  // attaches the servo right eye X
  myservo4.attach(servo4);  // attaches the servo left eye Y
  myservo5.attach(servo5);  // attaches the servo right eye Y
  
  pinMode(ledPin, OUTPUT);      // declare LED as output
  pinMode(inputPin, INPUT);     // declare sensor as input
  
  Serial.begin(9600);
  
  // Ready to go!
  Serial.println("Start");

  
  // Determiando o estado incinicial dos olhos
      myservo1.write(60);  // Palpebras Fechadas
      myservo2.write(100);  // Olho esquerdo centralizado no eixo X
      myservo3.write(100);  // Olho direito centralizado no eixo X
      myservo4.write(100);  // Olho esquerdo Cetralizado no eixo Y
      myservo5.write(90); // Olho direito Centralizado no eixo Y

      delay(100);
      digitalWrite(ledPin, LOW); // Desliga Led
      delay(100);
      digitalWrite(ledPin, HIGH); // Liga Led
      delay(100);
      digitalWrite(ledPin, LOW); // Desliga Led
      delay(100);
}
  
void loop(){
  float cmMsec, inMsec;
  cmMsec=distanceSensor.measureDistanceCm();

  Serial.println(cmMsec);
  if (cmMsec < 200 and cmMsec > 0) {            // check if the input is HIGH
    digitalWrite(ledPin, HIGH);  // turn LED ON
    Serial.println("Motion detected!");

            
      myservo1.write(120); 
      delay(2000);        

      myservo2.slowmove(80, 30);  
      myservo3.slowmove(80,30);  
      myservo4.write(100); 
      myservo5.write(90);  

      delay(1000); 

      myservo2.slowmove(120,30);  
      myservo3.slowmove(120,30); 
      myservo4.write(100);  
      myservo5.write(90);  

    delay(1000);

      myservo2.write(100);  
      myservo3.write(100);  
      myservo4.write(100);
      myservo5.write(90);

    delay(2500);
      
      myservo1.write(60);
      delay(300);
      myservo1.write(120);

    delay(1500);

      myservo1.write(60);
      delay(300);
      myservo1.write(120);

    delay(3000); 


    
  } else {
      Serial.println("No MOtion!");
      digitalWrite(ledPin, LOW); // turn LED OFF
      myservo1.write(60);   // CLOSE EYE
      delay(500);

    }
}