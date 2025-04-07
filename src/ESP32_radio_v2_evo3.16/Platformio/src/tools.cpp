#include "tools.h"
#include "config.h"

void Tools::processText(String &text)
{
    for (int i = 0; i < text.length(); i++)
    {
        switch (text[i])
        {
        case (char)0xC2:
            switch (text[i + 1])
            {
            case (char)0xB3:
                break;
            case (char)0x9C:
                break;
            case (char)0x8C:
                break;
            case (char)0xB9:
                break;
            case (char)0x9B:
                break;
            case (char)0xBF:
                break;
            case (char)0x9F:
                break;
            }
            break;
        case (char)0xC3:
            switch (text[i + 1])
            {
            case (char)0xB1:
                break;
            case (char)0xB3:
                break;
            case (char)0xBA:
                break;
            case (char)0xBB:
                break;
            case (char)0x93:
                break;
            }
            break;
        case (char)0xC4:
            switch (text[i + 1])
            {
            case (char)0x85:
                break;
            case (char)0x99:
                break;
            case (char)0x87:
                break;
            case (char)0x84:
                break;
            case (char)0x98:
                break;
            case (char)0x86:
                break;
            }
            break;
        case (char)0xC5:
            switch (text[i + 1])
            {
            case (char)0x82:
                break;
            case (char)0x84:
                break;
            case (char)0x9B:
                break;
            case (char)0xBB:
                break;
            case (char)0xBC:
                break;
            case (char)0x83:
                break;
            case (char)0x9A:
                break;
            case (char)0x81:
                break;
            case (char)0xB9:
                break;
            case (char)0xBA:
                break;
            }
            break;
        }
    }
}

// Funkcja odwracania bitów MSL-LSB <-> LSB-MSB
uint32_t Tools::reverse_bits(uint32_t inval, int bits)
{
    if (bits > 0)
    {
        bits--;
        return reverse_bits(inval >> 1, bits) | ((inval & 1) << bits);
    }
    return 0;
}

void Tools::drawSignalPower(uint8_t xpwr, uint8_t ypwr, bool print)
{
  // Wartosci na podstawie ->  https://www.intuitibits.com/2016/03/23/dbm-to-percent-conversion/
  int signal_dBM[] = {-100, -99, -98, -97, -96, -95, -94, -93, -92, -91, -90, -89, -88, -87, -86, -85, -84, -83, -82, -81, -80, -79, -78, -77, -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65, -64, -63, -62, -61, -60, -59, -58, -57, -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, -46, -45, -44, -43, -42, -41, -40, -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1};
  int signal_percent[] = {0, 0, 0, 0, 0, 0, 4, 6, 8, 11, 13, 15, 17, 19, 21, 23, 26, 28, 30, 32, 34, 35, 37, 39, 41, 43, 45, 46, 48, 50, 52, 53, 55, 56, 58, 59, 61, 62, 64, 65, 67, 68, 69, 71, 72, 73, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 90, 91, 92, 93, 93, 94, 95, 95, 96, 96, 97, 97, 98, 98, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};

  int signalpwr = WiFi.RSSI();
  uint8_t signalLevel = 0;

  // Pionowe kreseczki, szerokosc 11px

  // Czyscimy obszar pod kreseczkami
  u8g2.setDrawColor(0);
  u8g2.drawBox(xpwr, ypwr - 10, 11, 11);
  u8g2.setDrawColor(1);

  // Rysujemy podstawy 1x1px pod kazdą kreseczką
  u8g2.drawBox(xpwr, ypwr - 1, 1, 1);
  u8g2.drawBox(xpwr + 2, ypwr - 1, 1, 1);
  u8g2.drawBox(xpwr + 4, ypwr - 1, 1, 1);
  u8g2.drawBox(xpwr + 6, ypwr - 1, 1, 1);
  u8g2.drawBox(xpwr + 8, ypwr - 1, 1, 1);
  u8g2.drawBox(xpwr + 10, ypwr - 1, 1, 1);

  if (WiFi.status() == WL_CONNECTED)
  {
    // Rysujemy kreseczki
    if (signalpwr > -88)
    {
      signalLevel = 1;
      u8g2.drawBox(xpwr, ypwr - 2, 1, 1);
    } // 0-14
    if (signalpwr > -81)
    {
      signalLevel = 2;
      u8g2.drawBox(xpwr + 2, ypwr - 3, 1, 2);
    } // > 28
    if (signalpwr > -74)
    {
      signalLevel = 3;
      u8g2.drawBox(xpwr + 4, ypwr - 4, 1, 3);
    } // > 42
    if (signalpwr > -66)
    {
      signalLevel = 4;
      u8g2.drawBox(xpwr + 6, ypwr - 5, 1, 4);
    } // > 56
    if (signalpwr > -57)
    {
      signalLevel = 5;
      u8g2.drawBox(xpwr + 8, ypwr - 6, 1, 5);
    } // > 70
    if (signalpwr > -50)
    {
      signalLevel = 6;
      u8g2.drawBox(xpwr + 10, ypwr - 8, 1, 7);
    } // > 84
  }

  if (print == true) // Jesli flaga print =1 to wypisujemy na serialu sile sygnału w % i w skali 1-6
  {
    for (int j = 0; j < 100; j++)
    {
      if (signal_dBM[j] == signalpwr)
      {
        Serial.print("Sygnału WiFi: ");
        Serial.print(signal_percent[j]);
        Serial.print("%  Poziom: ");
        Serial.print(signalLevel);
        Serial.print("   dBm: ");
        Serial.println(signalpwr);

        break;
      }
    }
  }
}

