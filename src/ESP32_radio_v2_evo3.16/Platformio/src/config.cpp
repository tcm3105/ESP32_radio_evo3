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
