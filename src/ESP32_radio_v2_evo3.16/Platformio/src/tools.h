#ifndef TOOLS_H_
#define TOOLS_H_

#include <Arduino.h>
#include <WiFiManager.h> 
#include "Audio.h"

extern Audio audio;

class Tools
{
public:
    void processText(String &text);
    uint32_t reverse_bits(uint32_t inval, int bits);
    void drawSignalPower(uint8_t xpwr, uint8_t ypwr, bool print);
    void encoderFunctionOrderChange();
    void displayDimmer(bool dimmerON);
    void vuMeter();
};

#endif