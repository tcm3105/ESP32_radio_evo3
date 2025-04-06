
#include "streamPlayer.h"
#include "config.h"

void StreamPlayer::bankMenuDisplay()
{
  displayStartTime = millis();
  Serial.println("debug--BankMenuDisplay");
  volumeSet = false;
  // previous_bank_nr = bank_nr;  // jesli weszlimy do menu "wybór banku" to zapisujemy obecny bank zanim zaczniemy krecic enkoderem
  bankMenuEnable = true;
  timeDisplay = false;
  displayActive = true;
  // bankChange = true;
  // currentOption = BANK_LIST;  // Ustawienie listy banków do przewijania i wyboru
  String bankNrStr = String(bank_nr);
  Serial.println("Wyświetlenie listy banków");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);
  u8g2.drawStr(80, 33, "BANK ");
  u8g2.drawStr(145, 33, String(bank_nr).c_str()); // numer banku
  if ((bankNetworkUpdate == true) || (noSDcard == true))
  {
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(185, 24, "NETWORK ");
    u8g2.drawStr(188, 34, "UPDATE  ");

    if (noSDcard == true)
    {
      // u8g2.drawStr(24, 24, "NO SD");
      u8g2.drawStr(24, 34, "NO CARD");
    }
  }

  u8g2.drawRFrame(21, 42, 214, 14, 3);               // Ramka do slidera bankow
  u8g2.drawRBox((bank_nr * 13) + 10, 44, 15, 10, 2); // wypełnienie slidera
  u8g2.sendBuffer();
}