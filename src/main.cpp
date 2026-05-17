#include <Arduino.h>
#include <BleGamepad.h>
#include <Bounce2.h>



BleGamepadConfiguration config;
BleGamepad bleGamepad("Crusty Controller", "Jordan The Grand");

bool buttonTask(void);
bool hatTask(void);
uint16_t readAxisAveraged (uint8_t pin);
bool axisTask(void);
void pinModeSetup(void);
void sendBatteryLevel(void);

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// TODO change GPIO placeholder number to actual numbers
// Button mapping from GPIO port
#define BTN_X_PIN       GPIO_NUM_4
#define BTN_O_PIN       GPIO_NUM_5
#define BTN_SQR_PIN     GPIO_NUM_6
#define BTN_TRI_PIN     GPIO_NUM_7
#define BTN_L3_PIN      GPIO_NUM_14
#define BTN_R3_PIN      GPIO_NUM_15
#define BTN_LB_PIN      GPIO_NUM_16
#define BTN_RB_PIN      GPIO_NUM_17
//special buttons have to be semi separate
#define BTN_START_PIN   GPIO_NUM_12
#define BTN_SELECT_PIN  GPIO_NUM_13
#define BTN_HOME_PIN    GPIO_NUM_28
#define BTN_COUNT ARRAY_SIZE(BTN_PINS)

// hatswitch mapping
#define DPAD_UP_PIN     GPIO_NUM_8
#define DPAD_LEFT_PIN   GPIO_NUM_9
#define DPAD_DOWN_PIN   GPIO_NUM_10
#define DPAD_RIGHT_PIN  GPIO_NUM_11
#define HAT_COUNT ARRAY_SIZE(HAT_PINS)


// Analog Mapping
#define STICK_LX_PIN   GPIO_NUM_18
#define STICK_LY_PIN   GPIO_NUM_19
#define STICK_RX_PIN   GPIO_NUM_20
#define STICK_RY_PIN   GPIO_NUM_21
#define SLIDER_LT_PIN  GPIO_NUM_26
#define SLIDER_RT_PIN  GPIO_NUM_27
#define AXIS_COUNT ARRAY_SIZE(AXIS_PINS)

#define NUM_SAMPLES 8 // number of samples to average
#define DEADZONE 50
#define NUM_SPECIAL_BTN 3 // number of specials buttons
#define SLEEP_TIMOUT_MS (5 * 60 * 1000) // 5 minutes
#define BATTERY_CHECK_INTERVAL (30 * 1000) // 30 seconds
#define BTN_WAKEUP_BITMASK (1ULL << BTN_HOME_PIN)

#define BATTERY_PIN GPIO_NUM_35
#define BATT_MAX_MV 4200
#define BATT_MIN_MV 3300 // LiPo cutoff voltage

const uint8_t BTN_PINS[] = {
  // saves the buttons into an array for cleaner use
  // Button order in array:
  // X, O, SQR, TRI, DUP, DRIGHT, DLEFT, DDOWN, START, SELECT, L3, R3, LB, RB
      BTN_X_PIN, BTN_O_PIN, BTN_SQR_PIN, BTN_TRI_PIN,
      BTN_L3_PIN, BTN_R3_PIN, BTN_LB_PIN, BTN_RB_PIN,
      BTN_START_PIN, BTN_SELECT_PIN, BTN_HOME_PIN
    };
const uint8_t AXIS_PINS[] = {
// saves the analog inputs into an array for cleaner use
      STICK_LX_PIN, STICK_LY_PIN, STICK_RX_PIN, STICK_RY_PIN,
      SLIDER_LT_PIN, SLIDER_RT_PIN
};
const uint8_t HAT_PINS[] = {
      DPAD_UP_PIN, DPAD_LEFT_PIN, DPAD_DOWN_PIN, DPAD_RIGHT_PIN
};
const TickType_t pollInterval = pdMS_TO_TICKS(10); //10ms or 100Hz polling rate

Bounce2::Button btns[BTN_COUNT];
Bounce2::Button hats[HAT_COUNT];

uint16_t lastAxisValues[AXIS_COUNT] = {0};
unsigned long lastInputTime = 0;
unsigned long lastBatteryCheck = 0;
unsigned long now = 0;

bool buttonTask(void) {
    bool changed = false;

    for (int i = 0; i < BTN_COUNT; i++) {
        btns[i].update(); // updates the button presses each poll

        if (btns[i].fell()) { // fell = just went LOW = pressed
            changed = true;
            if (i < BTN_COUNT - NUM_SPECIAL_BTN)
                bleGamepad.press(i + 1);
            else {
                switch(i) {
                    case (BTN_COUNT - 3): bleGamepad.pressStart();  break;
                    case (BTN_COUNT - 2): bleGamepad.pressSelect(); break;
                    case (BTN_COUNT - 1): bleGamepad.pressHome();   break;
                }
            }
        }
        else if (btns[i].rose()) { // rose = just went HIGH = released
            changed = true;
            if (i < BTN_COUNT - NUM_SPECIAL_BTN)
                bleGamepad.release(i + 1);
            else {
                switch(i) {
                    case (BTN_COUNT - 3): bleGamepad.releaseStart();  break;
                    case (BTN_COUNT - 2): bleGamepad.releaseSelect(); break;
                    case (BTN_COUNT - 1): bleGamepad.releaseHome();   break;
                }
            }
        }
    }
    return changed;
}

