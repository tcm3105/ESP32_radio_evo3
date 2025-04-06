#include <Arduino.h>
#include "config.h"

void Config::saveConfig()
{
    if (SD.exists("/config.txt"))
    {
        File myFile = SD.open("/config.txt", FILE_WRITE);
        if (myFile)
        {
            // ...existing code...
            myFile.close();
        }
        else
        {
            // ...existing code...
        }
    }
    else
    {
        File myFile = SD.open("/config.txt", FILE_WRITE);
        if (myFile)
        {
            // ...existing code...
            myFile.close();
        }
        else
        {
            // ...existing code...
        }
    }
}

void Config::readConfig()
{
    String fileName = "/config.txt";
    if (!SD.exists(fileName))
    {
        // ...existing code...
        return;
    }

    File configFile = SD.open(fileName, FILE_READ);
    if (!configFile)
    {
        // ...existing code...
        return;
    }

    while (configFile.available())
    {
        String line = configFile.readStringUntil('\n');
        int lineStart = line.indexOf("=");
        if (lineStart != -1)
        {
            // ...existing code...
        }
    }
    configFile.close();
}

void Config::displayConfig()
{
    displayStartTime = millis(); // Uaktulniamy czas dla funkcji auto-powrotu z menu
    equalizerMenuEnable = true;  // Ustawiamy flagę menu equalizera
    timeDisplay = false;         // Wyłączamy zegar
    displayActive = true;        // Wyświetlacz aktywny

    u8g2.setFont(spleen6x12PL);

    // Strona 1
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Menu Config:");
    u8g2.setCursor(0, 24);
    u8g2.print("Display auto dimmer on/off");
    drawSwitch(220, 24, displayAutoDimmerOn);
    u8g2.setCursor(0, 36);
    u8g2.print("Auto dimmer time:");
    u8g2.setCursor(225, 36);
    u8g2.print(String(displayAutoDimmerTime) + "s");
    u8g2.setCursor(0, 47);
    u8g2.print("Auto dimmer value 0-14:              14");
    u8g2.setCursor(0, 58);
    u8g2.print("Night dimmer value 0-14:              0");
    u8g2.sendBuffer();
}

void Config::drawSwitch(uint8_t x, uint8_t y, bool state) // Ikona przełacznika szeroka (x) na 21, wysoka(y) na 10
{
    u8g2.setFont(u8g2_font_spleen5x8_mf);
    y = y - 9;
    u8g2.drawRFrame(x, y, 21, 10, 1);

    if (state == 1) // Rysujemy przełacznik w pozycji ON z napisem
    {
        u8g2.drawRBox(x + 8, y, 13, 10, 3);
        u8g2.setDrawColor(0);
        u8g2.drawStr(x + 10, y + 8, "ON");
        u8g2.setDrawColor(1);
    }
    else if (state == 0) // Rysujemy w pozycji OFF
    {
        u8g2.drawRBox(x, y, 11, 10, 3);
    }
    u8g2.setFont(spleen6x12PL); // Przywracamy podstawową czcionkę
}

