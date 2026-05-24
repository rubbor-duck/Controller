#include <Arduino.h>
#include <BleGamepad.h>
#include <Bounce2.h>
#include <NimBLEDevice.h>
#include <driver/rtc_io.h>
#include <main.h>

// TODO: WORK ON FIXING AXIS, then we can design/print the pcb board

BleGamepadConfiguration config;
BleGamepad bleGamepad("Crusty Controller", "Jordan The Grand");

Bounce2::Button btns[BTN_COUNT];
Bounce2::Button hats[HAT_COUNT];

unsigned long lastInputTime = 0;
unsigned long lastBatteryCheck = 0;
unsigned long unpairTimer = 0;
unsigned long sleepTimer = 0;
unsigned long now = 0;

int16_t lastAxisValues[AXIS_COUNT] = {0};

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
    static bool lastUp    = false;
    static bool lastLeft  = false;
    static bool lastDown  = false;
    static bool lastRight = false;

    // INPUT_PULLUP: LOW = pressed, so invert with !
    bool up    = !digitalRead(DPAD_UP_PIN);
    bool left  = !digitalRead(DPAD_LEFT_PIN);
    bool down  = !digitalRead(DPAD_DOWN_PIN);
    bool right = !digitalRead(DPAD_RIGHT_PIN);

    bool changed = (up    != lastUp)   || (left  != lastLeft) ||
                   (down  != lastDown) || (right != lastRight);

    if (changed)
    {
        lastUp = up; lastLeft = left; lastDown = down; lastRight = right;

        if      (up   && !left && !right)  bleGamepad.setHat(HAT_UP);
        else if (up   &&  right)           bleGamepad.setHat(HAT_UP_RIGHT);
        else if (up   &&  left)            bleGamepad.setHat(HAT_UP_LEFT);
        else if (down && !left && !right)  bleGamepad.setHat(HAT_DOWN);
        else if (down &&  right)           bleGamepad.setHat(HAT_DOWN_RIGHT);
        else if (down &&  left)            bleGamepad.setHat(HAT_DOWN_LEFT);
        else if (right)                    bleGamepad.setHat(HAT_RIGHT);
        else if (left)                     bleGamepad.setHat(HAT_LEFT);
        else                               bleGamepad.setHat(HAT_CENTERED);
    }

    return changed;
}

int16_t readAxisAveraged (uint8_t pin)
{
  uint32_t potValue = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    potValue += analogRead(pin);
    delayMicroseconds(50); // 50 micro seconds between samples
  }

  potValue = potValue / NUM_SAMPLES;

  // Map analog reading from 0 ~ 4095 to 32737 ~ 0 for use as an axis reading
  int32_t adjustedValue = map(potValue, 0, 4095, 32767, 0);

  adjustedValue = constrain(adjustedValue, -32767, 32767);

  return (int16_t)adjustedValue;
}

bool axisTask(void)
{
  // reads the current axis value, sees if the current change is greater than
  // the deadzone, then updates all the sticks if one changed
  bool changed = false;

  for (int i = 0; i < AXIS_COUNT; i++)
  {
    int16_t val = readAxisAveraged(AXIS_PINS[i]);

    if(abs(val-lastAxisValues[i]) > DEADZONE)
    {
      changed = true;
      lastAxisValues[i] = val;
    }
  }

  if(changed)
    {
      // TODO: fix triggers, find out why right stick is z and Rz
      bleGamepad.setX(lastAxisValues[0]);
      bleGamepad.setY(lastAxisValues[1]);
      bleGamepad.setRX(lastAxisValues[2]);
      bleGamepad.setRY(lastAxisValues[3]);
      bleGamepad.setSliders(lastAxisValues[4], lastAxisValues[5]);

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
    btns[i].interval(2); // 2ms debounce window
  }
  
  // sets hat pins to input pullup
  for (int i = 0; i < HAT_COUNT; i++)
  {
    pinMode(HAT_PINS[i], INPUT_PULLUP);
  }

  // sets axis pins to analog
  for (int i = 0; i < AXIS_COUNT; i++)
  {
    pinMode(AXIS_PINS[i], ANALOG);
  }
}

