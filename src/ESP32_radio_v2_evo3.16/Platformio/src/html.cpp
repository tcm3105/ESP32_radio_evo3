#include "html.h"

void Html::webUrlStationPlay()
{
  audio.stopSong();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);          // cziocnka 14x11
  u8g2.drawStr(34, 33, "Loading stream..."); // 8 znakow  x 11 szer
  u8g2.sendBuffer();

  mp3 = flac = aac = vorbis = false;

  Serial.println("debug-- Read station from WEB URL");

  if (url2play != "")
  {
    url2play.trim(); // Usuwamy białe znaki na początku i końcu
  }
  else
  {
    return;
  }

  if (url2play.isEmpty()) // jezeli link URL jest pusty
  {
    Serial.println("Błąd: Nie znaleziono stacji dla podanego numeru.");
    return;
  }

  // Weryfikacja, czy w linku znajduje się "http" lub "https"
  if (url2play.startsWith("http://") || url2play.startsWith("https://"))
  {
    // Wydrukuj nazwę stacji i link na serialu
    Serial.print("Link do stacji: ");
    Serial.println(url2play);

    u8g2.setFont(spleen6x12PL); // wypisujemy jaki stream jakie stacji jest ładowany
    u8g2.drawStr(34, 55, String(url2play).c_str());
    u8g2.sendBuffer();

    // Połącz z daną stacją
    audio.connecttohost(url2play.c_str());
  }
  else
  {
    Serial.println("Błąd: link stacji nie zawiera 'http' lub 'https'");
    Serial.println("Odczytany URL: " + url2play);
  }
  station_nr = 0;
  bank_nr = 0;
  stationName = stationNameStream;
  url2play = "";
}

void Html::stationBankListHtmlMobile()
{
  html = "";
  html += "<p>";
  for (int i = 1; i < 17; i++)
  {

    if (i == bank_nr)
    {
      html += "<button class=\"buttonBankSelected\" onClick=\"bankLoad('" + String(i) + "');\" id=\"Bank\" )>" + String(i) + "</button>" + String("\n");
    }
    else
    {
      html += "<button class=\"buttonBank\" onClick=\"bankLoad('" + String(i) + "');\" id=\"Bank\" )>" + String(i) + "</button>" + String("\n");
    }
    if (i == 8)
    {
      html += "</p><p>";
    }
  }
  html += "</p>";
  html += "<center>";

  html += "<table>";
  // html += "<tr><th colspan=\"2\" align=\"left\">Bank " + String(bank_nr) + " stations:</th></tr>";

  for (int i = 0; i < stationsCount; i++)
  {
    char station[STATION_NAME_LENGTH + 1]; // Tablica na nazwę stacji o maksymalnej długości zdefiniowanej przez STATION_NAME_LENGTH
    memset(station, 0, sizeof(station));   // Wyczyszczenie tablicy zerami przed zapisaniem danych
    int length = psramData[i * (STATION_NAME_LENGTH + 1)];
    for (int j = 0; j < min(length, STATION_NAME_LENGTH); j++)
    {
      station[j] = psramData[i * (STATION_NAME_LENGTH + 1) + 1 + j]; // Odczytaj znak po znaku nazwę stacji
    }

    html += "<tr>";

    if (i + 1 == station_nr)
    {
      html += "<td><p class='stationNumberListSelected'><b>" + String(i + 1) + "</b></p></td>";
      html += "<td><p class='stationListSelected' onClick=\"stationLoad('" + String(i + 1) + "');\"><b> " + String(station).substring(0, stationNameLenghtCut) + "</b></p></td>";
    }
    else
    {
      html += "<td><p class='stationNumberList'>" + String(i + 1) + "</p></td>";
      html += "<td><p class='stationList' onClick=\"stationLoad('" + String(i + 1) + "');\">" + String(station).substring(0, stationNameLenghtCut) + "</p></td>";
    }

    html += "</tr>" + String("\n");
  }
  html += "</table></div>";
  html += "<p style=\"font-size: 0.8rem;\">Web Radio, mobile, Evo: " + softwareRev + "</p>";
  html += "</center></body></html>";
}

void Html::stationBankListHtmlPC()
{
  html = "";
  html += "<p>";
  for (int i = 1; i < 17; i++)
  {

    if (i == bank_nr)
    {
      html += "<button class=\"buttonBankSelected\" onClick=\"bankLoad('" + String(i) + "');\" id=\"Bank\" )>" + String(i) + "</button>" + String("\n");
    }
    else
    {
      html += "<button class=\"buttonBank\" onClick=\"bankLoad('" + String(i) + "');\" id=\"Bank\" )>" + String(i) + "</button>" + String("\n");
    }
  }

  html += "</p>";
  html += "<center>" + String("\n");
  html += "<div class=\"column\">" + String("\n");
  for (int i = 0; i < MAX_STATIONS; i++)
  // for (int i = 0; i < stationsCount; i++)
  {
    char station[STATION_NAME_LENGTH + 1]; // Tablica na nazwę stacji o maksymalnej długości zdefiniowanej przez STATION_NAME_LENGTH
    memset(station, 0, sizeof(station));   // Wyczyszczenie tablicy zerami przed zapisaniem danych

    int length = psramData[i * (STATION_NAME_LENGTH + 1)];
    for (int j = 0; j < min(length, STATION_NAME_LENGTH); j++)
    {
      station[j] = psramData[i * (STATION_NAME_LENGTH + 1) + 1 + j]; // Odczytaj znak po znaku nazwę stacji
    }

    if ((i == 0) || (i == 25) || (i == 50) || (i == 75))
    {
      html += "<table>" + String("\n");
      // html += "<tr><th>No</th><th>Station</th></tr>" + String("\n");
    }

    // 0-98   >98
    if (i >= stationsCount)
    {
      station[0] = '\0';
    } // Jesli mamy mniej niz 99 stacji to wypełniamy pozostałe komórki pustymi wartościami

    if (i + 1 == station_nr)
    {
      html += "<tr>";
      html += "<td><p class='stationNumberListSelected'><b>" + String(i + 1) + "</b></p></td>";
      html += "<td><p class='stationListSelected' onClick=\"stationLoad('" + String(i + 1) + "');\"><b> " + String(station).substring(0, stationNameLenghtCut) + "</b></p></td>";
      html += "</tr>" + String("\n");
    }
    else
    {
      html += "<tr>";
      html += "<td><p class='stationNumberList'>" + String(i + 1) + "</p></td>";
      html += "<td><p class='stationList' onClick=\"stationLoad('" + String(i + 1) + "');\">" + String(station).substring(0, stationNameLenghtCut) + "</p></td>";
      html += "</tr>" + String("\n");
    }

    if ((i == 24) || (i == 49) || (i == 74)) //||(i == 98))
    {
      html += "</table>" + String("\n");
    }
  }

  html += "</table>" + String("\n");
  html += "</div>" + String("\n");
  html += "<p style=\"font-size: 0.8rem;\">Web Radio, desktop, Evo: " + softwareRev + "</p>" + String("\n");
  html += "</center></body></html>";
}

