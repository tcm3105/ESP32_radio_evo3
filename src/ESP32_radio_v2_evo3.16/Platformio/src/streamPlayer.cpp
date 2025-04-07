
#include "streamPlayer.h"
#include "config.h"

// Obsługa wyświetlacza dla odtwarzanego strumienia radia internetowego
void StreamPlayer::displayRadio()
{
  if (displayMode == 0)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_fub14_tf);
    // stationName = stationName.substring(0, stationNameLenghtCut - 1);
    // u8g2.drawStr(24, 16, stationName.c_str());
    u8g2.drawStr(24, 16, stationName.substring(0, stationNameLenghtCut - 1).c_str());
    u8g2.drawRBox(1, 1, 21, 16, 4); // Rbox pod numerem stacji

    // Funkcja wyswietlania numeru Banku na dole ekranu
    u8g2.setFont(spleen6x12PL);
    char BankStr[8];
    snprintf(BankStr, sizeof(BankStr), "Bank%02d", bank_nr); // Formatujemy numer banku do postacji 00
    // Wyswietlamy numer Banku w dolnej linijce
    u8g2.drawBox(154, 54, 1, 12); // dorysowujemy 1px pasek przed napisem "Bank" dla symetrii
    u8g2.setDrawColor(0);
    u8g2.setCursor(155, 63); // Pozycja napisu Bank0x na dole ekranu
    u8g2.print(BankStr);

    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_spleen8x16_mr);
    char StationNrStr[3];
    snprintf(StationNrStr, sizeof(StationNrStr), "%02d", station_nr); // Formatowanie informacji o stacji i banku do postaci 00
    u8g2.setCursor(4, 14);                                            // Pozycja numeru stacji na gorze po lewej ekranu
    u8g2.print(StationNrStr);
    u8g2.setDrawColor(1);

    u8g2.setFont(spleen6x12PL);

    // Jesli stacja nie nadaje stationString to podmieniamy pusty stationString na nazwę staji - stationNameStream
    if (stationString == "") // Jeżeli stationString jest pusty i stacja go nie nadaje
    {
      if (stationNameStream == "") // jezeli nie ma równiez stationName
      {
        stationStringScroll = "---";
      } // wstawiamy trzy kreseczki do wyswietlenia
      else // jezeli jest station name to prawiamy w "-- NAZWA --" i wysylamy do scrollera
      {
        stationStringScroll = ("-- " + stationNameStream + " --");
      } // Zmienna stationStringScroller przyjmuje wartość stationNameStream
    }
    else // Jezeli stationString zawiera dane to przypisujemy go do stationStringScroll do funkcji scrollera
    {
      toolsClass.processText(stationString);          // przetwarzamy polsie znaki
      stationStringScroll = stationString + "      "; // dodajemy separator do przewijanego tekstu
    }

    Serial.print("debug -> Display0 (ekran radio) stationStringScroll: ");
    Serial.println(stationStringScroll);

    // Liczymy długość napisu stationStringScroll
    stationStringScrollWidth = stationStringScroll.length() * 6;

    u8g2.drawLine(0, 52, 255, 52);

    // Przeliczamy Hz na kHz
    int SampleRate = sampleRateString.toInt();
    int SampleRateRest = SampleRate % 1000;
    SampleRateRest = SampleRateRest / 100;
    SampleRate = SampleRate / 1000;

    // String displayString = sampleRateString.substring(1) + "Hz " + bitsPerSampleString + "bit " + bitrateString + "Kbps";
    String displayString = String(SampleRate) + "." + String(SampleRateRest) + "kHz " + bitsPerSampleString + "bit " + bitrateString + "kbps";
    // String displayString = String(SampleRate) + "." + String(SampleRateRest) + "k " + bitsPerSampleString + "b " + bitrateString + "k";
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(0, 63, displayString.c_str());
    // u8g2.sendBuffer();
  }
  else if (displayMode == 1) // Tryb wświetlania zegara z 1 linijką radia na dole
  {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.setFont(spleen6x12PL);
    u8g2.drawLine(0, 50, 255, 50); // Linia separacyjna zegar, dolna linijka radia

    char StationNrStr[3];
    snprintf(StationNrStr, sizeof(StationNrStr), "%02d", station_nr);
    // stationName = stationName.substring(0, 25);
    int StationNameEnd = stationName.indexOf("  "); // Wycinamy nazwe stacji tylko do miejsca podwojnej spacji
    stationName = stationName.substring(0, StationNameEnd);

    if (stationString == "") // Jeżeli stationString jest pusty i stacja go nie nadaje
    {
      if (stationNameStream == "") // jezeli nie ma równiez stationName
      {
        stationStringScroll = String(StationNrStr) + "." + stationName + ", ---";
      } // wstawiamy trzy kreseczki do wyswietlenia
      else // jezeli jest brak "stationString" ale jest "stationName" to składamy NR.Nazwa stacji z pliku, nadawany stationNameStream + separator przerwy
      {
        stationStringScroll = String(StationNrStr) + "." + stationName + ", " + stationNameStream + "      ";
      }
    }
    else // stationString != "" -> ma wartość
    {
      toolsClass.processText(stationString); // przetwarzamy polsie znaki
      stationStringScroll = String(StationNrStr) + "." + stationName + ", " + stationString + "      ";
      Serial.println(stationStringScroll);
    }
    Serial.print("debug -> Display1 (zegar) stationStringScroll: ");
    Serial.println(stationStringScroll);

    // Liczymy długość napisu stationStringScrollWidth
    stationStringScrollWidth = stationStringScroll.length() * 6;
  }
  else if (displayMode == 2) // Tryb wświetlania mode 3
  {
    u8g2.clearBuffer();
    u8g2.setFont(spleen6x12PL);
    // stationName = stationName.substring(0, stationNameLenghtCut);
    // u8g2.drawStr(24, 11, stationName.c_str());
    u8g2.drawStr(24, 11, stationName.substring(0, stationNameLenghtCut).c_str()); // Przyciecie i wyswietlenie dzieki temu nie zmieniamy zawartosci zmiennej stationName
    u8g2.drawRBox(1, 1, 18, 13, 4);                                               // Rbox pod numerem stacji

    // Funkcja wyswietlania numeru Banku na dole ekranu
    char BankStr[8];
    snprintf(BankStr, sizeof(BankStr), "Bank%02d", bank_nr); // Formatujemy numer banku do postacji 00

    // Wyswietlamy numer Banku w dolnej linijce
    u8g2.drawBox(154, 54, 1, 12); // dorysowujemy 1px pasek przed napisem "Bank" dla symetrii
    u8g2.setDrawColor(0);
    u8g2.setCursor(155, 63); // Pozycja napisu Bank0x na dole ekranu
    u8g2.print(BankStr);

    u8g2.setDrawColor(0);
    char StationNrStr[3];
    snprintf(StationNrStr, sizeof(StationNrStr), "%02d", station_nr); // Formatowanie informacji o stacji i banku do postaci 00
    u8g2.setCursor(4, 11);                                            // Pozycja numeru stacji na gorze po lewej ekranu
    u8g2.print(StationNrStr);
    u8g2.setDrawColor(1);

    // Jesli stacja nie nadaje stationString to podmieniamy pusty stationString na nazwę staji - stationNameStream
    if (stationString == "") // Jeżeli stationString jest pusty i stacja go nie nadaje
    {
      if (stationNameStream == "") // jezeli nie ma równiez stationName
      {
        stationStringScroll = "---";
      } // wstawiamy trzy kreseczki do wyswietlenia
      else // jezeli jest station name to oprawiamy w "-- NAZWA --" i wysylamy do scrollera
      {
        stationStringScroll = ("-- " + stationNameStream + " --");
      } // Zmienna stationStringScroller przyjmuje wartość stationNameStream
    }
    else // Jezeli stationString zawiera dane to przypisujemy go do stationStringScroll do funkcji scrollera
    {
      toolsClass.processText(stationString); // przetwarzamy polsie znaki
      stationStringScroll = stationString;
    }

    u8g2.drawLine(0, 52, 255, 52);

    // Przeliczamy Hz na kHz
    int SampleRate = sampleRateString.toInt();
    int SampleRateRest = SampleRate % 1000;
    SampleRateRest = SampleRateRest / 100;
    SampleRate = SampleRate / 1000;

    String displayString = String(SampleRate) + "." + String(SampleRateRest) + "kHz " + bitsPerSampleString + "bit " + bitrateString + "kbps";
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(0, 63, displayString.c_str());
  }
}

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

