#include "keyboard.h"

uint8_t Keyboard::handleKeyboard()
{
  // for (;;) {
  //  vTaskDelay(50);
  // int keyboardValue = analogRead(keyboardPin); // Odczyt wartości analogowej przycisku
  uint8_t key = 17;

  for (int i = 0; i < 32; i++)
  {
    keyboardValue = keyboardValue + analogRead(keyboardPin);
  }
  keyboardValue = keyboardValue / 32;

  if (debugKeyboard == 1)
  {
    Serial.print("debug - ADC odczyt: ");
    Serial.print(keyboardValue);
    Serial.print(" flaga przycisk wcisniety:");
    Serial.println(keyboardButtonPressed);
  }
  // Kasujemy flage nacisnietego przycisku tylko jesli nic nie jest wcisnięte
  if (keyboardValue > keyboardButtonNeutral - keyboardButtonThresholdTolerance)
  {
    keyboardButtonPressed = false;
  }
  // Jesli nic nie jest wcisniete i nic nie było nacisnięte analizujemy przycisk
  if (keyboardValue < keyboardButtonNeutral - keyboardButtonThresholdTolerance)
  {
    if (keyboardButtonPressed == false)
    {
      // Sprawdzamy stan klawiatury
      if ((keyboardValue <= keyboardButtonThreshold_1 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_1 - keyboardButtonThresholdTolerance))
      {
        key = 1;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_2 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_2 - keyboardButtonThresholdTolerance))
      {
        key = 2;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_3 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_3 - keyboardButtonThresholdTolerance))
      {
        key = 3;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_4 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_4 - keyboardButtonThresholdTolerance))
      {
        key = 4;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_5 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_5 - keyboardButtonThresholdTolerance))
      {
        key = 5;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_6 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_6 - keyboardButtonThresholdTolerance))
      {
        key = 6;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_7 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_7 - keyboardButtonThresholdTolerance))
      {
        key = 7;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_8 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_8 - keyboardButtonThresholdTolerance))
      {
        key = 8;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_9 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_9 - keyboardButtonThresholdTolerance))
      {
        key = 9;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_0 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_0 - keyboardButtonThresholdTolerance))
      {
        key = 0;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Memory + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Memory - keyboardButtonThresholdTolerance))
      {
        key = 11;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Shift + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Shift - keyboardButtonThresholdTolerance))
      {
        key = 12;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Auto + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Auto - keyboardButtonThresholdTolerance))
      {
        key = 13;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Band + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Band - keyboardButtonThresholdTolerance))
      {
        key = 14;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Mute + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Mute - keyboardButtonThresholdTolerance))
      {
        key = 15;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Scan + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Scan - keyboardButtonThresholdTolerance))
      {
        key = 16;
        keyboardButtonPressed = true;
      }

      if ((key < 17) && (keyboardButtonPressed == true))
      {
        Serial.print("ADC odczyt poprawny: ");
        Serial.print(keyboardValue);
        Serial.print(" przycisk: ");
        Serial.println(key);
        keyboardValue = 0;

        return key;

      }
    }
  }
  return key;
}


