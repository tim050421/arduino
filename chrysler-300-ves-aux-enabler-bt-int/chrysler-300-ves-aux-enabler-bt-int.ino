#include <mcp_can_dfs.h>
#include <mcp_can.h>

#include <SPI.h>
#include "mcp_can.h"
#include <avr/sleep.h>

#define CAN_MODULE_CS_PIN 10
#define ANNOUNCE_PERIOD_MS 2000
#define BUTTON_PRESS_DEBOUNCE_MS 350
#define LONG_PRESS 3500
#define CAN_INT 2                              // Set INT to pin 2

// #define BENCH_MODE_ON // When radio is removed from the car it needs to receive power-on message regularly so it thinks key is on

MCP_CAN CAN(CAN_MODULE_CS_PIN);

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
// #define BT_CONV 16 // Plus to converter 5v - always on
// #define BT_POWER 17
#define BT_PAUSE 18 // Not exist in radio - remap to FF/REW

bool onBeep = false;
bool btPause = false;
bool switchedOff = false; // Audio switched off - do not send signal

/////////
#define RS_TO_MCP2515 true                                   // Set this to false if Rs is connected to your Arduino
volatile unsigned char flagRecv = 0;
#define KEEP_AWAKE_TIME 15000                                // time the controller will stay awake after the last activity on the bus (in ms)
unsigned long lastBusActivity = millis();
/////////

void initCANMask()
{
  CAN.init_Mask(0,0,0x4ff);
  CAN.init_Filt(0,0,0x0); // ID 0x0
  CAN.init_Filt(1,0,0x12); // ID 0x12 FOB lock
  CAN.init_Mask(1,0,0x4ff);
  CAN.init_Filt(2,0,0x2c); // ID 0x2c Temp etc.
  CAN.init_Filt(3,0,0x394); // ID 0x394 Audio buttons
  CAN.init_Filt(4,0,0x3f1); // ID 0x3f1 Audio on-off
  CAN.init_Filt(5,0,0x1b); // ID 0x1b VIN? for wake???
}

void resetCANMask()
{
  CAN.init_Mask(0,0,0x0);
  CAN.init_Filt(0,0,0x0); // ID 0x0
  CAN.init_Filt(1,0,0x0); // ID 0x12 FOB lock
  CAN.init_Mask(1,0,0x0);
  CAN.init_Filt(2,0,0x0); // ID 0x2c Temp etc.
  CAN.init_Filt(3,0,0x0); // ID 0x394 Audio buttons
  CAN.init_Filt(4,0,0x0); // ID 0x3f1 Audio on-off
  CAN.init_Filt(5,0,0x0); // ID 0x1b VIN? for wake???
}

void initBTPins()
{
  pinMode(14, OUTPUT); // BT Pause pin
  pinMode(15, OUTPUT); // BT Next pin
//  pinMode(16, OUTPUT); // BT 5v Converter pin
  pinMode(18, OUTPUT); // BT Prev pin
  digitalWrite(BT_NEXT, LOW); digitalWrite(BT_PREV, LOW); /* digitalWrite(BT_CONV, HIGH); digitalWrite(4, HIGH); */ digitalWrite(BT_PAUSE, LOW);
}

void pressDown(unsigned short pin) {
  digitalWrite(pin, HIGH);
  delay(100);
}

void pressUp(unsigned short pin) {
  digitalWrite(pin, LOW);
  delay(100);
}

void btResetPairing() {
  pressDown(BT_PAUSE);
  Serial.println("Resetting BT");
  delay(LONG_PRESS);
  pressUp(BT_PAUSE);
}
void setup() {
  // Arduino setup
  Serial.begin(115200);
  Serial.println("Chrysler VES Enabler by latonita/TT v.1.1");
  initBTPins();
  // MCP2515 setup
  int i = 0;
  while(CAN_OK != CAN.begin(CAN_83K3BPS, MCP_8MHz) && i < 10) {
    Serial.println("CAN init fail");
    delay(250);
    i++;
  }
  CAN.setMode(MODE_NORMAL);
  Serial.println("CAN init ok");

  // Set Filters
  initCANMask();
//  resetCANMask();

  // Reset BT adapter to pair mode
  pressDown(BT_PAUSE);
  Serial.println("Resetting BT");
  delay(4000);
  pressUp(BT_PAUSE);
  Serial.println("Resetting BT - done");

/////////
  pinMode(CAN_INT, INPUT);                           // Configuring pin for /INT input
  attachInterrupt(digitalPinToInterrupt(CAN_INT), MCP2515_ISR, FALLING);
  CAN.setSleepWakeup(1);                              // this tells the MCP2515 to wake up on incoming messages
////////  

}