// Funkcja do wyświetlania listy stacji radiowych z opcją wyboru poprzez zaznaczanie w negatywie
void StreamPlayer::displayStations()
{
  listedStations = true;
  u8g2.clearBuffer(); // Wyczyść bufor przed rysowaniem, aby przygotować ekran do nowej zawartości
  u8g2.setFont(spleen6x12PL);
  u8g2.setCursor(60, 10);                                         // Ustaw pozycję kursora (x=60, y=10) dla nagłówka
  u8g2.print("RADIO STATIONS:   ");                               // Wyświetl nagłówek "Radio Stations:"
  u8g2.print(String(station_nr) + " / " + String(stationsCount)); // Dodaj numer aktualnej stacji i licznik wszystkich stacji

  int displayRow = 1; // Zmienna dla numeru wiersza, zaczynając od drugiego (pierwszy to nagłówek)

  // erial.print("FirstVisibleLine:");
  // Serial.print(firstVisibleLine);

  // Wyświetlanie stacji, zaczynając od drugiej linii (y=21)
  for (int i = firstVisibleLine; i < min(firstVisibleLine + maxVisibleLines, stationsCount); i++)
  {
    char station[STATION_NAME_LENGTH + 1]; // Tablica na nazwę stacji o maksymalnej długości zdefiniowanej przez STATION_NAME_LENGTH
    memset(station, 0, sizeof(station));   // Wyczyszczenie tablicy zerami przed zapisaniem danych

    // Odczytaj długość nazwy stacji z PSRAM dla bieżącego indeksu stacji
    int length = psramData[i * (STATION_NAME_LENGTH + 1)]; //----------------------------------------------

    // Odczytaj nazwę stacji z PSRAM jako ciąg bajtów, maksymalnie do STATION_NAME_LENGTH
    for (int j = 0; j < min(length, STATION_NAME_LENGTH); j++)
    {
      station[j] = psramData[i * (STATION_NAME_LENGTH + 1) + 1 + j]; // Odczytaj znak po znaku nazwę stacji
    }

    // Sprawdź, czy bieżąca stacja to ta, która jest aktualnie zaznaczona
    if (i == currentSelection)
    {
      u8g2.setDrawColor(1);                          // Ustaw biały kolor rysowania
      u8g2.drawBox(0, displayRow * 13 - 2, 256, 13); // Narysuj prostokąt jako tło dla zaznaczonej stacji (x=0, szerokość 256, wysokość 10)
      u8g2.setDrawColor(0);                          // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji
    }
    else
    {
      u8g2.setDrawColor(1); // Dla niezaznaczonych stacji ustaw zwykły biały kolor tekstu
    }
    // Wyświetl nazwę stacji, ustawiając kursor na odpowiedniej pozycji
    u8g2.drawStr(0, displayRow * 13 + 8, String(station).c_str());
    // u8g2.print(station);  // Wyświetl nazwę stacji

    // Przejdź do następnej linii (następny wiersz na ekranie)
    displayRow++;
  }
  // Przywróć domyślne ustawienia koloru rysowania (biały tekst na czarnym tle)
  u8g2.setDrawColor(1); // Biały kolor rysowania
  u8g2.sendBuffer();    // Wyślij zawartość bufora do ekranu OLED, aby wyświetlić zmiany
}

