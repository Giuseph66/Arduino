const int ADC_PIN = A3;

void setup() {
  Serial.begin(115200);
  analogReference(DEFAULT); // 0..5V
}

void loop() {
  int v = analogRead(ADC_PIN);
  Serial.println(v);
  delay(5); // ~200 Hz
}
