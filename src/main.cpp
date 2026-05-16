#include <Arduino.h>
#include <BleGamepad.h>



BleGamepadConfiguration config;
BleGamepad bleGamepad("Crusty Controller", "Jordan The Grand", 100);

void buttonTask(void);
void pinModeSetup(void);

// TODO change GPIO placeholder number to actual numbers
// Button mapping from GPIO port
#define BTN_X_PIN       GPIO_NUM_4
#define BTN_O_PIN       GPIO_NUM_5
#define BTN_SQR_PIN     GPIO_NUM_6
#define BTN_TRI_PIN     GPIO_NUM_7
#define BTN_DUP_PIN     GPIO_NUM_8
#define BTN_DRIGHT_PIN  GPIO_NUM_9
#define BTN_DLEFT_PIN   GPIO_NUM_10
#define BTN_DDOWN_PIN   GPIO_NUM_11
#define BTN_START_PIN   GPIO_NUM_12
#define BTN_SELECT_PIN  GPIO_NUM_13
#define BTN_L3_PIN      GPIO_NUM_14
#define BTN_R3_PIN      GPIO_NUM_15
#define BTN_LB_PIN      GPIO_NUM_16
#define BTN_RB_PIN      GPIO_NUM_17

// Analog Mapping from GPIO port
#define ANALOG_LX_PIN   GPIO_NUM_18
#define ANALOG_LY_PIN   GPIO_NUM_19
#define ANALOG_RX_PIN   GPIO_NUM_20
#define ANALOG_RY_PIN   GPIO_NUM_21
#define ANALOG_LT_PIN   GPIO_NUM_26
#define ANALOG_RT_PIN   GPIO_NUM_27

// saves the buttons into an array for cleaner use
// Button order in array:
// X, O, SQR, TRI, DUP, DRIGHT, DLEFT, DDOWN, START, SELECT, L3, R3, LB, RB
const uint8_t BTN_PINS[] = {
      BTN_X_PIN, BTN_O_PIN, BTN_SQR_PIN, BTN_TRI_PIN, 
      BTN_DUP_PIN, BTN_DRIGHT_PIN, BTN_DLEFT_PIN, BTN_DDOWN_PIN,
      BTN_START_PIN, BTN_SELECT_PIN, BTN_L3_PIN, BTN_R3_PIN,
      BTN_LB_PIN, BTN_RB_PIN
    };

// converts the total byte size of the array into number of elements
// size of array / size of each element in array = # of elements (32 byte / 1 byte = # of elements)
const int NUM_BUTTONS = sizeof(BTN_PINS) / sizeof(BTN_PINS[0]);

// saves the analog inputs into an array for cleaner use
const uint8_t ANALOG_PINS[] = {
      ANALOG_LX_PIN, ANALOG_LY_PIN, ANALOG_RX_PIN, ANALOG_RY_PIN,
      ANALOG_LT_PIN, ANALOG_RT_PIN
};

const int NUM_ANALOG = sizeof(ANALOG_PINS) / sizeof(ANALOG_PINS[0]);

uint32_t lastState = 0;


// function to read all the buttons, check if the last read is different than the current read,
// and then sends the report to the computer if the buttons pressed are different
void buttonTask(void)
{
  // creates the clock and has a lastWake so the function will sleep between pollings
  const TickType_t pollInterval = pdMS_TO_TICKS(10); //10ms or 100Hz polling rate
  TickType_t lastWake = xTaskGetTickCount();

  uint32_t currentState = 0;

  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    currentState |= (digitalRead(BTN_PINS[i]) == LOW) << i;
  }

  if (currentState != lastState)  // checks if any buttons have been pressed or released
  {
    bleGamepad.sendReport(); // Send the report to the pc
  }
}

void pinModeSetup(void)
{
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }

  for (int i = 0; i < NUM_ANALOG_INPUTS; i++)
  {

  }
}

void setup() {
  bleGamepad.begin();
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  bleGamepad.begin(&config);


}

void loop() {
  if (bleGamepad.isConnected()) {
    buttonTask();
  }
}