void MCP2515_ISR() {
    flagRecv = 1;
//    Serial.println(".");
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
  delay(80);       // Beep length
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

void checkIncomingMessages() {
  static unsigned int canId = 0;
  static unsigned char len = 0;
  static unsigned char buf[8];
  static unsigned char oldMode = radioMode;
  static byte err2;
  static unsigned char newMsg[7];
  
//  if(flagRecv) {
//      flagRecv = 0;
  if(!digitalRead(CAN_INT)) {                          // If CAN0_INT pin is low, read receive buffer
//  if(CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&len, buf);
    lastBusActivity = millis();
  }
  else {
    if(Serial.available() != 0){
      if(Serial.parseInt() == 1){
        Serial.println("RND/PTY pressed");
        btResetPairing();
      } else {
        Serial.println("Terminal button pressed!");
      }
    }
    return;
  }
  canId = CAN.getCanId();

  switch (canId) {
    case 0x0:
//      if(buf[0] == 0x63) { // Command to switch on audio
//        Serial.print("Audio On signal sent with: 0x");
//        Serial.print(buf[1], HEX); Serial.print(" 0x");
//        Serial.print(buf[2], HEX); Serial.print(" 0x");
//        Serial.print(buf[3], HEX); Serial.print(" 0x");
//        Serial.print(buf[4], HEX); Serial.print(" 0x");
//        Serial.println(buf[5], HEX);
////          switchedOff = false;
//      }
      break;
    case 0x01b: // VIN code
      break;
    case 0x2c: // Engine temp etc - convert data from CCN 2005 to CCN 2006+
      for(int i=0; i < 6; i++)
        newMsg[i] = buf[i];
      newMsg[6] = 0xff;
      err2 = CAN.sendMsgBuf(0x2c, 0, 7, newMsg);
//      if(err2 != CAN_OK)
//        Serial.print("Error sending CCN data: 0x"); Serial.println(err2, HEX);
      break;
    case 0x12: // Lock auto from keyfob
      if(buf[0] == 1 && buf[1] == 1 && buf[3] == 64 && buf[4] == 197 && onBeep == false){  //Lock
        onBeep = true;  
        beepOnLock();
        Serial.println("Lock auto beep!!!");
        switchedOff = true;
      } else if(buf[0] == 3 && buf[1] == 1 && buf[3] == 64 && buf[4] == 197){ 
        Serial.println("Unlock auto beep!!!");
        onBeep = false;
        switchedOff = false;
      }
      break;
    case 0x3f1: // 1009
      if(buf[0] == 28) {
        Serial.print("Audio power on/off - ");
        Serial.println(buf[1]);
        if(buf[1] == 1 && !switchedOff) { // Off
//          btPowerOff();
          shortPress(BT_PAUSE);
          switchedOff = true;
        } else if(buf[1] == 0) {
//          btPowerOn();
          switchedOff = false;
          sendAnnouncements();
        }
      } 
      break;
    case 0x394: // 916 Audio button pressed/depressed
      switch(buf[3]) {
          case 2: // Prev track
            Serial.println("Prev track pressed");
            shortPress(BT_PREV);
            break;
          case 1: // Next track
            Serial.println("Next track pressed");
            shortPress(BT_NEXT);
            break;
          case 8: // Rewind
          case 4: // Fast Forward
            Serial.println("Pause pressed");
            shortPress(BT_PAUSE);
            break;
          case 0: // Button depressed
            break;
          case 32: // RND/PTY
            Serial.println("RND/PTY pressed");
            btResetPairing();
            break;
          default:
            break;
      }
/*      Serial.print("Audio head button pressed - ");
      for(int i=0;i<4;i++){
       Serial.print(buf[i]);
       Serial.print(", ");
      } 
      Serial.println(""); */
      break;
    default:
          if(CAN.isExtendedFrame()) {
            Serial.print("E ");
          }
          Serial.print("{ \"id\": 0x"); Serial.print(canId, HEX);
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

void goToSleep() {

  // Put MCP2515 into sleep mode
  Serial.println("CAN sleep");
  CAN.sleep();

  // Put the MCU to sleep
  Serial.println("MCU sleep");

  // Clear serial buffers before sleeping
  Serial.flush();

  cli(); // Disable interrupts
  if (!flagRecv) { // Make sure we havn't missed an interrupt between the check above and now. If an interrupt happens between now and sei()/sleep_cpu() then sleep_cpu() will immediately wake up again
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
      sleep_enable();
      sleep_bod_disable();
      sei();
      sleep_cpu();
      // Now the Arduino sleeps until the next message arrives...
      sleep_disable();
  }
  sei();

  CAN.wake(); // When the MCP2515 wakes up it will be in LISTENONLY mode, here we put it into the mode it was before sleeping

  initBTPins();
  initCANMask();
  Serial.println("Woke up");
}

// Main loop
void loop() {
  if (!flagRecv && millis() > lastBusActivity + KEEP_AWAKE_TIME) {
    goToSleep();
  } else {
    flagRecv = 0;                   // clear flag

//    if (millis() > lastAnnounce + ANNOUNCE_PERIOD_MS) {
//      lastAnnounce = millis();
//      Serial.println("Arduino alive.");
//    }

    checkIncomingMessages();
  }
}
