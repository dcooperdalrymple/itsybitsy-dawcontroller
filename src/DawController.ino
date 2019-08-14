/**
 * Title: DawController
 * Author: D Cooper Dalrymple
 * Created: 14/08/2019
 * Updated: 14/08/2019
 * https://dcooperdalrymple.com/
 */

#include "MIDIUSB.h"

#define MIDI_CHANNEL 0
#define MIDI_RELATIVE_CONSTRAIN 15
#define MIDI_LOOP_DELAY 5

enum midiRelativeControlMode {
  TWOS_COMPLEMENT,
  BINARY_OFFSET,
  SIGN_MAGNITUDE
};

#define MIDI_CC_NONE 0x00
#define MIDI_CC_STOP 0x14
#define MIDI_CC_PLAY 0x15
#define MIDI_CC_FAST_FORWARD 0x16
#define MIDI_CC_REWIND 0x17
#define MIDI_CC_RECORD 0x18
#define MIDI_CC_PAUSE 0x19
#define MIDI_CC_SHUTTLE 0x1A
#define MIDI_CC_SHUTTLE_ALT 0x1B
#define MIDI_CC_CUSTOM 0x1C

#define ENCODER_A 2
#define ENCODER_B 3
#define ENCODER_SWITCH 5
#define ENCODER_COMMAND MIDI_CC_SHUTTLE
#define ENCODER_COMMAND_ALT MIDI_CC_SHUTTLE_ALT
const midiRelativeControlMode encoderControlMode = TWOS_COMPLEMENT;

volatile uint8_t encoderState = 0;
volatile int8_t encoderValue = 0;
uint8_t encoderCommand = ENCODER_COMMAND;

#define NUM_BUTTONS 7
const uint8_t buttonPin[NUM_BUTTONS] = { ENCODER_SWITCH, 7, 9, 10, 11, 12, 13 };
const uint8_t buttonCommand[NUM_BUTTONS] = { MIDI_CC_CUSTOM, MIDI_CC_STOP, MIDI_CC_PLAY, MIDI_CC_PAUSE, MIDI_CC_RECORD, MIDI_CC_REWIND, MIDI_CC_FAST_FORWARD };
uint8_t buttonCurrent = 0x00;
uint8_t buttonPrevious = 0x00;

uint8_t i = 0;

void setup() {
  Serial.begin(31250);

  // Configure all control pins with pull-up resistors
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_SWITCH, INPUT_PULLUP);
  for (i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPin[i], INPUT_PULLUP);
  }
  
  // Encoder Interrupts
  attachInterrupt(0, updateEncoder, CHANGE);
  attachInterrupt(1, updateEncoder, CHANGE);
}

void loop() {
  updateButtons();
  sendControls();
}

void controlChange(uint8_t command, int8_t value, midiRelativeControlMode controlMode = TWOS_COMPLEMENT) {
  midiEventPacket_t event = { 0x0B, 0xB0 | MIDI_CHANNEL, command, mapControlValue(value, controlMode) };
  MidiUSB.sendMIDI(event);
}

uint8_t mapControlValue(int8_t value, midiRelativeControlMode controlMode) {
  switch (controlMode) {
    case TWOS_COMPLEMENT:
      // sendMIDI handles bit masking on bit 7
      return value;
    case BINARY_OFFSET:
      return value + 64;
    case SIGN_MAGNITUDE:
      uint8_t mask = (uint8_t)(value >> 7);
      uint8_t abs = (uint8_t)((value + mask) ^ mask);
      uint8_t sign = mask & 0b01000000;
      return (abs & 0b00111111) | sign;
  }
}

void updateButtons() {
  for (i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(buttonPin[i]) == LOW) {
      bitWrite(buttonCurrent, i, 1);
    } else {
      bitWrite(buttonCurrent, i, 0);
    }
  }
}

void updateEncoder() {
  
  // Join values as binary
  int state = (encoderState << 2) | (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);

  // Check to see if encoder has incremented or decremented based on previous and current state
  if (state == 0b1101 || state == 0b0100 || state == 0b0010 || state == 0b1011) encoderValue++;
  else if (state == 0b1110 || state == 0b0111 || state == 0b0001 || state == 0b1000) encoderValue--;

  // Store current state
  encoderState = state & 0b0011;
  
}

void sendControls() {
  
  // Handle Button Updates
  if (buttonCurrent != buttonPrevious) {
    for (i = 0; i < NUM_BUTTONS; i++) {
      if (bitRead(buttonCurrent, i) != bitRead(buttonPrevious, i) && buttonCommand[i] != MIDI_CC_NONE) {
        controlChange(buttonCommand[i], (1 - bitRead(buttonCurrent, i)) * 127);
        
        // Switch encoder to alternate command if encoder switch is pressed
        if (buttonPin[i] == ENCODER_SWITCH) {
          encoderCommand = bitRead(buttonCurrent, i) ? ENCODER_COMMAND_ALT : ENCODER_COMMAND;
        }
      }
    }
    buttonPrevious = buttonCurrent;
  }
  
  // Handle Encoder Changes; send as 2's complement
  if (abs(encoderValue) > 0) {
    //encoderValue = constrain(encoderValue, -MIDI_RELATIVE_CONSTRAIN, MIDI_RELATIVE_CONSTRAIN); // Constrain encoder value to min and max relative change
    controlChange(encoderCommand, encoderValue, encoderControlMode);
    encoderValue = 0;
  }
  
  // Write out remaining buffer
  MidiUSB.flush();
  
  // Add delay if necessary
  //delay(MIDI_LOOP_DELAY);
  
}