// Jesli dany bank istnieje juz na karcie SD to odczytujemy tylko dany Bank z karty
void Config::readSDStations()
{
    stationsCount = 0;
    Serial.println("Plik Banu isnieje na karcie SD. Czytamy TYLKO z karty");
    // mp3 = flac = aac = false;
    mp3 = flac = aac = vorbis = false;
    stationString.remove(0); // Usunięcie wszystkich znaków z obiektu stationString

    // Tworzymy nazwę pliku banku
    String fileName = String("/bank") + (bank_nr < 10 ? "0" : "") + String(bank_nr) + ".txt";

    // Sprawdzamy, czy plik istnieje
    if (!SD.exists(fileName))
    {
        Serial.println("Błąd: Plik banku nie istnieje.");
        return;
    }

    // Otwieramy plik w trybie do odczytu
    //todo dlaczego nie używamy: myFile
    File bankFile = SD.open(fileName, FILE_READ);
    if (!bankFile) // jesli brak pliku to...
    {
        Serial.println("Błąd: Nie można otworzyć pliku banku.");
        return;
    }

    // Przechodzimy do odpowiedniego wiersza pliku
    int currentLine = 0;
    String stationUrl = "";

    while (bankFile.available()) // & currentLine <= MAX_STATIONS)
    {
        // if (currentLine < MAX_STATIONS)
        //{
        String line = bankFile.readStringUntil('\n');
        currentLine++;

        // currentLine == station_nr
        stationName = line.substring(0, 42);
        int urlStart = line.indexOf("http"); // Szukamy miejsca, gdzie zaczyna się URL
        if (urlStart != -1)
        {
            stationUrl = line.substring(urlStart); // Wyciągamy URL od "http"
            stationUrl.trim();                     // Usuwamy białe znaki na początku i końcu
            ////Serial.print(" URL stacji:");
            /// Serial.println(stationUrl);
            // String station = currentLine + "   " + stationName + "  " + stationUrl;
            String station = stationName + "  " + stationUrl;
            Config::sanitizeAndSaveStation(station.c_str()); // przepisanie stacji do EEPROMu  (RAMU)
        }
        //}
    }
    Serial.print("Zamykamy plik bankFile na wartosci currentLine:");
    Serial.println(currentLine);
    bankFile.close(); // Zamykamy plik po odczycie
}

// Funkcja przetwarza i zapisuje stację do pamięci EEPROM
void Config::sanitizeAndSaveStation(const char *station)
{
    // Bufor na przetworzoną stację - o jeden znak dłuższy niż maksymalna długość linku
    char sanitizedStation[STATION_NAME_LENGTH + 1];

    // Indeks pomocniczy dla przetwarzania
    int j = 0;

    // Przeglądaj każdy znak stacji i sprawdź czy jest to drukowalny znak ASCII
    for (int i = 0; i < STATION_NAME_LENGTH && station[i] != '\0'; i++)
    {
        // Sprawdź, czy znak jest drukowalnym znakiem ASCII
        if (isprint(station[i]))
        {
            // Jeśli tak, dodaj do przetworzonej stacji
            sanitizedStation[j++] = station[i];
        }
    }

    // Dodaj znak końca ciągu do przetworzonej stacji
    sanitizedStation[j] = '\0';

    // Zapisz przetworzoną stację do pamięci EEPROM
    Config::saveStationToPSRAM(sanitizedStation);
}

// Funkcja odpowiedzialna za zapisywanie informacji o stacji do pamięci EEPROM.
void Config::saveStationToPSRAM(const char *station)
{
    // Sprawdź, czy istnieje jeszcze miejsce na kolejną stację w pamięci EEPROM.
    if (stationsCount < MAX_STATIONS)
    {
        int length = strlen(station);

        // Sprawdź, czy długość linku nie przekracza ustalonego maksimum.
        if (length <= STATION_NAME_LENGTH)
        {
            // Zapisz długość linku jako pierwszy bajt.
            psramData[stationsCount * (STATION_NAME_LENGTH + 1)] = length;
            // Zapisz link jako kolejne bajty w pamięci EEPROM.
            for (int i = 0; i < length; i++)
            {
                psramData[stationsCount * (STATION_NAME_LENGTH + 1) + 1 + i] = station[i];
            }

            // Potwierdź zapis do pamięci EEPROM.
            // EEPROM.commit();

            // Wydrukuj informację o zapisanej stacji na Serialu.
            Serial.println(String(stationsCount + 1) + "   " + String(station)); // Drukowanie na serialu od nr 1 jak w banku na serwerze

            // Zwiększ licznik zapisanych stacji.
            stationsCount++;

            u8g2.setFont(spleen6x12PL); // progress bar pobieranych stacji
            u8g2.drawStr(21, 36, "Progress:");
            u8g2.drawStr(75, 36, String(stationsCount).c_str()); // Napisz licznik pobranych stacji

            u8g2.drawRFrame(21, 42, 212, 12, 3); // Ramka paska postępu ladowania stacji stacji w>8 h>8
            displayPositionX = (stationsCount * 2) + 8;         // Dodajemy gdy stationCount=1 + 8 aby utrzymac warunek dla zaokrąglonego drawRBox - szerokość W>6 h>6 ma byc W>=2*(r+1), h >= 2*(r+1)
            u8g2.drawRBox(23, 44, displayPositionX, 8, 2);      // Pasek postepu ladowania stacji z serwera lub karty SD
            u8g2.sendBuffer();
        }
        else
        {
            // Informacja o błędzie w przypadku zbyt długiego linku do stacji.
            Serial.println("Błąd: Link do stacji jest zbyt długi");
        }
    }
    else
    {
        // Informacja o błędzie w przypadku osiągnięcia maksymalnej liczby stacji.
        Serial.println("Błąd: Osiągnięto maksymalną liczbę zapisanych stacji");
    }
}

