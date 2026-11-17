// Educational prototype receiver for SerialBackend.  NOT certified traffic equipment.
// Wiring (each LED through a ~330 ohm resistor to GND):
//   D2 RED, D3 YELLOW, D4 GREEN, D5 LEFT arrow, D6 STRAIGHT arrow, D7 RIGHT arrow
// Protocol: "T <G|Y|R|O> <0..7>\n".  Fail-safe: if no valid line arrives for 5 s,
// every output goes dark (never leave a lamp state latched on a dead link).
const uint8_t PIN_RED = 2, PIN_YEL = 3, PIN_GRN = 4, PIN_L = 5, PIN_S = 6, PIN_R = 7;
unsigned long lastGood = 0;

void setAll(char c, uint8_t mask) {
  digitalWrite(PIN_RED, c == 'R');
  digitalWrite(PIN_YEL, c == 'Y');
  digitalWrite(PIN_GRN, c == 'G');
  digitalWrite(PIN_L, mask & 1);
  digitalWrite(PIN_S, mask & 2);
  digitalWrite(PIN_R, mask & 4);
}

void setup() {
  for (uint8_t p = 2; p <= 7; ++p) pinMode(p, OUTPUT);
  setAll('O', 0);
  Serial.begin(9600);
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 5 && line[0] == 'T' && line[1] == ' ' && line[3] == ' ') {
      char c = line[2];
      int m = line[4] - '0';
      if ((c == 'G' || c == 'Y' || c == 'R' || c == 'O') && m >= 0 && m <= 7) {
        setAll(c, c == 'O' ? 0 : m);
        lastGood = millis();
        Serial.println("OK");
        return;
      }
    }
    Serial.println("ERR");
  }
  if (millis() - lastGood > 5000) setAll('O', 0);
}
