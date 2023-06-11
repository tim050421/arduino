#include <SoftwareSerial.h>

// https://github.com/zapta/linbus/tree/master/analyzer/arduino
#include "lin_frame.h"

// Pins we use for MCP2004
#define CS_PIN 8
#define RX_PIN 10
#define TX_PIN 11
#define FAULT_PIN 12

#define SYN_FIELD 0x55

SoftwareSerial LINBusSerial(RX_PIN, TX_PIN);
LinFrame frame;

int ledState = LOW;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 1000;           // interval at which to blink (milliseconds)

void initPins()
{
  pinMode(A0, OUTPUT); // 1-st chan
  pinMode(A1, OUTPUT); // 2-nd chan
  pinMode(A2, OUTPUT); // 3-rd chan
  pinMode(A3, OUTPUT); // 4-th chan
  pinMode(A4, OUTPUT); // +
  pinMode(A5, OUTPUT); // -
  pinMode(A6, OUTPUT); // 5-th chan
  pinMode(A7, OUTPUT); // 6-th chan
  digitalWrite(A0, HIGH); digitalWrite(A1, HIGH); digitalWrite(A2, HIGH); digitalWrite(A3, HIGH); 
  digitalWrite(A4, HIGH); digitalWrite(A5, HIGH); // + - 
  digitalWrite(A6, HIGH); digitalWrite(A7, HIGH);
  // set the digital pin as output:
  pinMode(LED_BUILTIN, OUTPUT);
  // setup softserial pins
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  pinMode(FAULT_PIN, OUTPUT);
  digitalWrite(FAULT_PIN, HIGH);

}

void setup() {
  initPins();

  Serial.begin(250000);
  Serial.println("LIN Debugging begins");

  LINBusSerial.begin(9600);
  frame = LinFrame();
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
//  processRelays();
  
  if (LINBusSerial.available()) {
    digitalWrite(LED_BUILTIN, HIGH);
    char b = LINBusSerial.read();
    unsigned char n = frame.num_bytes();
    Serial.print("Received data len, byte: "); Serial.print(n); Serial.print(" , 0x"); Serial.println(b, HEX);

    // MCP2004 LIN bus frame:
    // ZERO_BYTE SYN_BYTE ID_BYTE DATA_BYTES.. CHECKSUM_BYTE
    if (b == SYN_FIELD && n > 2 && frame.get_byte(n - 1) == 0) { // Next frame
      if(frame.isValid()){
        dump_frame();
      }
      frame.reset();
    } else if (n == LinFrame::kMaxBytes) {
      frame.reset();
    } else {
      frame.append_byte(b);
    }
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void dump_frame() {
  for (unsigned char i = 0; i < frame.num_bytes(); i++) {
    Serial.print(frame.get_byte(i), HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void processRelays()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
      digitalWrite(A2, LOW);
    } else {
      ledState = LOW;
      digitalWrite(A2, HIGH);
    }
    digitalWrite(LED_BUILTIN, ledState);
  }
}