// Funkcja do zapisywania numeru stacji i numeru banku na karcie SD
void Config::saveStationOnSD()
{
  // Sprawdź, czy plik station_nr.txt istnieje

  Serial.print("Zapisujemy bank: ");
  Serial.println(bank_nr);
  Serial.print("Zapisujemy stacje: ");
  Serial.println(station_nr);

  // Sprawdź, czy plik station_nr.txt istnieje
  if (SD.exists("/station_nr.txt"))
  {
    Serial.println("Plik station_nr.txt już istnieje.");

    // Otwórz plik do zapisu i nadpisz aktualną wartość station_nr
    myFile = SD.open("/station_nr.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(station_nr);
      myFile.close();
      Serial.println("Aktualizacja station_nr.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku station_nr.txt.");
    }
  }
  else
  {
    Serial.println("Plik station_nr.txt nie istnieje. Tworzenie...");

    // Utwórz plik i zapisz w nim aktualną wartość station_nr
    myFile = SD.open("/station_nr.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(station_nr);
      myFile.close();
      Serial.println("Utworzono i zapisano station_nr.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas tworzenia pliku station_nr.txt.");
    }
  }

  // Sprawdź, czy plik bank_nr.txt istnieje
  if (SD.exists("/bank_nr.txt"))
  {
    Serial.println("Plik bank_nr.txt już istnieje.");

    // Otwórz plik do zapisu i nadpisz aktualną wartość bank_nr
    myFile = SD.open("/bank_nr.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(bank_nr);
      myFile.close();
      Serial.println("Aktualizacja bank_nr.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku bank_nr.txt.");
    }
  }
  else
  {
    Serial.println("Plik bank_nr.txt nie istnieje. Tworzenie...");

    // Utwórz plik i zapisz w nim aktualną wartość bank_nr
    myFile = SD.open("/bank_nr.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(bank_nr);
      myFile.close();
      Serial.println("Utworzono i zapisano bank_nr.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas tworzenia pliku bank_nr.txt.");
    }
  }

  if (noSDcard == true)
  {
    Serial.println("Brak karty SD zapisujemy do EEPROM");
    EEPROM.write(0, station_nr);
    EEPROM.write(1, bank_nr);
    EEPROM.commit();
  }
}

