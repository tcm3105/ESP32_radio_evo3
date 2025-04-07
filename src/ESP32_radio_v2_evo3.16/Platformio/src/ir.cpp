
#include "ir.h"

void Ir::rcInputKey(uint8_t i)
{
  rcInputDigitsMenuEnable = true;
  if (bankMenuEnable == true)
  {

    if (i == 0)
    {
      i = 10;
    }
    bank_nr = i;
    streamPlayerClass.bankMenuDisplay();
  }
  else
  {
    timeDisplay = false;
    displayActive = true;
    displayStartTime = millis();

    if (rcInputDigit1 == 0xFF)
    {
      rcInputDigit1 = i;
    }
    else
    {
      rcInputDigit2 = i;
    }

    int y = 35;
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_fub14_tf); // cziocnka 14x11
    u8g2.drawStr(65, y, "Station:");
    if (rcInputDigit1 != 0xFF)
    {
      u8g2.drawStr(153, y, String(rcInputDigit1).c_str());
    }
    else
    {
      u8g2.drawStr(153, displayPositionX, "_");
    }

    if (rcInputDigit2 != 0xFF)
    {
      u8g2.drawStr(164, y, String(rcInputDigit2).c_str());
    }
    else
    {
      u8g2.drawStr(164, y, "_");
    }

    if ((rcInputDigit1 != 0xFF) && (rcInputDigit2 != 0xFF)) // jezeli obie wartosci nie są puste
    {
      station_nr = (rcInputDigit1 * 10) + rcInputDigit2;
    }
    else if ((rcInputDigit1 != 0xFF) && (rcInputDigit2 == 0xFF)) // jezeli tylko podalismy jedna cyfrę
    {
      station_nr = rcInputDigit1;
    }

    if (station_nr > stationsCount) // sprawdzamy czy wprowadzona wartość nie wykracza poza licze stacji w danym banku
    {
      station_nr = stationsCount; // jesli wpisana wartość jest wieksza niz ilosc stacji to ustawiamy war
    }

    if (station_nr < 1)
    {
      station_nr = stationFromBuffer;
    }

    // Odczyt stacji pod daną komórka pamieci PSRAM:
    char station[STATION_NAME_LENGTH + 1];                                // Tablica na nazwę stacji o maksymalnej długości zdefiniowanej przez STATION_NAME_LENGTH
    memset(station, 0, sizeof(station));                                  // Wyczyszczenie tablicy zerami przed zapisaniem danych
    int length = psramData[(station_nr - 1) * (STATION_NAME_LENGTH + 1)]; // Odczytaj długość nazwy stacji z PSRAM dla bieżącego indeksu stacji

    for (int j = 0; j < min(length, STATION_NAME_LENGTH); j++)
    {                                                                               // Odczytaj nazwę stacji z PSRAM jako ciąg bajtów, maksymalnie do STATION_NAME_LENGTH
      station[j] = psramData[(station_nr - 1) * (STATION_NAME_LENGTH + 1) + 1 + j]; // Odczytaj znak po znaku nazwę stacji
    }
    u8g2.setFont(spleen6x12PL);
    String stationNameText = String(station);
    stationNameText = stationNameText.substring(0, 25); // Przycinamy do 23 znakow

    u8g2.drawLine(0, 48, 256, 48);
    u8g2.setFont(spleen6x12PL);
    u8g2.setCursor(0, 60);
    u8g2.print("Bank:" + String(bank_nr) + ", 1-" + String(stationsCount) + "     " + stationNameText);
    u8g2.sendBuffer();

    if ((rcInputDigit1 != 0xFF) && (rcInputDigit2 != 0xFF)) // jezeli wpisalismy obie cyfry to czyscimy pola aby mozna bylo je wpisac ponownie
    {
      rcInputDigit1 = 0xFF; // czyscimy cyfre 1, flaga pustej zmiennej F aby naciskajac kolejny raz mozna bylo wypisac cyfre bez czekania 6 sek
      rcInputDigit2 = 0xFF; // czyscimy cyfre 2, flaga pustej zmiennej F
    }
  }
}