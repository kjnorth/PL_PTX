/** @author Kodiak North
 * @date 05/21/2020
 * test project with NRF24l01 module in PTX mode, to 
 * communicate with another NRF24l01 module in PRX mode
 */

#include <Arduino.h>
#include "ConfigPTX.h"
#include "..\lib\DataLog\DataLog.h"
#include "nRF24L01.h"
#include "RF24.h"

bool IsConnected(void);

TX_TO_RX ttr;
RF24 radio(RF_CE_PIN, RF_CSN_PIN); // Create a radio object

void setup() {
  Serial.begin(115200);

  ttr.Phase = 0xA4;
  ttr.LEDControl = 0xB7;
  ttr.FrontEncoder = 157291;
  // ttr.Count = 14;

#ifdef RF_USE_IRQ_PIN
  pinMode(RF_IRQ_PIN, INPUT);
#endif  

  if (!radio.begin())
    Serial.println("PTX failed to initialize");
  else {
    // RF24 library begin() function enables PTX mode
    radio.setAddressWidth(5); // set address size to 5 bytes
    radio.setRetries(15, 5); // set 5 retries with 500us delays in between
    radio.setChannel(RF_CHANNEL); // set communication channel
    // radio.setPayloadSize(NUM_TTR_BYTES); // set payload size to number of bytes being SENT
    radio.enableDynamicPayloads();
    radio.enableAckPayload(); // enable payload attached to ACK from PRX
    radio.setPALevel(RF24_PA_LOW); // set power amplifier level. Using LOW for tests on bench. Should use HIGH on PL/Truck
    radio.setDataRate(RF24_1MBPS); // set data rate to most reliable speed
    // radio.setDataRate(RF24_2MBPS);
    radio.openWritingPipe(RF_PTX_WRITE_ADDR); // open the writing pipe on the address we chose
    Serial.println("PTX initialization successful");
  }
}

unsigned long curTime = 0;
unsigned long preLogTime = 0;
unsigned long preSendTime = 0;
unsigned long lastReceiveTime = 0;
static RX_TO_TX rtt;
void loop() {
  curTime = millis();
  IsConnected();

  /** check if IRQ pin is active before sending since it is not attached to an
   * interrupt pin on the Arduino. This means it can be missed in the main
   * loop if placed directly after the 'write', and the PTX hasn't received
   * the ACK and payload from the PRX. By checking the pin before sending, we
   * will capture the IRQ on the next iteration of the loop. */
  /** @note try to throw this in TIMER ISR so we capture data right when it's
   * sent back */
#ifdef RF_USE_IRQ_PIN  
  if (LOW == digitalRead(RF_IRQ_PIN)) {
    bool tx_ok=false, tx_fail=false, rx_ready=false;
    radio.whatHappened(tx_ok, tx_fail, rx_ready);
    if (tx_ok || rx_ready) {
#endif      
      if (radio.isAckPayloadAvailable()) {
        radio.read(&rtt, NUM_RTT_BYTES);
        lastReceiveTime = curTime;
      }
#ifdef RF_USE_IRQ_PIN      
    }
  }
#endif  

  if (curTime - preSendTime >= 10) {
    ttr.Phase++;
    ttr.LEDControl++;
    ttr.FrontEncoder++;
#ifdef RF_USE_IRQ_PIN    
    radio.startFastWrite(&ttr, NUM_TTR_BYTES, 0);
#else    
    radio.writeFast(&ttr, NUM_TTR_BYTES, 0);
#endif    
    preSendTime = curTime;
  }

  if (curTime - preLogTime >= 1000) {
    // LogInfo("pitch ", rtt.Pitch, 2);
    // LogInfo(", roll ", rtt.Roll, 2);
    LogInfo("pitch %d", rtt.Pitch);
#if ROLL    
    LogInfo(", roll %d", rtt.Roll);
#endif    
    LogInfo(F(", switchStatus 0x%X, solenoid Status 0x%X, isConnected %d\n"),
                rtt.SwitchStatus, rtt.SolenoidStatus, IsConnected());
    preLogTime = curTime;
  }
}

bool IsConnected(void) {
  static bool conn = false;
  if (curTime - lastReceiveTime >= 250 && conn) {
    LogInfo("connection to PRX is lost!\n");
    conn = false;
  }
  else if (lastReceiveTime > 0 && curTime - lastReceiveTime < 250 && !conn) {
    LogInfo("established connection to PRX\n");
    conn = true;
  }
  return conn;
}