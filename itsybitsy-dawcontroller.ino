/**
 * Title: itsybitsy-dawcontroller
 * Author: D Cooper Dalrymple
 * Created: 14/08/2019
 * Updated: 07/05/2021
 * https://dcooperdalrymple.com/
 */

#include "Keyboard.h"
#include "Mouse.h"

#define BUTTON_STOP     11
#define BUTTON_PLAY     9
#define BUTTON_PAUSE    10
#define BUTTON_RECORD   12
#define BUTTON_REWIND   7
#define BUTTON_FORWARD  13

#define KEY_NONE 0
#define KEY_STOP ' '
#define KEY_PLAY ' '
#define KEY_FORWARD ']'
#define KEY_REWIND '['
#define KEY_RECORD 'r' // Needs ctl
#define KEY_PAUSE ' ' // Needs ctl

#define ENCODER_A 2
#define ENCODER_B 3
#define ENCODER_SWITCH 5
#define ENCODER_MODIFIER KEY_LEFT_SHIFT
#define ENCODER_MODIFIER_ALT KEY_LEFT_ALT

volatile uint8_t encoderState = 0;
volatile int8_t encoderValue = 0;
uint8_t encoderModifier = ENCODER_MODIFIER;
uint8_t encoderModifierPrevious = ENCODER_MODIFIER;
unsigned long encoderTime = 0;
const unsigned long encoderDelay = 250;

#define NUM_BUTTONS 7
const uint8_t buttonPin[NUM_BUTTONS] = { ENCODER_SWITCH, BUTTON_STOP, BUTTON_PLAY, BUTTON_PAUSE, BUTTON_RECORD, BUTTON_REWIND, BUTTON_FORWARD };
const uint8_t buttonCommand[NUM_BUTTONS] = { KEY_NONE, KEY_STOP, KEY_PLAY, KEY_PAUSE, KEY_RECORD, KEY_REWIND, KEY_FORWARD };
const uint8_t buttonModifier[NUM_BUTTONS] = { KEY_NONE, KEY_NONE, KEY_NONE, KEY_LEFT_CTRL, KEY_LEFT_CTRL, KEY_NONE, KEY_NONE };
uint8_t buttonTemp = 0x00;
uint8_t buttonCurrent = 0x00;
uint8_t buttonPrevious = 0x00;
unsigned long buttonTime[NUM_BUTTONS] = { 0, 0, 0, 0, 0, 0, 0 };
const unsigned long buttonDelay = 100;

uint8_t i = 0;

/**
 * Setup
 */

void setup() {
    // Configure all control pins with pull-up resistors
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);
    pinMode(ENCODER_SWITCH, INPUT_PULLUP);
    for (i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttonPin[i], INPUT); //_PULLUP);
    }

    // Encoder Interrupts
    attachInterrupt(0, updateEncoder, CHANGE);
    attachInterrupt(1, updateEncoder, CHANGE);

    Serial.begin(9600);
    Mouse.begin();
    Keyboard.begin();
}

void loop() {
    updateButtons();
    sendControls();
}

/**
 * Control Commands (Keyboard & Mouse)
 */

void sendControls() {

    // Handle Button Updates
    if (buttonCurrent != buttonPrevious) {
        // Do all button commands on release
        for (i = 0; i < NUM_BUTTONS; i++) {
            if (isButtonPress(i)) {
                sendCommand(buttonCommand[i], buttonModifier[i]);
            }
        }

        // Switch encoder to alternate command if encoder switch is pressed
        encoderModifier = isButtonPressed(0) ? ENCODER_MODIFIER_ALT : ENCODER_MODIFIER; // 0 = ENCODER_SWITCH
    }

    // Handle Encoder Changes
    if (sendScroll(encoderValue, encoderModifier)) encoderValue = 0;

}

bool sendCommand(uint8_t command, uint8_t modifier) {
    if (command == KEY_NONE) return false;

    if (modifier != KEY_NONE) Keyboard.press(modifier);
    Keyboard.write(command);
    if (modifier != KEY_NONE) Keyboard.releaseAll();

    return true;
}

bool sendScroll(int8_t amount, uint8_t modifier) {
    if (abs(encoderValue) == 0) {
        if (millis() - encoderTime > encoderDelay) Keyboard.releaseAll();
        return false;
    }

    if (encoderModifierPrevious != modifier) {
        Keyboard.releaseAll();
        encoderModifierPrevious = modifier;
        encoderTime = 0;
    }

    if (millis() - encoderTime > encoderDelay && modifier != KEY_NONE) Keyboard.press(modifier);
    encoderTime = millis();

    Mouse.move(0, 0, amount); // Scroll wheel

    return true;
}

/**
 * Buttons
 */

void updateButtons() {
    buttonPrevious = buttonCurrent;

    // Get values and update debounce time
    for (i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(buttonPin[i]) == HIGH) {
            bitWrite(buttonTemp, i, 1);
        } else {
            bitWrite(buttonTemp, i, 0);
        }
        if (bitRead(buttonTemp, i) != bitRead(buttonCurrent, i)) {
            buttonTime[i] = millis();
        }

        // Update state if debounce is over
        if (millis() - buttonTime[i] < buttonDelay) {
            bitWrite(buttonCurrent, i, bitRead(buttonTemp, i));
        }
    }
}

bool isButtonPressed(uint8_t i) {
    return bitRead(buttonCurrent, i) == 1;
}
bool isButtonPress(uint8_t i) {
    return bitRead(buttonCurrent, i) == 1 && bitRead(buttonCurrent, i) != bitRead(buttonPrevious, i);
}
bool isButtonRelease(uint8_t i) {
    return bitRead(buttonCurrent, i) == 0 && bitRead(buttonCurrent, i) != bitRead(buttonPrevious, i);
}

/**
 * Encoder
 */

void updateEncoder() {

    // Join values as binary
    int state = (encoderState << 2) | (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);

    // Check to see if encoder has incremented or decremented based on previous and current state
    if (state == 0b1101 || state == 0b0100 || state == 0b0010 || state == 0b1011) encoderValue++;
    else if (state == 0b1110 || state == 0b0111 || state == 0b0001 || state == 0b1000) encoderValue--;

    // Store current state
    encoderState = state & 0b0011;

}
