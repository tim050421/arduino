#include <SPI.h>
#include <mcp2515.h>

#define CHECK_PERIOD_MS 200
#define ANNOUNCE_PERIOD_MS 200

struct can_frame canMsg;
MCP2515 mcp2515(10);

unsigned long lastAnnounce = 0;

//unsigned char msgPowerOn[6]= {0x63,0,0,0,0,0};
struct can_frame msgWakeUp = {0x501, 8, 0x58,0x02,0,0, 0,0,0,0 };
struct can_frame newMsg;

void setup() {
  Serial.begin(115200);
  SPI.begin();

  mcp2515.reset();
  
  MCP2515::ERROR error = mcp2515.setConfigMode();
  if(error != MCP2515::ERROR_OK) {
    Serial.print("Error setting config mode: ");
    Serial.println(error, HEX);
  }
  
  error = mcp2515.setBitrate(CAN_125KBPS, MCP_8MHZ);
  if(error != MCP2515::ERROR_OK) {
    Serial.print("Error setting bitrate: ");
    Serial.println(error, HEX);
  }
  error = mcp2515.setNormalMode();
  if(error != MCP2515::ERROR_OK) {
    Serial.print("Error setting normal mode: ");
    Serial.println(error, HEX);
}

  Serial.println("RR seats Enabler by TT v.1.0");

//  CAN.sendMsgBuf(0x501, 0, 8, msgWakeUp);
  mcp2515.sendMessage(&msgWakeUp);

  newMsg.can_dlc=8;
}

///////////////////////////// BrutForce!
unsigned int brut = 0x600;
unsigned char msgBrutZero[8] = {0x0,0x8,0,0,0,0,0,0};
unsigned char msgBrutFF[8] =   {0x0,0x8,0xff,0xff,0xff,0xff,0xff,0xff};
// Disable seat operations from module 0x3ca
unsigned char msgDisableSeat[8] = {0x00,0x00,0x00,0x00,0x00,0xff,0x00,0x00};
/////////////////////////////

void sendAnnouncements() {
    newMsg.can_id=brut; 
    newMsg.data[0]=0xff; newMsg.data[1]=0; newMsg.data[2]=0; newMsg.data[3]=0; newMsg.data[4]=0; newMsg.data[5]=0; newMsg.data[6]=0; newMsg.data[7]=0;
    if(brut < 0x610) {
      if(brut != 0x3ca)
        mcp2515.sendMessage(&newMsg);
      Serial.print("Sent to: 0x"); Serial.println(brut, HEX);
      brut++;
    }
}

void checkIncomingMessages() {
  static unsigned char len = 0;
  static unsigned char buf[12];
  static byte err2;
  
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    switch (canMsg.can_id) {
      case 0x01b: // VIN code
        break;
      case 0x558:
        if(buf[0] == 0x1){
          mcp2515.sendMessage(&msgWakeUp);
          delay(80);
        }
        break;
      case 0x310:
        if(canMsg.data[0] == 0 && canMsg.data[1] == 0 && canMsg.data[2] == 0 && canMsg.data[3] == 0 && canMsg.data[4] == 0 && canMsg.data[5] == 0)
          break;
      case 0x378:
        if(canMsg.data[0] == 0 && canMsg.data[1] == 0 && canMsg.data[2] == 0 && canMsg.data[3] == 0 && canMsg.data[4] == 0 && canMsg.data[5] == 0)
          break;
      default:
            Serial.print("{ \"id\": 0x"); Serial.print(canMsg.can_id, HEX);
            Serial.print(", \"len\": "); Serial.print(canMsg.can_dlc);
            Serial.print(", \"value\": [");
            for(int i=0; i<len; i++) {
              if(i > 0)
                Serial.print(",");
              Serial.print(" 0x"); Serial.print(canMsg.data[i], HEX);
            }
            Serial.println(" ] }"); 
        break;
    } 
  }
}

void loop() {
  if (millis() > lastAnnounce + ANNOUNCE_PERIOD_MS) {
    lastAnnounce = millis();
    sendAnnouncements();
  }
  checkIncomingMessages();
}
