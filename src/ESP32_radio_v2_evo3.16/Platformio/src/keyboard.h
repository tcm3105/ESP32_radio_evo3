#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#include <Arduino.h>

extern const int keyboardPin;

// ---- Progi przełaczania ADC dla klawiatury matrycowej 5x3 w tunrze Sony ST-120 ---- //
const int keyboardButtonThresholdTolerance = 20; // Tolerancja dla pomiaru ADC
const int keyboardButtonNeutral = 4095;          // Pozycja neutralna
const int keyboardButtonThreshold_0 = 2375;      // Przycisk 0
const int keyboardButtonThreshold_1 = 10;        // Przycisk 1
const int keyboardButtonThreshold_2 = 545;       // Przycisk 2
const int keyboardButtonThreshold_3 = 1390;      // Przycisk 3
const int keyboardButtonThreshold_4 = 1925;      // Przycisk 4
const int keyboardButtonThreshold_5 = 2285;      // Przycisk 5
const int keyboardButtonThreshold_6 = 385;       // Przycisk 6
const int keyboardButtonThreshold_7 = 875;       // Przycisk 7
const int keyboardButtonThreshold_8 = 1585;      // Przycisk 8
const int keyboardButtonThreshold_9 = 2055;      // Przycisk 9
const int keyboardButtonThreshold_Shift = 2455;  // Shift - funkcja Enter/OK
const int keyboardButtonThreshold_Memory = 2170; // Memory - funkcja Bank menu
const int keyboardButtonThreshold_Band = 1640;   // Przycisk Band - funkcja Back
const int keyboardButtonThreshold_Auto = 730;    // Przycisk Auto - przelacza Radio/Zegar
const int keyboardButtonThreshold_Scan = 1760;   // Przycisk Scan - funkcja Dimmer ekranu OLED
const int keyboardButtonThreshold_Mute = 1130;   // Przycisk Mute - funkcja MUTE

// Flagi do monitorowania stanu klawiatury
extern unsigned long keyboardValue;
extern unsigned long keyboardLastSampleTime;
extern unsigned long keyboardSampleDelay;
extern bool keyboardButtonPressed; // Wcisnięcie klawisza
extern bool debugKeyboard;

class Keyboard
{
public:
    uint8_t handleKeyboard();
};

#endif