void Tools::encoderFunctionOrderChange()
{
  displayActive = true;
  displayStartTime = millis();
  volumeSet = false;
  timeDisplay = false;
  bankMenuEnable = false;
  encoderFunctionOrder = !encoderFunctionOrder;
  u8g2.clearBuffer();
  u8g2.setFont(spleen6x12PL);
  u8g2.drawStr(1, 14, "Encoder function order change:");
  if (encoderFunctionOrder == false)
  {
    u8g2.drawStr(1, 28, "Rotate for volume, press for station list");
  }
  if (encoderFunctionOrder == true)
  {
    u8g2.drawStr(1, 28, "Rotate for station list, press for volume");
  }
  u8g2.sendBuffer();
}

void Tools::displayDimmer(bool dimmerON)
{
  if ((dimmerON == 1) && (displayBrightness == 15) && (displayAutoDimmerOn == true))
  {
    u8g2.sendF("ca", 0xC7, dimmerDisplayBrightness);
  }
  if (dimmerON == 0)
  {
    u8g2.sendF("ca", 0xC7, displayBrightness);
    displayDimmerTimeCounter = 0;
  }
}

void Tools::vuMeter()
{
  vuMeterR = min(audio.getVUlevel() & 0xFF, 250); // wyciagamy ze zmiennej typu int16 kanał L
  vuMeterL = min(audio.getVUlevel() >> 8, 250);   // z wyzszej polowki wyciagamy kanal P

  // vuMeterL = (vuMeterL >> 1); // dzielimy przez 2 -> przesuniecie o jeden bit abyz  255 -> 64
  // vuMeterR = (vuMeterR >> 1);

  if (volumeMute == false)
  {
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 41, 253, 3); // czyszczenie ekranu pod VU meter
    u8g2.drawBox(0, 46, 253, 3);
    u8g2.setDrawColor(1);

    if (vuMeterMode == 1) // tryb 1 ciagle paski
    {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 41, vuMeterL, 3); // rysujemy kreseczki o dlugosci odpowiadajacej wartosci VU
      u8g2.drawBox(0, 46, vuMeterR, 3);
    }
    else // vuMeterMode == 0  tryb podstawowy, kreseczki z przerwami
    {
      for (uint8_t vusize = 0; vusize < vuMeterL; vusize++)
      {
        u8g2.drawBox(vusize, 41, 8, 2);
        vusize = vusize + 8;
      }
      for (uint8_t vusize = 0; vusize < vuMeterR; vusize++)
      {
        u8g2.drawBox(vusize, 46, 8, 2);
        vusize = vusize + 8;
      }
    }
  }
}