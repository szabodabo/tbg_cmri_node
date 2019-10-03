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
#define LEDOUT_BITS 24

#define SR_LOAD_PIN A2 // PC2
#define SR_MISO_PIN 12 // PB4

#define NUM_SHIFT_REGISTERS 3

Adafruit_TLC5947 ledout(1, SCK_PIN, LEDOUT_MOSI_PIN, LEDOUT_XLAT_PIN);
Auto485 bus(CMRI_TX_ENABLE_PIN);
CMRI cmri(/*address=*/CMRI_ADDRESS,
          /*input_bits=*/24,
          /*output_bits=*/LEDOUT_BITS,
          bus); // defaults to a SMINI with address 0. SMINI = 24 inputs, 48 outputs

bool out_bit_cache[LEDOUT_BITS];

void setup() {
  pinMode(CMRI_TX_ENABLE_PIN, OUTPUT);
  pinMode(LEDOUT_BLANK_PIN, OUTPUT);
  pinMode(SR_LOAD_PIN, OUTPUT);
  pinMode(SR_MISO_PIN, INPUT);
  
  // DE: max489 pin 4 --> CMRI_TX_ENABLE
  // ^RE: max489 pin 3 --> GND

  ledout.begin();
  digitalWrite(LEDOUT_BLANK_PIN, 0);
  bus.begin(28800, SERIAL_8N2); // make sure this matches your speed set in JMRI
}

void loop() {
  cmri.process();

  digitalWrite(SR_LOAD_PIN, LOW);
  delayMicroseconds(100);
  digitalWrite(SR_LOAD_PIN, HIGH);
  delayMicroseconds(100);

  for (int sr_idx = 0; sr_idx < NUM_SHIFT_REGISTERS; sr_idx++) {
    byte sr_data = shiftIn(SR_MISO_PIN, SCK_PIN, MSBFIRST);
    cmri.set_byte(sr_idx, sr_data);
  }

  bool rewrite_leds = false;
  for (int led = 0; led < LEDOUT_BITS; led++) {
    if (out_bit_cache[led] != cmri.get_bit(led)) {
      out_bit_cache[led] = cmri.get_bit(led);
      rewrite_leds = true;
    }
  }

  if (rewrite_leds) {
    for (int led = 0; led < LEDOUT_BITS; led++) {
      ledout.setPWM(led, out_bit_cache[led] ? 4090 : 0);
    }
    ledout.write();
  }
}
