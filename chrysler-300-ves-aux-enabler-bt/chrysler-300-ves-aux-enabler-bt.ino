#include <mcp_can_dfs.h>
#include <mcp_can.h>

#include <SPI.h>
#include "mcp_can.h"

#define CAN_MODULE_CS_PIN 10
#define ANNOUNCE_PERIOD_MS 500

#define BUTTON_PRESS_DEBOUNCE_MS 350

// #define BENCH_MODE_ON // When radio is removed from the car it needs to receive power-on message regularly so it thinks key is on

MCP_CAN CAN(CAN_MODULE_CS_PIN);
#define CAN0_INT 2                              // Set INT to pin 2

unsigned long lastAnnounce = 0;

unsigned char msgVesAuxMode[8] = {3,0,0,0,0,0,0,0};
unsigned char beepOn[5] = {128,0,0,136,12};
unsigned char beepOff[5] = {0,0,0,136,12};

#ifdef BENCH_MODE_ON
unsigned char msgPowerOn[6]= {0x63,0,0,0,0,0};
#endif

#define RADIOMODE_OTHER 0
#define RADIOMODE_AUX 1
unsigned char radioMode = RADIOMODE_OTHER;

#define BT_NEXT 14
#define BT_PREV 15
#define BT_CONV 16 // Plus to converter 5v - always on
#define BT_POWER 17
#define BT_PAUSE 18 // Not exist in radio - remap to another button?

bool onBeep = false;
bool btPause = false;

void setup() {
  // Arduino setup
  Serial.begin(115200);
  Serial.println("Chrysler VES Enabler by latonita/TT v.1.1");
  pinMode(14, OUTPUT); // BT Next pin
  pinMode(15, OUTPUT); // BT Prev pin
  pinMode(16, OUTPUT); // BT 5v Converter pin
  pinMode(18, OUTPUT); // BT Pause pin
  digitalWrite(14, LOW); digitalWrite(15, LOW); digitalWrite(16, HIGH); /* digitalWrite(4, HIGH); */ digitalWrite(18, LOW);
//  pinMode(CAN0_INT, INPUT);                           // Configuring pin for /INT input

  // MCP2515 setup
  while(CAN_OK != CAN.begin(CAN_83K3BPS, MCP_8MHz)) {
    Serial.println("CAN init fail");
    delay(250);
  }
  CAN.setMode(MODE_NORMAL);
  Serial.println("CAN init ok");
}

void sendAnnouncements() {
#ifdef BENCH_MODE_ON
    // when on bench - send power on command to radio to enable it
    CAN.sendMsgBuf(0x0, 0, 6, msgPowerOn);
    delay(25);
#endif
    //tell them VES AUX is here
    byte err = CAN.sendMsgBuf(0x3dd, 0, 8, msgVesAuxMode);
//    if(err != CAN_OK) {
//      Serial.print("Error sending audio mode data: 0x"); Serial.println(err, HEX);
//    }
    delay(25);
}

// Beep 
void beepOnLock() {
  byte err = CAN.sendMsgBuf(285, 0, 5, beepOn);
  delay(100);
  err = CAN.sendMsgBuf(285, 0, 5, beepOff);
  if(err != CAN_OK) {
    Serial.print("Error sending beep 0x"); Serial.println(err, HEX);
  }
  delay(300);
}

void shortPress(unsigned short pin) {
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
  delay(200);
}

void btPowerOff() {
  digitalWrite(BT_POWER, LOW);
  delay(200);
}
void btPowerOn() {
  digitalWrite(BT_POWER, HIGH);
  delay(200);
}

void checkIncomingMessages() {
  static unsigned int canId = 0;
  static unsigned char len = 0;
  static unsigned char buf[8];
  static unsigned char oldMode = radioMode;
  static byte err2;
  static unsigned char newMsg[7];
  static bool isBtOn = false;
  
//  if(!digitalRead(CAN0_INT)) {                          // If CAN0_INT pin is low, read receive buffer
  if(CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&len, buf);
  }
  else {
    return;
  }
  canId = CAN.getCanId();

  switch (canId) {
    case 0x0:
      if(buf[0] == 0x63) {
        Serial.print("Audio On signal sent with: 0x");
        Serial.print(buf[1], HEX); Serial.print(" 0x");
        Serial.print(buf[2], HEX); Serial.print(" 0x");
        Serial.print(buf[3], HEX); Serial.print(" 0x");
        Serial.print(buf[4], HEX); Serial.print(" 0x");
        Serial.println(buf[5], HEX);
        if(!isBtOn) {
          btPowerOn();
          isBtOn = true;
        }
      }
      break;
    case 0x09f:
      // current radio mode
      radioMode = (buf[0] & 0xF == 6) ? RADIOMODE_AUX : RADIOMODE_OTHER;

      if (oldMode != radioMode) {
        if (radioMode == RADIOMODE_AUX) {
          Serial.print("Radio Mode changed to AUX");
        } else {
          Serial.print("Radio Mode changed to something else");
        }
      }
      break;
    case 0x01b: // VIN code
      break;
    case 0x2c: // Engine temp etc - convert data from CCN 2005 to CCN 2006+
      for(int i=0; i < 6; i++)
        newMsg[i] = buf[i];
      newMsg[6] = 0xff;
      err2 = CAN.sendMsgBuf(0x2c, 0, 7, newMsg);
//      if(err2 != CAN_OK) {
//        Serial.print("Error sending CCN data: 0x"); Serial.println(err2, HEX);
//      }
      break;
    case 0x12: // Lock auto from keyfob
      if(buf[0] == 1 && buf[1] == 1 && buf[3] == 64 && buf[4] == 197 && onBeep == false){  //Lock
        onBeep = true;  
        beepOnLock();
        Serial.println("Lock auto beep!!!");
      } else if(buf[0] == 3 && buf[1] == 1 && buf[3] == 64 && buf[4] == 197){ 
        Serial.println("Unlock auto beep!!!");
        onBeep = false;
      }
      break;
    case 1009:
      if(buf[0] == 28) {
        Serial.print("Audio power on/off - ");
        Serial.println(buf[1]);
        if(buf[1] == 1) { // Off
          btPowerOff();
        } else if(buf[1] == 0) {
          btPowerOn();
        }
      }
      break;
    case 916: // Audio button pressed/depressed
      if(buf[0] == 23 && buf[1] == 4 && buf[2] == 36) {
          switch(buf[3]) {
              case 2: // Prev track
                Serial.println("Next track pressed");
                shortPress(BT_PREV);
                break;
              case 1: // Next track
                Serial.println("Prev track pressed");
                shortPress(BT_NEXT);
                break;
              case 8: // Rewind
              case 4: // Fast Forward
//                btPowerOnOff();
                Serial.println("Pause pressed");
                shortPress(BT_PAUSE);
                break;
              case 0: // Button depressed
                break;
              default:
                break;
          }
      }
      break;
    default:
/*          if(CAN.isExtendedFrame()) {
            Serial.print("E ");
          }
          Serial.print("{ \"id\": "); Serial.print(canId);
          Serial.print(", \"len\": "); Serial.print(len);
          Serial.print(", \"value\": [ ");
          for(int i=0; i<len; i++) {
            if(i > 0)
              Serial.print(",");
            Serial.print(" "); Serial.print(buf[i]);
          }
          Serial.println(" ] }"); */
      break;
  }
}

// Main loop
void loop() {
  if (millis() > lastAnnounce + ANNOUNCE_PERIOD_MS) {
    lastAnnounce = millis();
    sendAnnouncements();
  }
  checkIncomingMessages();
}