bool hatTask(void)
{
  bool changed = false;

  for (int i = 0; i < HAT_COUNT; i++)
  {
    hats[i].update(); // updates the button presses each poll
    if (hats[i].fell() || hats[i].rose())
    {
      changed = true;
    }
  }

  if (changed)
  {
    bool up    = hats[0].isPressed();
    bool left  = hats[1].isPressed();
    bool down  = hats[2].isPressed();
    bool right = hats[3].isPressed();

    // Combines dpad inputs into a single hat direction
    if      ( up && !left && !right)  bleGamepad.setHat(HAT_UP);
    else if ( up &&  right)           bleGamepad.setHat(HAT_UP_RIGHT);
    else if ( up &&  left)            bleGamepad.setHat(HAT_UP_LEFT);
    else if (down && !left && !right) bleGamepad.setHat(HAT_DOWN);
    else if (down &&  right)          bleGamepad.setHat(HAT_DOWN_RIGHT);
    else if (down &&  left)           bleGamepad.setHat(HAT_DOWN_LEFT);
    else if (right)                   bleGamepad.setHat(HAT_RIGHT);
    else if (left)                    bleGamepad.setHat(HAT_LEFT);
    else                              bleGamepad.setHat(HAT_CENTERED);
  }
  
  return changed;
}

uint16_t readAxisAveraged (uint8_t pin)
{
  uint32_t potValue = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    potValue += analogRead(pin);
  }

  potValue = potValue / NUM_SAMPLES;

  return potValue;
}

bool axisTask(void)
{
  // reads the current axis value, sees if the current change is greater than
  // the deadzone, then updates all the sticks if one changed
  bool changed = false;
  uint16_t currentAxisValues[AXIS_COUNT];

  for (int i = 0; i < AXIS_COUNT; i++)
  {
    currentAxisValues[i] = readAxisAveraged(AXIS_PINS[i]);

    if(abs(currentAxisValues[i]-lastAxisValues[i]) > DEADZONE)
    {
      changed = true;
      lastAxisValues[i] = currentAxisValues[i];
    }
  }

  if(changed)
    {
      bleGamepad.setLeftThumb(lastAxisValues[0], lastAxisValues[1]);
      bleGamepad.setRightThumb(lastAxisValues[2], lastAxisValues[3]);
      bleGamepad.setTriggers(lastAxisValues[4], lastAxisValues[5]);

      return changed;
    }
  
  else
  {
    return changed;
  }
}

void pinModeSetup(void)
{
  // sets btn pins to input pullup
  for (int i = 0; i < BTN_COUNT; i++)
  {
    btns[i].attach(BTN_PINS[i], INPUT_PULLUP);
    btns[i].interval(5); // 5ms debounce window
  }
  
  // sets hat pins to input pullup
  for (int i = 0; i < HAT_COUNT; i++)
  {
    hats[i].attach(HAT_PINS[i], INPUT_PULLUP);
    hats[i].interval(5);
  }

  // sets axis pins to analog
  for (int i = 0; i < AXIS_COUNT; i++)
  {
    pinMode(AXIS_PINS[i], ANALOG);
  }
}

void sendBatteryLevel(void) {
    uint32_t raw = readAxisAveraged(BATTERY_PIN); 
    uint32_t millivolts = (raw * 3300 / 4095) * 2; // x2 to undo voltage divider
    uint8_t percent = map(millivolts, BATT_MIN_MV, BATT_MAX_MV, 0, 100);
    percent = constrain(percent, 0, 100);
    bleGamepad.setBatteryLevel(percent);
}



void setup() {
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);  // sets the controller type to a generic xbox controller
  config.setButtonCount(BTN_COUNT - NUM_SPECIAL_BTN);
  config.setHatSwitchCount(1);  // only have 1 set of hat switches (4 hat switches per set)
  config.setWhichSpecialButtons(true, true, false, true, false, false, false, false); // enables the start, select, and home button
  config.setWhichAxes(true, true, false, true, true, false, true, true);  // enables the left/right joystick x/y axis, and left/right trigger
  
  pinModeSetup();

  bleGamepad.begin(&config);
}

void loop() {
  if (bleGamepad.isConnected()) {
    TickType_t lastWake = xTaskGetTickCount();  // creates the clock and has a lastWake so the function will sleep between pollings
    
    // if a button/hat/axis state changes, then newInput is set to true
    bool anyChanged = false;
    anyChanged |= buttonTask();
    anyChanged |= axisTask();
    anyChanged |= hatTask();
    
    now = millis(); // current time

    if (now - lastInputTime > SLEEP_TIMOUT_MS)  // if it has been more than 5 minutes of no changes
    {
      esp_sleep_enable_ext1_wakeup(BTN_WAKEUP_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
      esp_deep_sleep_start();
    }

    if (now - lastBatteryCheck > BATTERY_CHECK_INTERVAL)  // every 30 seconds, send the battery level to the computer
      {
          lastBatteryCheck = now;
          sendBatteryLevel();
          anyChanged = true;
      }

    if (anyChanged)
      {
        lastInputTime = now;
        bleGamepad.sendReport();  // sends report if any button/hat/axis state has changed and send the battery level
      }


    vTaskDelayUntil(&lastWake, pollInterval);
  }
}