bool sendBatteryLevel() 
{
  if (now - lastBatteryCheck > BATTERY_CHECK_INTERVAL)  // every 30 seconds, send the battery level to the computer
    {
      uint32_t raw = readAxisAveraged(BATTERY_PIN); 
      uint32_t millivolts = (raw * 3300 / 4095) * 2; // x2 to undo voltage divider
      uint8_t percent = map(millivolts, BATT_MIN_MV, BATT_MAX_MV, 0, 100);
      percent = constrain(percent, 0, 100);
      bleGamepad.setBatteryLevel(percent);

      lastBatteryCheck = now;
      return true;
    }
  else
    return false;
}

void rumbleTask(void)
{
  // add later once the base controller works fine
}

void sleep(void)
{
  // Keep pullups alive during deep sleep so pins don't float LOW
  rtc_gpio_pullup_en(BTN_HOME_PIN);
  rtc_gpio_pulldown_dis(BTN_HOME_PIN);

  esp_sleep_enable_ext1_wakeup(BTN_WAKEUP_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);  
  esp_deep_sleep_start();
}

void idleSleepTimer()
{
  if (now - lastInputTime > SLEEP_TIMOUT_MS)  // if it has been more than 5 minutes of no changes
    {
      sleep();
    }
}

void sleepTask()
{
  if((!btns[BTN_COUNT - 1].isPressed()) && (btns[BTN_COUNT - 2].isPressed())) // only home button held for more than 5 seconds
  {
    if (now - sleepTimer > 5000) 
    {
      digitalWrite(LED_BUILTIN, LOW);
      delay(3000);

      sleep();
    }
  }

  else
  {  
    sleepTimer = now;
  }
}

void unPairingTask()
{
  if ((!btns[BTN_COUNT - 1].isPressed()) && (!btns[BTN_COUNT - 2].isPressed())) // home and select buttons held
    {
      if (now - unpairTimer > 5000) 
        bleGamepad.deleteAllBonds(true);
    }
  else
    unpairTimer = now;
}


// Setup Runs Before The Main loop
void setup() {
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  config.setButtonCount(BTN_COUNT - NUM_SPECIAL_BTN);
  config.setHatSwitchCount(1);  // only have 1 set of hat switches (4 hat switches per set)
  config.setWhichSpecialButtons(true, true, false, true, false, false, false, false); // enables the start, select, and home button
  config.setWhichAxes(true, true, true, true, true, true, true, true);  // enables the left/right joystick x/y axis, and left/right trigger
  config.setAxesMin(0x8001); // -32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  config.setAxesMax(0x7FFF); // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal 
  pinModeSetup();
 
  bleGamepad.begin(&config);
  bleGamepad.setHat(HAT_CENTERED);
  bleGamepad.sendReport();

  now = millis();
  unpairTimer = now;
  lastInputTime = now;
  sleepTimer = now;

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

// Main loop
void loop() {
  if (bleGamepad.isConnected()) {
    TickType_t lastWake = xTaskGetTickCount();  // creates the clock and has a lastWake so the function will sleep between pollings
    
    // if a button/hat/axis state changes, then newInput is set to true
    bool anyChanged = false;
    anyChanged |= buttonTask();
    anyChanged |= axisTask();
    anyChanged |= hatTask();
    
    now = millis(); // current time


    idleSleepTimer(); // puts ESP to sleep if idle time is greater than 5 minutes

    sleepTask(); // if only home button is held down, then the controller will go to sleep

    // anyChanged |= sendBatteryLevel(now);  // sends battery level to computer ** Commented out for testing **

    if (now > 5000) // 5 second grace period
      unPairingTask(); // unpairs device

    if (anyChanged) // sends report if any button/hat/axis state has changed and send the battery level
      {
        lastInputTime = now;
        bleGamepad.sendReport();  
      }

      digitalWrite(LED_BUILTIN, HIGH);
    
    vTaskDelayUntil(&lastWake, pollInterval);
  }
}