// Funkcja do odczytu danych stacji radiowej z karty SD
void Config::readStationFromSD()
{
  // Sprawdź, czy karta SD jest dostępna
  if (!SD.begin(47))
  {
    // Serial.println("Nie można znaleźć karty SD. Ustawiam domyślne wartości: Station=1, Bank=1.");
    Serial.println("Nie można znaleźć karty SD. Ustawiam wartości z EEPROMu");
    // station_nr = 1;  // Domyślny numer stacji gdy brak karty SD
    // bank_nr = 1;     // Domyślny numer banku gdy brak karty SD
    EEPROM.get(0, station_nr);
    EEPROM.get(1, bank_nr);

    Serial.print("Odczyt EEPROM Stacja: ");
    Serial.println(station_nr);
    Serial.print("Odczyt EEPROM Bank: ");
    Serial.println(bank_nr);

    if ((station_nr > 99) || (station_nr == 0))
    {
      station_nr = 1;
    } // zabezpiecznie na wypadek błędnego odczytu EEPROMu lub wartości
    if ((bank_nr > 16) || (bank_nr == 0))
    {
      bank_nr = 1;
    }

    return;
  }

  // Sprawdź, czy plik station_nr.txt istnieje
  if (SD.exists("/station_nr.txt"))
  {
    myFile = SD.open("/station_nr.txt");
    if (myFile)
    {
      station_nr = myFile.parseInt();
      myFile.close();
      Serial.print("Wczytano station_nr z karty SD: ");
      Serial.println(station_nr);
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku station_nr.txt.");
    }
  }
  else
  {
    Serial.println("Plik station_nr.txt nie istnieje.");
    station_nr = 9; // ustawiamy stacje w przypadku braku pliku na karcie
  }

  // Sprawdź, czy plik bank_nr.txt istnieje
  if (SD.exists("/bank_nr.txt"))
  {
    myFile = SD.open("/bank_nr.txt");
    if (myFile)
    {
      bank_nr = myFile.parseInt();
      myFile.close();
      Serial.print("Wczytano bank_nr z karty SD: ");
      Serial.println(bank_nr);
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku bank_nr.txt.");
    }
  }
  else
  {
    Serial.println("Plik bank_nr.txt nie istnieje.");
    bank_nr = 1; // // ustawiamy bank w przypadku braku pliku na karcie
  }
}

// Funkcja testowa-debug, do odczytu PSRAMu, nie uzywana przez inne funkcje
void Config::readPSRAMstations()
{
  Serial.println("-------- POCZATEK LISTY STACJI ---------- ");
  for (int i = 0; i < stationsCount; i++)
  {
    // Odczyt stacji pod daną komórka pamieci PSRAM:
    char station[STATION_NAME_LENGTH + 1];                   // Tablica na nazwę stacji o maksymalnej długości zdefiniowanej przez STATION_NAME_LENGTH
    memset(station, 0, sizeof(station));                     // Wyczyszczenie tablicy zerami przed zapisaniem danych
    int length = psramData[(i) * (STATION_NAME_LENGTH + 1)]; // Odczytaj długość nazwy stacji z PSRAM dla bieżącego indeksu stacji

    for (int j = 0; j < min(length, STATION_NAME_LENGTH); j++)
    {                                                                  // Odczytaj nazwę stacji z PSRAM jako ciąg bajtów, maksymalnie do STATION_NAME_LENGTH
      station[j] = psramData[(i) * (STATION_NAME_LENGTH + 1) + 1 + j]; // Odczytaj znak po znaku nazwę stacji
    }
    String stationNameText = String(station);

    Serial.print(i + 1);
    Serial.print(" ");
    Serial.println(stationNameText);
  }

  Serial.println("-------- KONIEC LISTY STACJI ---------- ");

  String stationUrl = "";

  // Odczyt stacji pod daną komórka pamieci PSRAM:
  char station[STATION_NAME_LENGTH + 1];                                // Tablica na nazwę stacji o maksymalnej długości zdefiniowanej przez STATION_NAME_LENGTH
  memset(station, 0, sizeof(station));                                  // Wyczyszczenie tablicy zerami przed zapisaniem danych
  int length = psramData[(station_nr - 1) * (STATION_NAME_LENGTH + 1)]; // Odczytaj długość nazwy stacji z PSRAM dla bieżącego indeksu stacji

  for (int j = 0; j < min(length, STATION_NAME_LENGTH); j++)
  {                                                                               // Odczytaj nazwę stacji z PSRAM jako ciąg bajtów, maksymalnie do STATION_NAME_LENGTH
    station[j] = psramData[(station_nr - 1) * (STATION_NAME_LENGTH + 1) + 1 + j]; // Odczytaj znak po znaku nazwę stacji
  }

  // String stationNameText = String(station);

  Serial.println("-------- OBECNIE GRAMY  ---------- ");
  Serial.print(station_nr - 1);
  Serial.print(" ");
  Serial.println(String(station));
}

