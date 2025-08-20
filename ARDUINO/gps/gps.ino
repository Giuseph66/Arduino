#include <TinyGPS++.h>
TinyGPSPlus gps;

unsigned long lastUpdate   = 0;
const unsigned long TIMEOUT = 5000;

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println(F("=== GPS via Serial0 (0/1) a 9600bps ==="));
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // pisca LED pra mostrar que o loop está vivo
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  delay(100);

  // 1) leia NMEA cru, só decode (sem ecoar)
  while (Serial.available()) {
    char c = Serial.read();
    if (gps.encode(c)) lastUpdate = millis();
  }

  // 2) timeout?
  if (millis() - lastUpdate > TIMEOUT) {
    Serial.println(F("! TIMEOUT: sem NMEA completo do GPS"));
    delay(1000);
    return;
  }

  // 3) exibe fix apenas quando válido
  if (gps.location.isValid()) {
     Serial.print(F("Lat: ")); Serial.println(gps.location.lat(), 6);
    Serial.print(F("Lng: ")); Serial.println(gps.location.lng(), 6);
    Serial.print(F("Satélites: ")); Serial.println(gps.satellites.value());
    Serial.print(F("Alt (m): "));
      if (gps.altitude.isValid()) Serial.println(gps.altitude.meters());
      else                           Serial.println(F("n/d"));
    Serial.print(F("HDOP: "));
      if (gps.hdop.isValid())     Serial.println(gps.hdop.hdop());
      else                         Serial.println(F("n/d"));
    Serial.print(F("Vel (km/h): "));
      if (gps.speed.isValid())    Serial.println(gps.speed.kmph());
      else                         Serial.println(F("n/d"));
    Serial.print(F("Curso (°): "));
      if (gps.course.isValid())   Serial.println(gps.course.deg());
      else                         Serial.println(F("n/d"));
    Serial.print(F("Data: "));
      if (gps.date.isValid()) {
        Serial.print(gps.date.day()); Serial.print('/');
        Serial.print(gps.date.month()); Serial.print('/');
        Serial.println(gps.date.year());
      } else Serial.println(F("n/d"));
    Serial.print(F("Hora UTC: "));
      if (gps.time.isValid()) {
        if (gps.time.hour() < 10) Serial.print('0');
        Serial.print(gps.time.hour()); Serial.print(':');
        if (gps.time.minute() < 10) Serial.print('0');
        Serial.print(gps.time.minute()); Serial.print(':');
        if (gps.time.second() < 10) Serial.print('0');
        Serial.println(gps.time.second());
      } else Serial.println(F("n/d"));
    Serial.println();
  }
}
