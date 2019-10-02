/**
   A trivial C/MRI -> JMRI interface
   =================================
   Sets up pin 13 (LED) as an output, and attaches it to the first output bit
   of the emulated SMINI interface.

   To set up in JMRI:
   1: Create a new connection,
      - type = C/MRI,
      - connection = Serial,
      - port = <arduino's port>,
      - speed = 9600
   2: Click 'Configure C/MRI nodes' and create a new SMINI node
   3: Click 'Add Node' and then 'Done'
   4: Restart J/MRI and it should say "Serial: using Serial on COM<x>" - congratulations!
   5: Open Tools > Tables > Lights and click 'Add'
   6: Add a new light at hardware address 1, then click 'Create' and close the window. Ignore the save message.
   7: Click the 'Off' state button to turn the LED on. Congratulations!

   Debugging:
   Open the CMRI > CMRI Monitor window to check what is getting sent.
   With 'Show raw data' turned on the output looks like:
      [41 54 01 00 00 00 00 00]  Transmit ua=0 OB=1 0 0 0 0 0

   0x41 = 65 = A = address 0
   0x54 = 84 = T = transmit, i.e. PC -> C/MRI
   0x01 = 0b00000001 = turn on the 1st bit
   0x00 = 0b00000000 = all other bits off
*/

// Find pinout here: http://www.hobbytronics.co.uk/arduino-atmega328-pinout

#include <CMRI.h>
#include <Auto485.h>
#include <Adafruit_TLC5947.h>

#define CMRI_TX_ENABLE_PIN A3  // PC3
#define CMRI_ADDRESS 44

#define SCK_PIN 13 // PB5

#define LEDOUT_BLANK_PIN A1 // PC1
#define LEDOUT_XLAT_PIN A0 // PC0
#define LEDOUT_MOSI_PIN 11 // PB3

#define SR_LOAD_PIN A2 // PC2
#define SR_MISO_PIN 12 // PB4

#define NUM_SHIFT_REGISTERS 3

// test U2 outputs

Adafruit_TLC5947 ledout(1, SCK_PIN, LEDOUT_MOSI_PIN, LEDOUT_XLAT_PIN);
Auto485 bus(CMRI_TX_ENABLE_PIN);
CMRI cmri(/*address=*/CMRI_ADDRESS,
          /*input_bits=*/24,
          /*output_bits=*/24,
          bus); // defaults to a SMINI with address 0. SMINI = 24 inputs, 48 outputs

bool hacky_last_state = true;

void setup() {
  pinMode(CMRI_TX_ENABLE_PIN, OUTPUT);
  pinMode(LEDOUT_BLANK_PIN, OUTPUT);
  pinMode(SR_LOAD_PIN, OUTPUT);
  pinMode(SR_MISO_PIN, INPUT);
  
  // DE: max489 pin 4 --> CMRI_TX_ENABLE
  // ^RE: max489 pin 3 --> GND

  ledout.begin();
  digitalWrite(LEDOUT_BLANK_PIN, 0);
  //bus.begin(28800, SERIAL_8N2); // make sure this matches your speed set in JMRI
}

void loop() {
  //cmri.process();

  bool flipflop_counter = (millis() / 1000) % 2 == 0;
  if (flipflop_counter == hacky_last_state) {
    return;
  }

  digitalWrite(SR_LOAD_PIN, LOW);
  delayMicroseconds(100);
  digitalWrite(SR_LOAD_PIN, HIGH);
  delayMicroseconds(100);

  bool ledOn = true;

  for (int sr_idx = 0; sr_idx < NUM_SHIFT_REGISTERS; sr_idx++) {
    byte sr_data = shiftIn(SR_MISO_PIN, SCK_PIN, MSBFIRST);
    cmri.set_byte(sr_idx, sr_data);
//    for (int i = 0; i < 8; i++) {
//      if (bitRead(sr_data, i) == 0) {
//        ledOn = false;
//      }
//    }
  }

  // update input. Flips a bit back and forth every second
  //cmri.set_bit(0, do_thing);

  for (int led = 0; led < 23; led++) {
    ledout.setPWM(led, cmri.get_bit(led) ? 4090 : 0);
  }

  ledout.write();
  //digitalWrite(CMRI_TX_ENABLE_PIN, do_thing);
  hacky_last_state = flipflop_counter;
  
  // 2: update output. Reads bit 0 of T packet and sets the LED to this
  //digitalWrite(13, cmri.get_bit(0));
}
