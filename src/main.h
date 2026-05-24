#include <Arduino.h>

// Button mapping from GPIO port
#define BTN_X_PIN       GPIO_NUM_42
#define BTN_O_PIN       GPIO_NUM_41
#define BTN_SQR_PIN     GPIO_NUM_40
#define BTN_TRI_PIN     GPIO_NUM_39
#define BTN_L3_PIN      GPIO_NUM_15
#define BTN_R3_PIN      GPIO_NUM_38
#define BTN_LB_PIN      GPIO_NUM_16
#define BTN_RB_PIN      GPIO_NUM_37
//special buttons have to be semi separate
#define BTN_START_PIN   GPIO_NUM_36
#define BTN_SELECT_PIN  GPIO_NUM_17
#define BTN_HOME_PIN    GPIO_NUM_1
#define BTN_COUNT ARRAY_SIZE(BTN_PINS)

// hatswitch mapping
#define DPAD_UP_PIN     GPIO_NUM_18
#define DPAD_LEFT_PIN   GPIO_NUM_14
#define DPAD_DOWN_PIN   GPIO_NUM_21
#define DPAD_RIGHT_PIN  GPIO_NUM_12
#define HAT_COUNT ARRAY_SIZE(HAT_PINS)


// Analog Mapping
#define STICK_LX_PIN   GPIO_NUM_6
#define STICK_LY_PIN   GPIO_NUM_7
#define STICK_RX_PIN   GPIO_NUM_9
#define STICK_RY_PIN   GPIO_NUM_10 
#define SLIDER_LT_PIN  GPIO_NUM_5
#define SLIDER_RT_PIN  GPIO_NUM_4
#define AXIS_COUNT ARRAY_SIZE(AXIS_PINS)

#define NUM_SAMPLES 8 // number of samples to average
#define DEADZONE 50
#define NUM_SPECIAL_BTN 3 // number of specials buttons
#define SLEEP_TIMOUT_MS (5 * 60 * 1000) // 5 minutes
#define BATTERY_CHECK_INTERVAL (30 * 1000) // 30 seconds
#define BTN_WAKEUP_BITMASK (1ULL << (BTN_HOME_PIN))

#define BATTERY_PIN GPIO_NUM_13
#define BATT_MAX_MV 4200
#define BATT_MIN_MV 3300 // LiPo cutoff voltage


bool buttonTask(void);
bool hatTask(void);
int16_t readAxisAveraged (uint8_t pin);
bool axisTask(void);
void pinModeSetup(void);
bool sendBatteryLevel();
void rumbleTask(void);
void unPairingTask(void);
void idleSleepTimer(void);
void sleepTask(void);
void sleep(void);

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