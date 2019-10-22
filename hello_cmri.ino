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


#define FLASH_PERIOD_MS 250
long LAST_FLASH_TICK_MS = millis();
bool FLASH_RELAY_ON = false;

bool is_special_flashing_led(int led) {
  return led == 11;
}

// When a button is pressed, pulse the lights N times,
// where N is the position of the button in the data array.
//#define DEBUG_BUTTON_PULSES

// Turn on all LEDs when any button is pressed.
#define DEBUG_BUTTON_ALLON

// On startup, iterate through all LEDs.
#define STARTUP_CYCLE_LEDs

#if defined(DEBUG_BUTTON_PULSES) || defined(DEBUG_BUTTON_ALLON)
  #define SKIP_CMRI_FOR_TESTING
#endif

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
  allLEDsOff();
  digitalWrite(LEDOUT_BLANK_PIN, 0);

  #ifdef STARTUP_CYCLE_LEDs
  for (int i = 0; i < LEDOUT_BITS; i++) {
    ledout.setPWM(i, 4000);
    ledout.write();
    delay(70);
    ledout.setPWM(i, 0);
    ledout.write();
    delay(150);
  }
  #endif

  #ifndef SKIP_CMRI_FOR_TESTING
  bus.begin(28800, SERIAL_8N2); // make sure this matches your speed set in JMRI
  #endif
}

void allLEDsOff() {
  for (int led = 0; led < LEDOUT_BITS; led++) {
    ledout.setPWM(led, 0);
  }
  ledout.write();
}

void allLEDsOn() {
  for (int led = 0; led < LEDOUT_BITS; led++) {
    ledout.setPWM(led, 4000);
  }
  ledout.write();
}

void debugShowBit(bool on) {
  allLEDsOff();
  delay(25);

  for (int led = 0; led < LEDOUT_BITS; led++) {
    if (!on && led > 3) {
      ledout.setPWM(led, 0);
      continue;
    }
    ledout.setPWM(led, 4000);
  }
  ledout.write();
}

bool getNthBit(byte data, unsigned i) {
  return (data >> i) & 0x01 == 1;
}

void debugDisplayByte(byte data) {
  for (int i = 0; i < 8; i++) {
    debugShowBit(getNthBit(data, i));
    delay(50);
  }
}

void debugSetMappedByte(byte data, unsigned offset) {
  for (int i = 0; i < 8; i++) {
    bool set = getNthBit(data, i);
    ledout.setPWM(offset*8 + i, set ? 4000 : 0);
  }
}

void readButtons(bool* out) {
  digitalWrite(SR_LOAD_PIN, LOW);
  delayMicroseconds(100);
  digitalWrite(SR_LOAD_PIN, HIGH);
  delayMicroseconds(100);

  for (int r = 0; r < NUM_SHIFT_REGISTERS; r++) {
    for (int i = 0; i < 8; i++) {
      digitalWrite(SCK_PIN, 0);
      delayMicroseconds(5);
      out[r*8 + i] = !digitalRead(SR_MISO_PIN);
      digitalWrite(SCK_PIN, 1);
    }
  }
}

void loop() {
  #ifndef SKIP_CMRI_FOR_TESTING
  cmri.process();
  #endif

  bool btn[24];
  readButtons(btn);

  #ifdef DEBUG_BUTTON_PULSES

  for (int i = 0; i < 24; i++) {
    if (btn[i]) {
      for (int k = 0; k < i; k++) {
        debugShowBit(true);
        allLEDsOff();
        delay(75);
      }
      delay(750);
    }
  }
  return;
  #endif

  bool rewrite_leds = false;
  for (int led = 0; led < LEDOUT_BITS; led++) {
    #if defined(DEBUG_BUTTON_ALLON)
    bool is_on = false;
    for (int b = 0; b < 24; b++) {
      if (btn[b]) {
        is_on = true;
        break;
      }
    }
    #else
    bool is_on = cmri.get_bit(led);
    #endif

    if (is_on && is_special_flashing_led(led)) {
      if (millis() - LAST_FLASH_TICK_MS > FLASH_PERIOD_MS) {
        LAST_FLASH_TICK_MS = millis();
        FLASH_RELAY_ON = !FLASH_RELAY_ON;
      }
      is_on = FLASH_RELAY_ON;
    }

    if (out_bit_cache[led] != is_on) {
      out_bit_cache[led] = is_on;
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
