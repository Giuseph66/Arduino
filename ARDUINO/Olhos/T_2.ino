#include <VarSpeedServo.h>
#include <HCSR04.h>

#define TRIGGER_PIN 2
#define ECHO_PIN 4

UltraSonicDistanceSensor distanceSensor(TRIGGER_PIN, ECHO_PIN);

const int servo1 = 3;  // Eye lid
const int servo2 = 5;  // Left eye X
const int servo3 = 9;  // Right eye X
const int servo4 = 6;  // Left eye Y
const int servo5 = 10; // Right eye Y

VarSpeedServo palpebras;
VarSpeedServo olho_esquerdo_x;
VarSpeedServo olho_direito_x;
VarSpeedServo olhor_esquerdo_y;
VarSpeedServo olho_direito_y;

int ledPin = 11;
int inputPin = 2;

void setup() {
    Serial.begin(9600);
    Serial.println("Iniciando...");

    palpebras.attach(servo1);
    olho_esquerdo_x.attach(servo2);
    olho_direito_x.attach(servo3);
    olhor_esquerdo_y.attach(servo4);
    olho_direito_y.attach(servo5);

    pinMode(ledPin, OUTPUT);
    pinMode(inputPin, INPUT);

    Serial.println("Pronto!!!");

    //Estado inicial dos Olhos
    conjunto_motores({{palpebras,60, 0},{olho_direito_x,100, 30},{olho_esquerdo_x,100, 30},{olho_direito_y,90,0},{olhor_esquerdo_y,100,0}}); 


    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
}

void loop() {
    float cmMsec = distanceSensor.measureDistanceCm();
    Serial.println(cmMsec);

    if (cmMsec < 200 && cmMsec > 0) {
        digitalWrite(ledPin, HIGH);
        Serial.println("Motion detected!");

    motor_separado(palpebras,120, 0);
    delay(2000);        
    conjunto_motores({{palpebras,-1, 0},{olho_direito_x,80, 30},{olho_esquerdo_x,80, 30},{olho_direito_y,90,0},{olhor_esquerdo_y,100,0}});
    delay(1000); 
    conjunto_motores({{palpebras,-1, 0},{olho_direito_x,120, 30},{olho_esquerdo_x,120, 30},{olho_direito_y,90,0},{olhor_esquerdo_y,100,0}});
    delay(1000);
    conjunto_motores({{palpebras,-1, 0},{olho_direito_x,100, 30},{olho_esquerdo_x,100, 30},{olho_direito_y,90,0},{olhor_esquerdo_y,100,0}});
    delay(2500);
    motor_separado(palpebras,60, 0);
    delay(300);
    motor_separado(palpebras,120, 0);
    delay(1500);
    motor_separado(palpebras,60, 0);
    delay(300);
    motor_separado(palpebras,120, 0);
    delay(3000); 
    } else {
        Serial.println("No Motion!");
        digitalWrite(ledPin, LOW);
        palpebras.write(60);
        delay(500);
    }
}

// Função para controlar um motor individual
void motor_separado(VarSpeedServo &motor, int position, int speed) {
    if (speed>0)and(position>-1){
        motor.slowmove(position, speed);
    }else if (position>-1){
        motor.write(position);
    }else{
        Serial.println("Sem movimentos para esse servo!");
    }
}

// Estrutura para armazenar parâmetros do motor
struct MotorConfig {
    VarSpeedServo &motor;
    int position;
    int speed;
};

// Função para controlar múltiplos motores
void conjunto_motores(std::initializer_list<MotorConfig> motors) {
    for (const auto &m : motors) {
        motor_separado(m.motor, m.position, m.speed);
    }
}
