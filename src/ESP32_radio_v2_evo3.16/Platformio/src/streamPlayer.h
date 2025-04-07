#ifndef STREAMPLAYER_H_
#define STREAMPLAYER_H_

#include <Arduino.h>
#include "Audio.h"
#include "config.h"
#include "tools.h"

extern Audio audio;
extern Config configClass;
extern Tools toolsClass;
class StreamPlayer
{
public:
    void displayRadio();
    void bankMenuDisplay();
    void displayStations();
    void changeStation();
};

#endif