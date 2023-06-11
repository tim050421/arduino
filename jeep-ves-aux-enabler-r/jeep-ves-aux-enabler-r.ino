#include <mcp_can_dfs.h>
#include <mcp_can.h>

/*****************************************************************************************
*
* Hack for my Jeep Grand Cherokee WH 2006 (european) + RAR radio head unit
*
* My hardware:
*  1. Arduino Pro Mini 5v/16Mhz
*  2. Mcp2515_can SPI module (8Mhz)
*
* Features:
*  1. Bench mode to enable radio functioning while removed from the car 
*  2. Emulating VES presense to enable AUX IN in head unit
*
* Copyright (C) 2015-2017 Anton Viktorov <latonita@yandex.ru>
*                                    https://github.com/latonita/jeep-canbus
*
* This is free software. You may use/redistribute it under The MIT License terms.
*
*****************************************************************************************/
#include <SPI.h>
#include <mcp_can.h>

#define CAN_MODULE_CS_PIN 10

#define CHECK_PERIOD_MS 200
#define ANNOUNCE_PERIOD_MS 500

#define BUTTON_PRESS_DEBOUNCE_MS 350

#define BENCH_MODE_ON // When radio is removed from the car it needs to receive power-on message regularly so it thinks key is on

MCP_CAN CAN(CAN_MODULE_CS_PIN);
#define CAN0_INT 2                              // Set INT to pin 2

unsigned long lastAnnounce = 0;

unsigned char msgVesAuxMode[8] = {3,0,0,0,0,0,0,0};
unsigned char beepOn[8] = {128,0,0,136,12};
unsigned char beepOff[8] = {0,0,0,136,12};

#ifdef BENCH_MODE_ON
unsigned char msgPowerOn[6]= {0x63,0,0,0,0,0};
#endif

#define RADIOMODE_OTHER 0
#define RADIOMODE_AUX 1
unsigned char radioMode = RADIOMODE_OTHER;

void setup() {
  Serial.begin(115200);
  Serial.println("Chrysler VES Enabler by latonita/TT v.1.1");

  // My Jeep 2006 uses low-speed interior can bus 83.3kbps. Newer models use 125kbps for interior can-b bus and call it 'high-speed'
  // I use 8MHz mcp2515_can SPI module, had to update mcp_can lib to be able to work with lower freq. originally is works only with 16MHz modules
  while(CAN_OK != CAN.begin(CAN_125KBPS, MCP_16MHz)) {
//  while(CAN_OK != CAN.begin(CAN_83K3BPS, MCP_8MHz)) {
    Serial.println("CAN init fail");
    delay(250);
  }
  CAN.setMode(MODE_NORMAL);
///  CAN.setMode(MODE_LOOPBACK);
//  pinMode(CAN0_INT, INPUT); // Configuring pin for /INT input

//  CAN.init_Mask(0, 0, 0x000); 
//  CAN.init_Mask(1, 0, 0x000);
     
  Serial.println("CAN init ok");
}

void sendAnnouncements() {
#ifdef BENCH_MODE_ON
    // when on bench - send power on command to radio to enable it
    CAN.sendMsgBuf(0x0, 0, 6, msgPowerOn);
    delay(25);
#endif
    //tell them VES AUX is here
//    byte err = CAN.sendMsgBuf(0x03dd, 0, 8, msgVesAuxMode);
//    if(err != CAN_OK) {
//      Serial.print("Error sending audio mode data: 0x"); Serial.println(err, HEX);
//    }
//    delay(25);
}

void beepOnLock() {
  byte err = CAN.sendMsgBuf(285, 0, 5, beepOn);
  delay(200);
  err = CAN.sendMsgBuf(285, 0, 5, beepOff);
    if(err != CAN_OK) {
      Serial.print("Error sending beep 0x"); Serial.println(err, HEX);
    }
}

void checkIncomingMessages() {
  static unsigned int canId = 0;
  static unsigned char len = 0;
  static unsigned char buf[12];
  static unsigned char oldMode = radioMode;
  static byte err2;
  static unsigned char newMsg[7];
  static bool oldCCN = false;
  
//  if(!digitalRead(CAN0_INT)) {                          // If CAN0_INT pin is low, read receive buffer
  if(CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&len, buf);
  }
  else {
    return;
  }
  canId = CAN.getCanId();

  switch (canId) {
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
    // Steering buttons:
//    case 0x3a0:
//      Serial.print("Steering wheel button pressed: ");
//      Serial.print(buf[0], HEX);
//      Serial.print("\n");
//      break;
    // Ignition key
    case 0x20b:
      switch(buf[0]) { 
        case 0x0:
          Serial.println("Ignition - no key");
          break;
        case 0x1:
          Serial.println("Ignition - key in");
          break;
        case 0x63:
        case 63:
          Serial.println("Ignition - key in accessories pos");
          break;
        case 0x87:
        case 87:
          Serial.println("Ignition - key in ON/RUN pos");
          break;
        case 0xaf:
          Serial.println("Ignition - key in starter pos");
          break;
      }
      break;
    case 0x01b: // VIN code
      break;
    case 0x2c: // Engine temp etc
      for(int i=0; i < 6; i++)
        newMsg[i] = buf[i];
      newMsg[6] = 0xff;
      err2 = CAN.sendMsgBuf(0x2c, 0, 7, newMsg);
      if(err2 != CAN_OK) {
        Serial.print("Error sending CCN data: 0x"); Serial.println(err2, HEX);
        if(err2 == 0x07)
          oldCCN = true;
      }
      break;
    case 0x12: // Lock auto from keyfob ?
      if(buf[0] == 1 && buf[1] == 1 && buf[3] == 64 && buf[4] == 197){ 
              beepOnLock();
      }
      break;
    default:
          if(CAN.isExtendedFrame()) {
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
          Serial.println(" ] }"); 
      break;
  }
}
void loop() {
  if (millis() > lastAnnounce + ANNOUNCE_PERIOD_MS) {
    lastAnnounce = millis();
    sendAnnouncements();
  }
  checkIncomingMessages();
}