void Config::readEqualizerFromSD()
{
  // Sprawdź, czy karta SD jest dostępna
  if (!SD.begin(47))
  {
    Serial.println("Nie można znaleźć karty SD. Ustawiam domyślne wartości filtrow Equalziera.");
    toneHiValue = 0;  // Domyślna wartość filtra gdy brak karty SD
    toneMidValue = 0; // Domyślna wartość filtra gdy brak karty SD
    toneLowValue = 0; // Domyślna wartość filtra gdy brak karty SD
    return;
  }

  // Sprawdź, czy plik equalizer.txt istnieje
  if (SD.exists("/equalizer.txt"))
  {
    myFile = SD.open("/equalizer.txt");
    if (myFile)
    {
      toneHiValue = myFile.parseInt();
      toneMidValue = myFile.parseInt();
      toneLowValue = myFile.parseInt();
      myFile.close();

      Serial.println("Wczytano equalizer.txt z karty SD: ");

      Serial.print("Filtr High equalizera odczytany z SD: ");
      Serial.println(toneHiValue);

      Serial.print("Filtr Mid equalizera odczytany z SD: ");
      Serial.println(toneMidValue);

      Serial.print("Filtr Low equalizera odczytany z SD: ");
      Serial.println(toneLowValue);
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku equalizer.txt.");
    }
  }
  else
  {
    Serial.println("Plik equalizer.txt nie istnieje.");
    toneHiValue = 0;  // Domyślna wartość filtra gdy brak karty SD
    toneMidValue = 0; // Domyślna wartość filtra gdy brak karty SD
    toneLowValue = 0; // Domyślna wartość filtra gdy brak karty SD
  }
}

void Config::saveEqualizerOnSD()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);                 // cziocnka 14x11
  u8g2.drawStr(1, 33, "Saving equalizer settings"); // 8 znakow  x 11 szer
  u8g2.sendBuffer();

  // Sprawdź, czy plik equalizer.txt istnieje

  Serial.print("Filtr High: ");
  Serial.println(toneHiValue);

  Serial.print("Filtr Mid: ");
  Serial.println(toneMidValue);

  Serial.print("Filtr Low: ");
  Serial.println(toneLowValue);

  // Sprawdź, czy plik istnieje
  if (SD.exists("/equalizer.txt"))
  {
    Serial.println("Plik equalizer.txt już istnieje.");

    // Otwórz plik do zapisu i nadpisz aktualną wartość flitrów equalizera
    myFile = SD.open("/equalizer.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(toneHiValue);
      myFile.println(toneMidValue);
      myFile.println(toneLowValue);
      myFile.close();
      Serial.println("Aktualizacja equalizer.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku equalizer.txt.");
    }
  }
  else
  {
    Serial.println("Plik equalizer.txt nie istnieje. Tworzenie...");

    // Utwórz plik i zapisz w nim aktualną wartość filtrów equalizera
    myFile = SD.open("/equalizer.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(toneHiValue);
      myFile.println(toneMidValue);
      myFile.println(toneLowValue);
      myFile.close();
      Serial.println("Utworzono i zapisano equalizer.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas tworzenia pliku equalizer.txt.");
    }
  }
}

