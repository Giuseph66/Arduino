const int ADC_PIN = 22;

void setup() {
  Serial.begin(115200);
}

void loop() {
  int v = analogRead(ADC_PIN);
  Serial.println(v);
  delay(100); // ~200 Hz
}
