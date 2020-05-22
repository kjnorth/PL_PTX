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

TX_TO_RX ttr;
RF24 radio(RF_CE_PIN, RF_CSN_PIN); // Create a Radio

void setup() {
  Serial.begin(115200);

  ttr.Phase = 0xA4;
  ttr.LEDControl = 0xB7;
  ttr.FrontEncoder = 0xFFEEDDCC;

  pinMode(RF_IRQ_PIN, INPUT);

  if (!radio.begin())
    Serial.println("PTX failed to initialize");
  else {
    // RF24 library begin() function enables PTX mode
    radio.setAddressWidth(5); // set address size to 5 bytes
    radio.setRetries(15, 15); // set 2 retries with 500us delays in between
    radio.setChannel(RF_CHANNEL); // set communication channel
    // radio.setPayloadSize(NUM_TTR_BYTES); // set max transmission payload size to sizeof TTR data struct
    radio.setPALevel(RF24_PA_LOW); // set power amplifier level. Using LOW for tests on bench. Should use HIGH on PL/Truck
    radio.setDataRate(RF24_1MBPS); // set data rate to most reliable speed
    radio.openWritingPipe(RF_PTX_WRITE_ADDR);
    radio.openReadingPipe(0, RF_PTX_READ_ADDR_P0);
    // No need to call startListening() because the PTX transmits first
    Serial.println("PTX initialization successful");
  }
}

unsigned long curTime = 0;
static unsigned long preTime = 0;
unsigned long curSendTime = 0;
static unsigned long curReceiveTime = 0;
void loop() {
  curTime = millis();
  if (curTime - preTime >= 1000) {
    radio.startWrite(&ttr, NUM_TTR_BYTES, 0); // last param - request ACK (0), NOACK (1)
    curSendTime = curTime;
    preTime = curTime;
  }

  if (LOW == digitalRead(RF_IRQ_PIN)) {
      bool tx_ok, tx_fail, rx_ready;
      radio.whatHappened(tx_ok, tx_fail, rx_ready);
      if (tx_ok || rx_ready) {
        static RX_TO_TX rtt;
        static int i = 0;
        radio.read(&rtt, NUM_RTT_BYTES);
        curReceiveTime = millis();
        LogInfo(F("switchStatus %d, solenoid Status %d, tx_ok %d, rx_ready %d, count %d, dt [ms] %ld\n"),
                rtt.SwitchStatus, rtt.SolenoidStatus, tx_ok, rx_ready, ++i, curSendTime-curReceiveTime);
      }
      else if (tx_fail) {
        static int j = 0;
        LogInfo(F("tx has reached the number maximum number of retries %d, count %d, time %ld ms\n"), radio.getARC(), ++j, millis());
      }
    }
}