void Config::readVolumeFromSD()
{
  // Sprawdź, czy karta SD jest dostępna
  if (!SD.begin(47))
  {
    Serial.println("Nie można znaleźć karty SD. Ustawiam wartość Volume z EEPROMu.");
    Serial.print("Wartość Volume: ");
    EEPROM.get(2, volumeValue);
    if (volumeValue > 21)
    {
      volumeValue = 10;
    } // zabezpiczenie przed pusta komorka EEPROM o wartosci FF (255)

    volumeBufferValue = volumeValue;

    Serial.println(volumeValue);
    return;
  }
  // Sprawdź, czy plik volume.txt istnieje
  if (SD.exists("/volume.txt"))
  {
    myFile = SD.open("/volume.txt");
    if (myFile)
    {
      volumeValue = myFile.parseInt();
      myFile.close();

      Serial.println("Wczytano volume.txt z karty SD");
      Serial.print("Wartość Volume odczytany z SD: ");
      Serial.println(volumeValue);
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku volume.txt");
    }
  }
  else
  {
    Serial.println("Plik volume.txt nie istnieje.");
    Serial.print("Wartość Volume domyślna:");
    Serial.println(volumeValue);
  }
  volumeBufferValue = volumeValue;
}

void Config::saveVolumeOnSD()
{
  volumeBufferValue = volumeValue;

  // Sprawdź, czy plik volume.txt istnieje
  Serial.print("Volume: ");
  Serial.println(volumeValue);

  // Sprawdź, czy plik istnieje
  if (SD.exists("/volume.txt"))
  {
    Serial.println("Plik volume.txt już istnieje.");

    // Otwórz plik do zapisu i nadpisz aktualną wartość flitrów equalizera
    myFile = SD.open("/volume.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(volumeValue);
      myFile.close();
      Serial.println("Aktualizacja volume.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku volume.txt.");
    }
  }
  else
  {
    Serial.println("Plik volume.txt nie istnieje. Tworzenie...");

    // Utwórz plik i zapisz w nim aktualną wartość głośności
    myFile = SD.open("/volume.txt", FILE_WRITE);
    if (myFile)
    {
      myFile.println(volumeValue);
      myFile.close();
      Serial.println("Utworzono i zapisano volume.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas tworzenia pliku volume.txt.");
    }
  }
  if (noSDcard == true)
  {
    EEPROM.write(2, volumeValue);
    EEPROM.commit();
  }
}

