#ifndef IR_H_
#define IR_H_

#include "config.h"
#include "streamPlayer.h" // Include the header for streamPlayer
extern StreamPlayer streamPlayerClass; // Declare streamPlayer as an external object

// ----------- PILOT IR ----------- //
// Przypisanie przycisków i adresu pilota w standardzie NEC
// pierwszy bajt adres, drugi komenda (B914 - adres B9 komenda 14)

#define rcCmdVolumeUp 0xB914   // Głosnosc +
#define rcCmdVolumeDown 0xB915 // Głośnosc -
#define rcCmdArrowRight 0xB90B // strzałka w prawo - nastepna stacja
#define rcCmdArrowLeft 0xB90A  // strzałka w lewo - poprzednia stacja
#define rcCmdArrowUp 0xB987    // strzałka w góre - lista stacji krok do gory
#define rcCmdArrowDown 0xB986  // strzałka w dół - lista stacj krok na dół
#define rcCmdBack 0xB985       // Przycisk powrotu
#define rcCmdOk 0xB90E         // Przycisk Ent - zatwierdzenie stacji
#define rcCmdSrc 0xB913        // Przełączanie źródła radio, odtwarzacz
#define rcCmdMute 0xB916       // Wyciszenie dzwieku
#define rcCmdAud 0xB917        // Equalizer dzwieku
#define rcCmdDirect 0xB90F     // Janość ekranu, dwa tryby 1/16 lub pełna janość
#define rcCmdBankMinus 0xB90C  // Wysweitla wybór banku
#define rcCmdBankPlus 0xB90D   // Wysweitla wybór banku
#define rcCmdRed 0xB988        // Przełacza ładowanie banku kartaSD - serwer GitHub w menu bank
#define rcCmdGreen 0xB992      // VU wyłaczony, VU tryb 1, VU tryb 2, zegar
#define rcCmdKey0 0xB900       // Przycisk "0"
#define rcCmdKey1 0xB901       // Przycisk "1"
#define rcCmdKey2 0xB902       // Przycisk "2"
#define rcCmdKey3 0xB903       // Przycisk "3"
#define rcCmdKey4 0xB904       // Przycisk "4"
#define rcCmdKey5 0xB905       // Przycisk "5"
#define rcCmdKey6 0xB906       // Przycisk "6"
#define rcCmdKey7 0xB907       // Przycisk "7"
#define rcCmdKey8 0xB908       // Przycisk "8"
#define rcCmdKey9 0xB909       // Przycisk "9"

class Ir
{
public:
    void rcInputKey(uint8_t i);
};

#endif