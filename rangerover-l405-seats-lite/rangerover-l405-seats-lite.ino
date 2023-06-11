#include <mcp_can_dfs.h>
#include <mcp_can.h>
#include <SPI.h>
#include <mcp_can.h>

#define CAN_MODULE_CS_PIN 10

#define CHECK_PERIOD_MS 200
#define ANNOUNCE_PERIOD_MS 104

MCP_CAN CAN(CAN_MODULE_CS_PIN);
#define CAN0_INT 2                              // Set INT to pin 2

unsigned long lastAnnounce = 0;
unsigned long millsTimer;

//unsigned char msgPowerOn[6]= {0x63,0,0,0,0,0};
unsigned char msgWakeUp[8] =   {0x58, 0x12,0x7F,0xFF,0x8,0x11,0,0x3 };
unsigned char msgWakeUpDr[8] = {0x0B, 0x12,0x7F,0xFF,0x8,0x11,0,0x3 };

unsigned char diagnosticSession[2] = {0x10,0x1};
// ReadDataByIdentifier Bytes 3,4 - identifier, next is params
unsigned char dataByIdentifier[4] {0x22, 0x1, 0x0, 0x0};

unsigned int msg5Ids[8] = {0x501, 0x504, 0x505, 0x506, 0x507, 0x508, 0x51E, 0x526};
unsigned char msg500[8][8] = {
  {0x4, 0x12, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0},
  {0x5, 0x12, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0 },
  {0x6, 0x12, 0x0, 0x0, 0x5, 0x0, 0x0, 0x0},
  {0x7, 0x12, 0x0, 0x0, 0x6, 0x0, 0x0, 0x0},
  {0x8, 0x12, 0x0, 0x0, 0x7, 0x0, 0x0, 0x0 },
  {0xB, 0x2, 0x7F, 0xFF, 0x8, 0x11, 0x0, 0x3},
  {0x26, 0x2, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0},
  {0x58, 0x12, 0x0, 0x0, 0x26, 0x0, 0x0, 0x0}
};

void setup() {
  Serial.begin(250000);
  Serial.println("RR seats Enabler by TT v.1.1");

  while(CAN_OK != CAN.begin(CAN_125KBPS, MCP_16MHz)) {
    Serial.println("CAN init fail");
    delay(250);
  }
  CAN.setMode(MODE_NORMAL);
//  CAN.init_Mask(0, 0, 0x000); 
//  CAN.init_Mask(1, 0, 0x000);
     
  Serial.println("CAN init ok");

//  CAN.sendMsgBuf(0x501, 0, 8, msgWakeUp);
}

// Disable seat operations from module 0x3ca
unsigned char msgDisableSeat[8] = {0x00,0x00,0x00,0x00,0x00,0xff,0x00,0x00};
/////////////////////////////

int i = 0;

void sendAnnouncements() {
  if(i < 8) {
//    CAN.sendMsgBuf(0x508, 0, 8, msgWakeUpDr);
    CAN.sendMsgBuf(msg5Ids[i], 0, 8, msg500[i]);
    i++;
  } else { 
    i = 0;
    
    CAN.sendMsgBuf(0x744, 0, 2, diagnosticSession);
    Serial.println("Checking session");
    delay(1000);
    CAN.sendMsgBuf(0x744, 0, 4, dataByIdentifier);
    Serial.println("Read data by identifier");
    delay(1000);

  }
}

void checkIncomingMessages() {
  static unsigned int canId = 0;
  static unsigned char len = 0;
  static unsigned char buf[12];
  static byte err2;
  static unsigned char newMsg[7];
  
  if(CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&len, buf);
  }
  else {
    return;
  }
  canId = CAN.getCanId();

  switch (canId) {
    case 0x310: // Seat heartbeat? codes
    case 0x311:
      break;
    case 0x24D:
    case 0x378:
//      if(buf[5] == 0)
        break;
/*      for (int i = 0; i < 1; i++){
        unsigned char b[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1F, 0x59, 0x52};
        CAN.sendMsgBuf(0x3ca, 0, 8, b);
        delay(20);
      } 
      Serial.println("Commands received");
      break; */
    case 0x50B: // Right (driver) seat wakeup
//      CAN.sendMsgBuf(0x508, 0, 8, msgWakeUpDr);
      Serial.print(".");
      break; 
/*    case 0x558: // Wake up module and not allow to sleep
      CAN.sendMsgBuf(0x501, 0, 8, msgWakeUp);        
      Serial.println(",");
      delay(80);
      break; */
    default:
          Serial.print("{ \"id\": \"0x"); Serial.print(canId, HEX);
          Serial.print("\", \"len\": "); Serial.print(len);
          Serial.print(", \"value\": [");
          for(int i=0; i<len; i++) {
            if(i > 0)
              Serial.print("\",");
            Serial.print(" \"0x"); Serial.print(buf[i], HEX);
          }
          Serial.println("\" ] }"); 
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