// Funkcja do pobierania listy stacji radiowych z serwera
void Config::fetchStationsFromServer()
{
  bankChange = true;
  u8g2.setFont(spleen6x12PL);
  u8g2.clearBuffer();
  u8g2.setCursor(21, 23);
  u8g2.print("Loading BANK:" + String(bank_nr) + " stations from:");
  u8g2.sendBuffer();

  currentSelection = 0;
  firstVisibleLine = 0;
  station_nr = 1;
  previous_bank_nr = bank_nr; // jesli ładujemy stacje to ustawiamy zmienna previous_bank

  // Utwórz obiekt klienta HTTP
  HTTPClient http;

  // URL stacji dla danego banku
  String url;

  // Wybierz URL na podstawie bank_nr za pomocą switch
  switch (bank_nr)
  {
  case 1:
    url = STATIONS_URL1;
    break;
  case 2:
    url = STATIONS_URL2;
    break;
  case 3:
    url = STATIONS_URL3;
    break;
  case 4:
    url = STATIONS_URL4;
    break;
  case 5:
    url = STATIONS_URL5;
    break;
  case 6:
    url = STATIONS_URL6;
    break;
  case 7:
    url = STATIONS_URL7;
    break;
  case 8:
    url = STATIONS_URL8;
    break;
  case 9:
    url = STATIONS_URL9;
    break;
  case 10:
    url = STATIONS_URL10;
    break;
  case 11:
    url = STATIONS_URL11;
    break;
  case 12:
    url = STATIONS_URL12;
    break;
  case 13:
    url = STATIONS_URL13;
    break;
  case 14:
    url = STATIONS_URL14;
    break;
  case 15:
    url = STATIONS_URL15;
    break;
  case 16:
    url = STATIONS_URL16;
    break;
  default:
    Serial.println("Nieprawidłowy numer banku");
    return;
  }

  // Tworzenie nazwy pliku dla danego banku
  String fileName = String("/bank") + (bank_nr < 10 ? "0" : "") + String(bank_nr) + ".txt";

  // Sprawdzenie, czy plik istnieje
  if (SD.exists(fileName) && bankNetworkUpdate == false)
  {
    Serial.println("Plik banku " + fileName + " już istnieje.");
    u8g2.setFont(spleen6x12PL);
    // u8g2.drawStr(147, 23, "SD card");
    u8g2.print("SD CARD");
    u8g2.sendBuffer();
    readSDStations(); // Jesli plik istnieje to odczytujemy go tylko z karty
  }
  else
  // if (bankNetworkUpdate = true)
  {
    bankNetworkUpdate = false;
    // stworz plik na karcie tylko jesli on nie istnieje GR
    u8g2.print("GitHub");
    u8g2.sendBuffer();
    {
      // Próba utworzenia pliku, jeśli nie istnieje
      File bankFile = SD.open(fileName, FILE_WRITE);

      if (bankFile)
      {
        Serial.println("Utworzono plik banku: " + fileName);
        bankFile.close(); // Zamykanie pliku po utworzeniu
      }
      else
      {
        Serial.println("Błąd: Nie można utworzyć pliku banku: " + fileName);
        //  return;  // Przerwij dalsze działanie, jeśli nie udało się utworzyć pliku
      }
    }
    // Inicjalizuj żądanie HTTP do podanego adresu URL
    http.begin(url);

    // Wykonaj żądanie GET i zapisz kod odpowiedzi HTTP
    int httpCode = http.GET();

    // Wydrukuj dodatkowe informacje diagnostyczne
    Serial.print("Kod odpowiedzi HTTP: ");
    Serial.println(httpCode);

    // Sprawdź, czy żądanie było udane (HTTP_CODE_OK)
    if (httpCode == HTTP_CODE_OK)
    {
      // Pobierz zawartość odpowiedzi HTTP w postaci tekstu
      String payload = http.getString();
      //  Otwórz plik w trybie zapisu, aby zapisać payload
      File bankFile = SD.open(fileName, FILE_WRITE);
      if (bankFile)
      {
        bankFile.println(payload); // Zapisz dane do pliku
        bankFile.close();          // Zamknij plik po zapisaniu
        Serial.println("Dane zapisane do pliku: " + fileName);
      }
      else
      {
        Serial.println("Błąd: Nie można otworzyć pliku do zapisu: " + fileName);
      }
      // Zapisz każdą niepustą stację do pamięci EEPROM z indeksem
      int startIndex = 0;
      int endIndex;
      stationsCount = 0;
      // Przeszukuj otrzymaną zawartość w poszukiwaniu nowych linii
      while ((endIndex = payload.indexOf('\n', startIndex)) != -1 && stationsCount < MAX_STATIONS)
      {
        // Wyodrębnij pojedynczą stację z otrzymanego tekstu
        String station = payload.substring(startIndex, endIndex);

        // Sprawdź, czy stacja nie jest pusta, a następnie przetwórz i zapisz
        if (!station.isEmpty())
        {
          // Zapisz stację do pliku na karcie SD
          sanitizeAndSaveStation(station.c_str());
        }
        // Przesuń indeks początkowy do kolejnej linii
        startIndex = endIndex + 1;
      }
    }
    else
    {
      // W przypadku nieudanego żądania wydrukuj informację o błędzie z kodem HTTP
      Serial.printf("Błąd podczas pobierania stacji. Kod HTTP: %d\n", httpCode);
    }
    // Zakończ połączenie HTTP
    http.end();
  }
  bankChange = false;
}
