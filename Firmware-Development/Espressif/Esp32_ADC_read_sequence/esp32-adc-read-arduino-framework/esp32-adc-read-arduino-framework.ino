
/* TODO */

void setup() {
    Serial.begin(115200);
}

void loop() {
    int adcValues[8];

    adcValues[0] = analogRead(36);  // ADC1_CH0 (GPIO36)
    adcValues[1] = analogRead(39);  // ADC1_CH3 (GPIO39)
    adcValues[2] = analogRead(34);  // ADC1_CH6 (GPIO34)
    adcValues[3] = analogRead(35);  // ADC1_CH7 (GPIO35)
    adcValues[4] = analogRead(32);  // ADC1_CH4 (GPIO32)
    adcValues[5] = analogRead(33);  // ADC1_CH5 (GPIO33)
    adcValues[6] = analogRead(25);  // ADC2_CH8 (GPIO25) - Wi-Fi Conflict
    adcValues[7] = analogRead(26);  // ADC2_CH9 (GPIO26) - Wi-Fi Conflict

    Serial.println("ADC Readings:");
    for (int i = 0; i < 8; i++) {
        Serial.printf("ADC%d: %d\n", i, adcValues[i]);
    }

    delay(1000);  // Wait 1 second before next reading
}