void StreamPlayer::changeStation()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);          // cziocnka 14x11
  u8g2.drawStr(34, 33, "Loading stream..."); // 8 znakow  x 11 szer
  u8g2.sendBuffer();

  mp3 = flac = aac = vorbis = false;
  stationFromBuffer = station_nr;
  stationString.remove(0); // Usunięcie wszystkich znaków z obiektu stationString
  stationNameStream.remove(0);

  Serial.println("debug-- Read station from PSRAM");
  String stationUrl = "";

  // Odczyt stacji pod daną komórka pamieci PSRAM:
  char station[STATION_NAME_LENGTH + 1];                                // Tablica na nazwę stacji o maksymalnej długości zdefiniowanej przez STATION_NAME_LENGTH
  memset(station, 0, sizeof(station));                                  // Wyczyszczenie tablicy zerami przed zapisaniem danych
  int length = psramData[(station_nr - 1) * (STATION_NAME_LENGTH + 1)]; // Odczytaj długość nazwy stacji z PSRAM dla bieżącego indeksu stacji

  for (int j = 0; j < min(length, STATION_NAME_LENGTH); j++)
  {                                                                               // Odczytaj nazwę stacji z PSRAM jako ciąg bajtów, maksymalnie do STATION_NAME_LENGTH
    station[j] = psramData[(station_nr - 1) * (STATION_NAME_LENGTH + 1) + 1 + j]; // Odczytaj znak po znaku nazwę stacji
  }

  String line = String(station); // Przypisujemy dane odczytane z PSRAM do zmiennej line

  // Wyciągnij pierwsze 42 znaki i przypisz do stationName
  stationName = line.substring(0, 41); // 42 Skopiuj pierwsze 42 znaki z linii
  Serial.print("Nazwa stacji: ");
  Serial.println(stationName);

  // Znajdź część URL w linii, np. po numerze stacji
  int urlStart = line.indexOf("http"); // Szukamy miejsca, gdzie zaczyna się URL
  if (urlStart != -1)
  {
    stationUrl = line.substring(urlStart); // Wyciągamy URL od "http"
    stationUrl.trim();                     // Usuwamy białe znaki na początku i końcu
  }
  else
  {
    return;
  }

  if (stationUrl.isEmpty()) // jezeli link URL jest pusty
  {
    Serial.println("Błąd: Nie znaleziono stacji dla podanego numeru.");
    return;
  }

  // Weryfikacja, czy w linku znajduje się "http" lub "https"
  if (stationUrl.startsWith("http://") || stationUrl.startsWith("https://"))
  {
    // Wydrukuj nazwę stacji i link na serialu
    Serial.print("Aktualnie wybrana stacja: ");
    Serial.println(station_nr);
    Serial.print("Link do stacji: ");
    Serial.println(stationUrl);

    u8g2.setFont(spleen6x12PL); // wypisujemy jaki stream jakie stacji jest ładowany
    u8g2.drawStr(34, 55, String(stationName.substring(0, stationNameLenghtCut)).c_str());
    u8g2.sendBuffer();
    // Połącz z daną stacją
    audio.connecttohost(stationUrl.c_str());
    stationFromBuffer = station_nr;
    bankFromBuffer = bank_nr;

    configClass.saveStationOnSD(); // Zapisujemy jaki numer stacji i który bank gramy
  }
  else
  {
    Serial.println("Błąd: link stacji nie zawiera 'http' lub 'https'");
    Serial.println("Odczytany URL: " + stationUrl);
  }
  currentSelection = station_nr - 1;       // ustawiamy stacje na liscie na obecnie odtwarzaczną po zmianie stacji
  firstVisibleLine = currentSelection + 1; // pierwsza widoczna lina to grająca stacja przy starcie
  if (currentSelection + 1 >= stationsCount - 1)
  {
    firstVisibleLine = currentSelection - 3;
  }
}
