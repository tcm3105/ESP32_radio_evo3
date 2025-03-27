// ###############################################################################################
// ESP32 Radio Evo3 - Interent Radio Player
// Support OLED SSD1322 dipslay and PCB5102A DAC
// ###############################################################################################
// Robgold 2025
// Source -> https://github.com/dzikakuna/ESP32_radio_evo3/tree/main/src/ESP32_radio_v2_evo3.16
// Based on project https://github.com/sarunia/ESP32_radio_player_v2
// TCM3105 2025 - PlatformIO code refactoring
// ###############################################################################################

#include "Arduino.h"     // Standardowy nagłówek Arduino, który dostarcza podstawowe funkcje i definicje
#include "Audio.h"       // Biblioteka do obsługi funkcji związanych z dźwiękiem i audio
#include "SPI.h"         // Biblioteka do obsługi komunikacji SPI
#include "SD.h"          // Biblioteka do obsługi kart SD
#include "FS.h"          // Biblioteka do obsługi systemu plików
#include <U8g2lib.h>     // Biblioteka do obsługi wyświetlaczy
#include <ezButton.h>    // Biblioteka do obsługi enkodera z przyciskiem
#include <HTTPClient.h>  // Biblioteka do wykonywania żądań HTTP, umożliwia komunikację z serwerami przez protokół HTTP
#include <Ticker.h>      // Mechanizm tickera do odświeżania timera 1s, pomocny do cyklicznych akcji w pętli głównej
#include <WiFiManager.h> // Biblioteka do zarządzania konfiguracją sieci WiFi, opis jak ustawić połączenie WiFi przy pierwszym uruchomieniu jest opisany tu: https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
// #include <ArduinoJson.h> // Biblioteka do parsowania i tworzenia danych w formacie JSON, użyteczna do pracy z API
#include <Time.h> // Biblioteka do obsługi funkcji związanych z czasem, np. odczytu daty i godziny
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include "main.h"

// Only for i2c Keyboard
// #include "Wire.h"
// #include "I2CKeyPad8x8.h"

// deklaracja wersji oprogramowania i nazwy hosta widocznego w routerach
#define softwareRev "v3.16"    // Wersja oprogramowania radia
#define hostname "ESP32-Radio" // Definicja nazwy hosta widoczna na zewnątrz

File myFile; // Uchwyt pliku

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R2, /* cs=*/CS_OLED, /* dc=*/DC_OLED, /* reset=*/RESET_OLED); // Hardware SPI 3.12inch OLED
// U8G2_SSD1363_256X128_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/CS_OLED, /* dc=*/DC_OLED, /* reset=*/RESET_OLED);  // Hardware SPI 3.12inch OLED
// U8G2_SH1122_256X64_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ CS_OLED, /* dc=*/ DC_OLED, /* reset=*/ RESET_OLED);		// Hardware SPI  2.08inch OLED

// Przypisujemy port serwera www
// WiFiServer server(80);

AsyncWebServer server(80);

// Inicjalizacja WiFiManagera
WiFiManager wifiManager;

// Konfiguracja nowego SPI z wybranymi pinami dla czytnika kart SD
SPIClass customSPI = SPIClass(HSPI); // Używamy HSPI, ale z własnymi pinami

ezButton button1(SW_PIN1); // Utworzenie obiektu przycisku z enkodera 1 ezButton, podłączonego do pinu 4
ezButton button2(SW_PIN2); // Utworzenie obiektu przycisku z enkodera 1 ezButton, podłączonego do pinu 1
Audio audio;               // Obiekt do obsługi funkcji związanych z dźwiękiem i audio
AudioBuffer audioBuffer;
Ticker timer1; // Timer do updateTimer co 1s
Ticker timer2; // Timer do getWeatherData co 60s
// Ticker timer3;           // Timer do przełączania wyświetlania danych pogodoych w ostatniej linii co 10s
WiFiClient client; // Obiekt do obsługi połączenia WiFi dla klienta HTTP

String processor(const String &var)
{
  // Serial.println(var);
  if (var == "SLIDERVALUE")
  {
    return String(volumeValue);
  }
  if (var == "STATIONNAMEVALUE")
  {
    return String(stationName.substring(0, stationNameLenghtCut));
  }
  if (var == "BANKVALUE")
  {
    return String(bank_nr);
  }
  if (var == "STATIONNUMBER")
  {
    return String(station_nr);
  }
  return String();
}

// Funkcja odwracania bitów MSL-LSB <-> LSB-MSB
uint32_t reverse_bits(uint32_t inval, int bits)
{
  if (bits > 0)
  {
    bits--;
    return reverse_bits(inval >> 1, bits) | ((inval & 1) << bits);
  }
  return 0;
}

// Funkcja sprawdza, czy plik jest plikiem audio na podstawie jego rozszerzenia
bool isAudioFile(const char *filename)
{
  // Dodaj więcej rozszerzeń plików audio, jeśli to konieczne
  //  return (strstr(filename, ".mp3") || strstr(filename, ".MP3") || strstr(filename, ".wav") || strstr(filename, ".WAV") || strstr(filename, ".flac") || strstr(filename, ".FLAC"));
  //}

  // Znajdź ostatni wystąpienie kropki w nazwie pliku
  const char *ext = strrchr(filename, '.');

  // Jeśli nie znaleziono kropki lub nie ma rozszerzenia, zwróć false
  if (!ext)
  {
    return false;
  }

  // Sprawdź rozszerzenie, ignorując wielkość liter
  return (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".flac") == 0);
}

// Funkcja odpowiedzialna za zapisywanie informacji o stacji do pamięci EEPROM.
void saveStationToPSRAM(const char *station)
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
      x = (stationsCount * 2) + 8;         // Dodajemy gdy stationCount=1 + 8 aby utrzymac warunek dla zaokrąglonego drawRBox - szerokość W>6 h>6 ma byc W>=2*(r+1), h >= 2*(r+1)
      u8g2.drawRBox(23, 44, x, 8, 2);      // Pasek postepu ladowania stacji z serwera lub karty SD
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

// Funkcja przetwarza i zapisuje stację do pamięci EEPROM
void sanitizeAndSaveStation(const char *station)
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
  saveStationToPSRAM(sanitizedStation);
}

// Jesli dany bank istnieje juz na karcie SD to odczytujemy tylko dany Bank z karty
void readSDStations()
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
      sanitizeAndSaveStation(station.c_str()); // przepisanie stacji do EEPROMu  (RAMU)
    }
    //}
  }
  Serial.print("Zamykamy plik bankFile na wartosci currentLine:");
  Serial.println(currentLine);
  bankFile.close(); // Zamykamy plik po odczycie
}

// Funkcja do pobierania listy stacji radiowych z serwera
void fetchStationsFromServer()
{
  bankChange = true;
  u8g2.setFont(spleen6x12PL);
  u8g2.clearBuffer();
  // u8g2.drawStr(21, 10, "Bank:");
  // u8g2.drawStr(51, 10, String(bank_nr).c_str());
  // u8g2.drawStr(21, 23, "Loading station from:");
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
    // u8g2.drawStr(205, 23, "GitHub server");
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
      // Serial.println("Stacje pobrane z serwera:");
      // Serial.println(payload);  // Wyświetlenie pobranych danych (payload)
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

// Obsługa wyświetlacza dla odtwarzanego pliku z karty SD
void displayPlayer()
{
  if (id3tag == true)
  {
    timeDisplay = true;
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_spleen6x12_mr);
    u8g2.setCursor(0, 10);
    u8g2.print("PLAYING:");

    Serial.print("DEBUG--PlayedFolderName:");
    Serial.println(PlayedFolderName);

    if (PlayedFolderName.length() > 24)
    {
      u8g2.print(PlayedFolderName.substring(0, 23)); // Jesli folder muzyk > 16 znakow to wyswietlamy pierwszy 16 i trzy kropki
      u8g2.print("...");
    }
    else
    {
      u8g2.print(PlayedFolderName); // Jesli nazwa folderu miesci sie w 16 znakach wysweitlamy całosc
    }

    u8g2.setCursor(202, 10);
    u8g2.print(" Tr:");
    u8g2.print(fileFromBuffer);
    u8g2.print("/");
    u8g2.print(totalFilesInFolder);
    // u8g2.print(" FOLDER ");
    // u8g2.print(folderFromBuffer);
    // u8g2.print("/");
    // u8g2.print(directoryCount);

    if (artistString.length() > 21)
    {
      artistString = artistString.substring(0, 21); // Ogranicz długość tekstu do 33 znaków
    }
    u8g2.setCursor(0, 28);
    u8g2.setFont(u8g2_font_fub14_tf);
    // u8g2.print("Artysta: ");
    u8g2.print(artistString);

    if (titleString.length() > 35)
    {
      titleString = titleString.substring(0, 35); // Ogranicz długość tekstu do 35 znaków
    }
    u8g2.setFont(spleen6x12PL);
    u8g2.setCursor(0, 42);
    // u8g2.print("Tytul:");
    u8g2.print(titleString);

    /*if (folderNameString.startsWith("/"))
    {
      folderNameString = folderNameString.substring(1); // Usuń pierwszy ukośnik
    }

    if (folderNameString.length() > 34)
    {
      folderNameString = folderNameString.substring(0, 34); // Ogranicz długość tekstu do 34 znaków
    }
    u8g2.setCursor(0, 41);
    u8g2.print("Folder: ");
    u8g2.print(folderNameString);
    */
    u8g2.drawStr(0, 63, "                                           ");
    u8g2.drawLine(0, 51, 255, 51);
    String displayString = sampleRateString.substring(1) + "Hz " + bitsPerSampleString + "bit " + bitrateString + "kbps";
    u8g2.drawStr(0, 63, displayString.c_str());
    u8g2.sendBuffer();
    Serial.println("Tagi ID3 artysty, tytułu i folderu gotowe do wyświetlenia");
  }
  else
  {
    // Maksymalna długość wiersza (42 znaki)
    int maxLineLength = 42;
    int maxFirstLineLength = 26;
    int maxFirstLineLengthLongName = 42;
    timeDisplay = true;
    u8g2.clearBuffer();
    u8g2.setFont(spleen6x12PL);
    u8g2.setCursor(0, 10);
    u8g2.print("PLAYING:                    ");
    u8g2.print(fileFromBuffer);
    u8g2.print(" of ");
    u8g2.print(totalFilesInFolder);
    // u8g2.print(" FOLDER ");
    // u8g2.print(folderFromBuffer);
    // u8g2.print("/");
    // u8g2.print(directoryCount);
    // u8g2.drawStr(0, 21, "Brak danych ID3 utworu, nazwa pliku:");

    // Jeśli długość nazwy pliku przekracza 42 znaki na wiersz
    // if (fileNameString.length() > maxLineLength)
    if (fileNameString.length() > maxFirstLineLength)
    {

      int FileNameStringIndex = String(fileNameString).indexOf("-"); // Znajdujemy index ile znaków mamy w nazwie pliku do "-"
      // Jeśli nazwa pliku NIE mieści się w jednym wierszu

      // Prcyinamy nazwe artysty aby miesciła sie w pierwszej lini jest jest za długa
      String firstLine = String(fileNameString).substring(0, FileNameStringIndex);
      String secondLine = String(fileNameString).substring(FileNameStringIndex + 2, String(fileNameString).indexOf('.', FileNameStringIndex));

      if (firstLine.length() < 26)
      {
        firstLine = String(firstLine.substring(0, maxFirstLineLength)); // Nazwe Artysty przycinamy do wartosci FirstLineLenght dla dużej czcionka (długosc do 26 znakow)
        u8g2.setCursor(0, 28);
        u8g2.setFont(u8g2_font_fub14_tf); // W pierwszej lini jest nazwa Artysty - piszemy duza czcionką
        u8g2.print(firstLine);
      }
      else
      {
        firstLine = String(firstLine.substring(0, maxFirstLineLengthLongName)); // Nazwe Artysty przycinamy do wartosci maxFirstLineLengthLongName dla długosci > 26 znakow
        u8g2.setCursor(0, 28);
        u8g2.setFont(spleen6x12PL); // przy BARDZO długich nazwach (powyzej 26 znakow) pierwsza linia budowana jest mała cziocnka - rozwiazanie tymczasowe
        u8g2.print(firstLine);
      }

      // Drugi wiersz - pozostałe znaki nazwa utworu
      // Wyswietlamy

      u8g2.setFont(spleen6x12PL);
      u8g2.setCursor(0, 42);
      u8g2.print(secondLine);
    }
    else
    {
      int FileNameStringIndex = String(fileNameString).indexOf("-"); // Znajdujemy index ile znaków mamy w nazwie pliku do "-"
      // Jeśli nazwa pliku mieści się w jednym wierszu

      u8g2.setCursor(0, 28);
      u8g2.setFont(u8g2_font_fub14_tf);
      u8g2.print(String(fileNameString).substring(0, FileNameStringIndex)); // W pierwszej lini jest nazwa Artysty - piszemy duza czcionką

      // druga linia to nazwa utworu, zmieniamy cziocnke na małą
      u8g2.setFont(spleen6x12PL);
      u8g2.setCursor(0, 42);

      // Składamy nazwe utworu w przypadku braku id3tag.
      // Pierwsza linia wycina z nazwy pliku do znacznika "-" druga zawiera to co jest po znaczniku "-" do krpoki rozszerzenia "."

      u8g2.print(String(fileNameString).substring(FileNameStringIndex + 2, String(fileNameString).indexOf('.', FileNameStringIndex)));
    }
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(0, 63, "                                           ");
    u8g2.drawLine(0, 51, 255, 51);
    String displayString = sampleRateString.substring(1) + "Hz " + bitsPerSampleString + "bit " + bitrateString + "kbps" + " noID3";
    u8g2.drawStr(0, 63, displayString.c_str());
    u8g2.sendBuffer();
    Serial.println("Brak prawidłowych tagów ID3 do wyświetlenia");
  }
}

// Funkcja przetwarza tekst, zamieniając polskie znaki diakrytyczne
void processText(String &text)
{
  for (int i = 0; i < text.length(); i++)
  {
    switch (text[i])
    {
    case (char)0xC2:
      switch (text[i + 1])
      {
      case (char)0xB3:
        text.setCharAt(i, 0xB3);
        break; // Zamiana na "ł"
      case (char)0x9C:
        text.setCharAt(i, 0x9C);
        break; // Zamiana na "ś"
      case (char)0x8C:
        text.setCharAt(i, 0x8C);
        break; // Zamiana na "Ś"
      case (char)0xB9:
        text.setCharAt(i, 0xB9);
        break; // Zamiana na "ą"
      case (char)0x9B:
        text.setCharAt(i, 0xEA);
        break; // Zamiana na "ę"
      case (char)0xBF:
        text.setCharAt(i, 0xBF);
        break; // Zamiana na "ż"
      case (char)0x9F:
        text.setCharAt(i, 0x9F);
        break; // Zamiana na "ź"
      }
      text.remove(i + 1, 1);
      break;
    case (char)0xC3:
      switch (text[i + 1])
      {
      case (char)0xB1:
        text.setCharAt(i, 0xF1);
        break; // Zamiana na "ń"
      case (char)0xB3:
        text.setCharAt(i, 0xF3);
        break; // Zamiana na "ó" Unicode UTF-8
      case (char)0xBA:
        text.setCharAt(i, 0x9F);
        break; // Zamiana na "ź"
      case (char)0xBB:
        text.setCharAt(i, 0xAF);
        break; // Zamiana na "Ż"
      case (char)0x93:
        text.setCharAt(i, 0xD3);
        break; // Zamiana na "Ó" Unicode UTF-8
      }
      text.remove(i + 1, 1);
      break;
    case (char)0xC4:
      switch (text[i + 1])
      {
      case (char)0x85:
        text.setCharAt(i, 0xB9);
        break; // Zamiana na "ą" Unicode UTF-8
      case (char)0x99:
        text.setCharAt(i, 0xEA);
        break; // Zamiana na "ę" Unicode UTF-8
      case (char)0x87:
        text.setCharAt(i, 0xE6);
        break; // Zamiana na "ć" Unicode UTF-8
      case (char)0x84:
        text.setCharAt(i, 0xA5);
        break; // Zamiana na "Ą" Unicode UTF-8
      case (char)0x98:
        text.setCharAt(i, 0xCA);
        break; // Zamiana na "Ę" Unicode UTF-8
      case (char)0x86:
        text.setCharAt(i, 0xC6);
        break; // Zamiana na "Ć" Unicode UTF-8
      }
      text.remove(i + 1, 1);
      break;
    case (char)0xC5:
      switch (text[i + 1])
      {
      case (char)0x82:
        text.setCharAt(i, 0xB3);
        break; // Zamiana na "ł" Unicode UTF-8
      case (char)0x84:
        text.setCharAt(i, 0xF1);
        break; // Zamiana na "ń" Unicode UTF-8
      case (char)0x9B:
        text.setCharAt(i, 0x9C);
        break; // Zamiana na "ź" Unicode UTF-8
      case (char)0xBB:
        text.setCharAt(i, 0xAF);
        break; // Zamiana na "Ż" Unicode UTF-8
      case (char)0xBC:
        text.setCharAt(i, 0xBF);
        break; // Zamiana na "ż" Unicode UTF-8
      case (char)0x83:
        text.setCharAt(i, 0xD1);
        break; // Zamiana na "Ń" Unicode UTF-8
      case (char)0x9A:
        text.setCharAt(i, 0x97);
        break; // Zamiana na "Ś" Unicode UTF-8
      case (char)0x81:
        text.setCharAt(i, 0xA3);
        break; // Zamiana na "Ł" Unicode UTF-8
      case (char)0xB9:
        text.setCharAt(i, 0xAC);
        break; // Zamiana na "Ź" Unicode UTF-8
      case (char)0xBA:
        text.setCharAt(i, 0x9F);
        break; // Zamiana na "ź" Unicode UTF-8
      }
      text.remove(i + 1, 1);
      break;
    }
  }
}

// Obsługa wyświetlacza dla odtwarzanego strumienia radia internetowego
void displayRadio()
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
      processText(stationString);                     // przetwarzamy polsie znaki
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
      processText(stationString); // przetwarzamy polsie znaki
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
      processText(stationString); // przetwarzamy polsie znaki
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

void audio_info(const char *info)
{
  // Wyświetl informacje w konsoli szeregowej
  Serial.print("info        ");
  Serial.println(info);
  // Znajdź pozycję "BitRate:" w tekście
  int bitrateIndex = String(info).indexOf("BitRate:");
  bitratePresent = false;
  if (bitrateIndex != -1)
  {
    // Przytnij tekst od pozycji "BitRate:" do końca linii
    bitrateString = String(info).substring(bitrateIndex + 8, String(info).indexOf('\n', bitrateIndex));
    bitrateStringInt = bitrateString.toInt(); // przliczenie bps na Kbps
    bitrateStringInt = bitrateStringInt / 1000;
    bitrateString = String(bitrateStringInt);
    bitratePresent = true;

    if (currentOption == PLAY_FILES)
    {
      displayPlayer();
    }
    if (currentOption == INTERNET_RADIO)
    {
      // displayRadio();
      audioInfoRefresh = true;
    }
  }

  // Znajdź pozycję "SampleRate:" w tekście
  int sampleRateIndex = String(info).indexOf("SampleRate:");
  if (sampleRateIndex != -1)
  {
    // Przytnij tekst od pozycji "SampleRate:" do końca linii
    sampleRateString = String(info).substring(sampleRateIndex + 11, String(info).indexOf('\n', sampleRateIndex));
  }

  // Znajdź pozycję "BitsPerSample:" w tekście
  int bitsPerSampleIndex = String(info).indexOf("BitsPerSample:");
  if (bitsPerSampleIndex != -1)
  {
    // Przytnij tekst od pozycji "BitsPerSample:" do końca linii
    bitsPerSampleString = String(info).substring(bitsPerSampleIndex + 15, String(info).indexOf('\n', bitsPerSampleIndex));
  }

  // Znajdź pozycję "skip metadata" w tekście
  int metadata = String(info).indexOf("skip metadata");
  if (metadata != -1)
  {
    Serial.println("Brak ID3 - nazwa pliku: " + fileNameString);
    if (fileNameString.length() > 84)
    {
      fileNameString = String(fileNameString).substring(0, 84); // Przytnij string do 84 znaków, aby zmieścić w 2 liniach z dalszym podziałem na pełne wyrazy
    }
  }

  if (String(info).indexOf("MP3Decoder") != -1)
  {
    mp3 = true;
    flac = false;
    aac = false;
    vorbis = false;
  }

  if (String(info).indexOf("FLACDecoder") != -1)
  {
    flac = true;
    mp3 = false;
    aac = false;
    vorbis = false;
  }

  if (String(info).indexOf("AACDecoder") != -1)
  {
    aac = true;
    flac = false;
    mp3 = false;
    vorbis = false;
  }
  if (String(info).indexOf("VORBISDecoder") != -1)
  {
    vorbis = true;
    aac = false;
    flac = false;
    mp3 = false;
  }
}

void audio_id3data(const char *info)
{
  Serial.print("id3data     ");
  Serial.println(info);

  // Znajdź pozycję w tekście
  int artistIndex1 = String(info).indexOf("Artist: ");
  int artistIndex2 = String(info).indexOf("ARTIST=");

  if (artistIndex1 != -1)
  {
    // Przytnij tekst od pozycji "Artist:" do końca linii
    artistString = String(info).substring(artistIndex1 + 8, String(info).indexOf('\n', artistIndex1));
    Serial.println("Znalazłem artystę: " + artistString);
    id3tag = true;
  }
  if (artistIndex2 != -1)
  {
    // Przytnij tekst od pozycji "ARTIST=" do końca linii
    artistString = String(info).substring(artistIndex2 + 7, String(info).indexOf('\n', artistIndex2));
    Serial.println("Znalazłem artystę: " + artistString);
    id3tag = true;
  }

  // Znajdź pozycję w tekście
  int titleIndex1 = String(info).indexOf("Title: ");
  int titleIndex2 = String(info).indexOf("TITLE=");

  if (titleIndex1 != -1)
  {
    // Przytnij tekst od pozycji "Title: " do końca linii
    titleString = String(info).substring(titleIndex1 + 7, String(info).indexOf('\n', titleIndex1));
    Serial.println("Znalazłem tytuł: " + titleString);
    id3tag = true;
  }
  if (titleIndex2 != -1)
  {
    // Przytnij tekst od pozycji "TITLE=" do końca linii
    titleString = String(info).substring(titleIndex2 + 6, String(info).indexOf('\n', titleIndex2));
    Serial.println("Znalazłem tytuł: " + titleString);
    id3tag = true;
  }
}

void audio_bitrate(const char *info)
{
  Serial.print("bitrate     ");
  Serial.println(info);
}

void audio_eof_mp3(const char *info)
{
  fileEnd = true;
  Serial.print("eof_mp3     ");
  Serial.println(info);
}

void audio_showstation(const char *info)
{
  Serial.print("station     ");
  Serial.println(info);
  stationNameStream = info;
  audioInfoRefresh = true;
}

void audio_showstreamtitle(const char *info)
{
  // u8g2.setFont(spleen6x12PL);
  // u8g2.setFont(u8g2_font_6x12_mf);
  // u8g2.drawStr(0, 27, "                                           ");
  // u8g2.drawStr(0, 39, "                                           ");
  // u8g2.drawStr(0, 51, "                                           ");

  Serial.print("streamtitle ");
  Serial.println(info);
  stationString = String(info);
  if (currentOption == INTERNET_RADIO)
  {

    ActionNeedUpdateTime = true;
    if ((volumeSet == false) && (bankMenuEnable == false) && (listedStations == false) && (rcInputDigitsMenuEnable == false) && (equalizerMenuEnable == false))
    {
      // screenRefresh = true;
      audioShowStreamtitleRefresh = true;
      // displayRadio();
    }
  }
}

void audio_commercial(const char *info)
{
  Serial.print("commercial  ");
  Serial.println(info);
}

void audio_icyurl(const char *info)
{
  Serial.print("icyurl      ");
  Serial.println(info);
}

void audio_lasthost(const char *info)
{
  Serial.print("lasthost    ");
  Serial.println(info);
}

void audio_eof_speech(const char *info)
{
  Serial.print("eof_speech  ");
  Serial.println(info);
}

void displayMenu()
{
  timeDisplay = false;
  menuEnable = true;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_spleen8x16_mr);
  u8g2.drawStr(65, 20, "MENU");

  switch (currentOption)
  {
  case PLAY_FILES:
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_spleen8x16_mr);
    u8g2.drawStr(65, 20, "MENU");

    u8g2.setDrawColor(1);         // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji
    u8g2.drawBox(0, 27, 112, 15); // Narysuj prostokąt jako tło dla zaznaczonej stacji (x=0, szerokość 256, wysokość 10)
    u8g2.setDrawColor(0);         // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji
    u8g2.drawStr(0, 40, " MUSIC PLAYER ");
    u8g2.setDrawColor(1);
    u8g2.drawStr(0, 60, " Net radio    ");
    break;
  case INTERNET_RADIO:
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_spleen8x16_mr);
    u8g2.drawStr(65, 20, "MENU");

    u8g2.setDrawColor(1); // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji
    u8g2.drawStr(0, 40, " Music player ");
    // u8g2.setDrawColor(1);
    u8g2.drawBox(0, 47, 112, 15); // Narysuj prostokąt jako tło dla zaznaczonej stacji (x=0, szerokość 256, wysokość 10)
    u8g2.setDrawColor(0);         // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji
    u8g2.drawStr(0, 60, " NET RADIO    ");
    u8g2.setDrawColor(1);
    break;
  }
  u8g2.sendBuffer();
}

void printDirectoriesAndSavePaths(File dir, int numTabs, String currentPath)
{
  directoryCount = 0;
  // Przejrzyj wszystkie pliki w katalogu
  while (true)
  {
    File entry = dir.openNextFile();

    if (!entry) // Jeżeli nie ma więcej plików, przerwij pętlę
    {
      break; // Koniec plików
    }

    // Sprawdź, czy to katalog
    if (entry.isDirectory())
    {
      // Utwórz pełną ścieżkę do bieżącego katalogu
      String path = currentPath + "/" + entry.name();
      Serial.print("String path:");
      Serial.println(path);
      // Zapisz pełną ścieżkę do tablicy
      directories[directoryCount] = path;

      // Wydrukuj numer indeksu i pełną ścieżkę
      Serial.print(directoryCount);
      Serial.print(": ");
      Serial.println(path.substring(1));

      // Zwiększ licznik katalogów
      directoryCount++;

      // Jeżeli to nie katalog System Volume Information, wydrukuj na ekranie OLED
      if (path != "/System Volume Information")
      {
        for (int i = 1; i < 7; i++)
        {
          // Przygotuj pełną ścieżkę dla wyświetlenia
          String fullPath = directories[i];

          // Ogranicz długość do 21 znaków
          fullPath = fullPath.substring(1, 42);
        }
      }
    }
    // Zamknij plik
    entry.close();
  }
}

void encoderFunctionOrderChange()
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

void bankMenuDisplay()
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
  // else
  //{
  //   u8g2.setFont(u8g2_font_fub14_tf);
  //   u8g2.drawStr(170, 33, "      ");
  // }
  u8g2.drawRFrame(21, 42, 214, 14, 3);               // Ramka do slidera bankow
  u8g2.drawRBox((bank_nr * 13) + 10, 44, 15, 10, 2); // wypełnienie slidera
  u8g2.sendBuffer();
}

// =========== Funkcja do obsługi przycisków enkoderów, debouncing i długiego naciśnięcia ==============//
void handleButtons()
{
  static unsigned long buttonPressTime1 = 0; // Zmienna do przechowywania czasu naciśnięcia przycisku
  static bool isButton1Pressed = false;      // Flaga do śledzenia, czy przycisk jest wciśnięty
  static bool action1Taken = false;          // Flaga do śledzenia, czy akcja została wykonana
                                             // static unsigned long lastPressTime = 0;    // Zmienna do kontrolowania debouncingu (ostatni czas naciśnięcia)

  static unsigned long buttonPressTime2 = 0; // Zmienna do przechowywania czasu naciśnięcia przycisku enkodera 2
  static bool isButton2Pressed = false;      // Flaga do śledzenia, czy przycisk enkodera 2 jest wciśnięty
  static bool action2Taken = false;          // Flaga do śledzenia, czy akcja dla enkodera 2 została wykonana

  static unsigned long lastPressTime = 0; // Zmienna do kontrolowania debouncingu (ostatni czas naciśnięcia)
  const unsigned long debounceDelay = 50; // Opóźnienie debouncingu

  // ===== Obsługa przycisku enkodera 1 =====
  int reading1 = digitalRead(SW_PIN1);

  // Debouncing dla przycisku enkodera 1
  if (reading1 == LOW) // Przycisk jest wciśnięty (stan niski)
  {
    if (millis() - lastPressTime > debounceDelay)
    {
      lastPressTime = millis(); // Aktualizujemy czas ostatniego naciśnięcia

      // Sprawdzamy, czy przycisk był wciśnięty przez 3 sekundy
      if (!isButton1Pressed)
      {
        buttonPressTime1 = millis(); // Ustawiamy czas naciśnięcia
        isButton1Pressed = true;     // Ustawiamy flagę, że przycisk jest wciśnięty
        action1Taken = false;        // Resetujemy flagę akcji dla enkodera 1
      }

      // Jeśli przycisk jest wciśnięty przez co najmniej 3 sekundy i akcja jeszcze nie była wykonana
      if (millis() - buttonPressTime1 >= buttonLongPressTime1 && !action1Taken)
      {
        timeDisplay = false;
        displayMenu();
        menuEnable = true;
        displayActive = true;
        displayStartTime = millis();

        Serial.println("Wyświetlenie menu po przytrzymaniu przycisku enkodera 1");

        // Ustawiamy flagę, że akcja została wykonana
        action1Taken = true;
      }
    }
  }
  else
  {
    isButton1Pressed = false; // Resetujemy flagę naciśnięcia przycisku enkodera 1
    action1Taken = false;     // Resetujemy flagę akcji dla enkodera 1
  }

  // ===== Obsługa przycisku enkodera 2 =====
  int reading2 = digitalRead(SW_PIN2);

  // Debouncing dla przycisku enkodera 2

  if (reading2 == LOW) // Przycisk jest wciśnięty (stan niski)
  {
    if (millis() - lastPressTime > debounceDelay)
    {
      // encoderButton2 = true;  // Ustawiamy flagę, że przycisk został wciśnięty
      lastPressTime = millis(); // Aktualizujemy czas ostatniego naciśnięcia

      // Sprawdzamy, czy przycisk był wciśnięty przez 3 sekundy
      if (!isButton2Pressed)
      {
        buttonPressTime2 = millis(); // Ustawiamy czas naciśnięcia
        isButton2Pressed = true;     // Ustawiamy flagę, że przycisk jest wciśnięty
        action2Taken = false;        // Resetujemy flagę akcji dla enkodera 2
        action3Taken = false;        // Resetujmy flage akcji Super długiego wcisniecia enkodera 2
        volumeSet = false;
      }

      /*if ((millis() - buttonPressTime2 >= buttonShortPressTime2) && (millis() - buttonPressTime2 < buttonSuperLongPressTime2) &&(millis() - buttonPressTime2 < buttonLongPressTime2))
      {
        volumeSet = true;
        timeDisplay = false;
        displayStartTime = millis();
        Serial.println("debug--krotkie nacisniecie enkodera 2");
      }
      */

      if (millis() - buttonPressTime2 >= buttonLongPressTime2 && millis() - buttonPressTime2 >= buttonSuperLongPressTime2 && action3Taken == false)
      {
        encoderFunctionOrderChange();

        /*
        displayActive = true;
        displayStartTime = millis();

        debugKeyboard = !debugKeyboard;
        Serial.print("Pomiar wartości ADC ON/OFF:");
        Serial.println(debugKeyboard);
        */

        action3Taken = true;
      }

      // Jeśli przycisk jest wciśnięty przez co najmniej 3 sekundy i akcja jeszcze nie była wykonana
      if (millis() - buttonPressTime2 >= buttonLongPressTime2 && !action2Taken && millis() - buttonPressTime2 < buttonSuperLongPressTime2)
      {
        Serial.println("debug--Bank Menu");
        bankMenuDisplay();

        // Ustawiamy flagę akcji, aby wykonała się tylko raz
        action2Taken = true;
      }
    }
  }
  else
  {
    isButton2Pressed = false; // Resetujemy flagę naciśnięcia przycisku enkodera 2
    action2Taken = false;     // Resetujemy flagę akcji dla enkodera 2
    action3Taken = false;
  }
}

int maxSelection()
{
  if (currentOption == INTERNET_RADIO)
  {
    return stationsCount - 1;
  }
  else if (currentOption == PLAY_FILES)
  {
    return directoryCount - 1;
  }
  return 0; // Zwraca 0, jeśli żaden warunek nie jest spełniony
}

// Funkcja do przewijania w dół
void scrollDown()
{
  if (currentSelection < maxSelection())
  {
    currentSelection++;
    if (currentSelection >= firstVisibleLine + maxVisibleLines)
    {
      firstVisibleLine++;
    }
  }
  else
  {
    // Jeśli osiągnięto maksymalną wartość, przejdź do najmniejszej (0)
    currentSelection = 0;
    firstVisibleLine = 0; // Przywróć do pierwszej widocznej linii
  }

  Serial.print("Scroll Down: CurrentSelection = ");
  Serial.println(currentSelection);
}

// Funkcja do wyświetlania folderów na ekranie OLED z uwzględnieniem zaznaczenia
void displayFolders()
{
  u8g2.clearBuffer();
  u8g2.setFont(spleen6x12PL);
  u8g2.setCursor(0, 10);
  u8g2.print("   ODTWARZACZ PLIKOW - LISTA KATALOGOW    ");
  // u8g2.setCursor(0, 21);
  // u8g2.print(currentDirectory);  // Wyświetl bieżący katalog

  int displayRow = 1; // Zmienna dla numeru wiersza, zaczynając od drugiego (pierwszy to nagłówek)

  // Wyświetlanie katalogów zaczynając od pierwszej widocznej linii
  for (int i = firstVisibleLine; i < min(firstVisibleLine + 4, directoryCount); i++)
  {
    String fullPath = currentDirectory + directories[i];

    // Pomijaj "System Volume Information"
    if (fullPath != "/System Volume Information")
    {
      Serial.print("----------------------------------");
      Serial.print("debug--Full path CurretnDirectory:");
      Serial.println(currentDirectory);
      // Sprawdź, czy ścieżka zaczyna się od aktualnego katalogu
      if (fullPath.startsWith(currentDirectory))
      // if (fullPath.startsWith(folderNameString))

      {
        // Ogranicz długość do 42 znaków
        String displayedPath = fullPath.substring(currentDirectory.length() + 1, currentDirectory.length() + 42);
        Serial.print("debug--Displayedpath:");
        Serial.println(displayedPath);
        // Podświetlenie zaznaczonego katalogu
        // if (i == x) {x= i+1 }

        if (i == currentSelection)
        {
          Serial.print("debug--Full path:");
          Serial.println(fullPath);
          Serial.print("debug--Indeks i:");
          Serial.println(i);
          Serial.print("debug--CurrentDirectory: ");
          Serial.println(currentDirectory);

          u8g2.setFont(spleen6x12PL);
          u8g2.setDrawColor(1);                          // Biały kolor tła
          u8g2.drawBox(0, displayRow * 13 - 2, 256, 13); // Narysuj prostokąt jako tło dla zaznaczonego folderu
          u8g2.setDrawColor(0);                          // Czarny kolor tekstu
        }
        else
        {
          u8g2.setDrawColor(1);
        }
        // Wyświetl ścieżkę
        //  u8g2.setDrawColor(1);
        u8g2.setFont(spleen6x12PL);
        u8g2.drawStr(0, displayRow * 13 + 8, String(displayedPath).c_str());

        // Przesuń się do kolejnego wiersza
        displayRow++;
      }
    }
    else
    {
      x == i;
      Serial.println("SystemVOLUME");
    }
  }
  // Przywróć domyślne ustawienia koloru rysowania (biały tekst na czarnym tle)
  u8g2.setDrawColor(1); // Biały kolor rysowania
  u8g2.sendBuffer();
}

// Funkcja do wylistowania katalogów z karty
void listDirectories(const char *dirname)
{
  File root = SD.open(dirname);
  if (!root)
  {
    Serial.println("1-Błąd otwarcia katalogu!");
    Serial.print("debug--ER-dirname:");
    Serial.println(dirname);
    return;
  }
  Serial.print("debug--dirname:");
  Serial.println(dirname);

  printDirectoriesAndSavePaths(root, 0, ""); // Początkowo pełna ścieżka jest pusta
  Serial.println("Wylistowano katalogi z karty SD");
  root.close();
  scrollDown();
  displayFolders();
}

// Funkcja do przewijania w górę
void scrollUp()
{
  if (currentSelection > 0)
  {
    currentSelection--;
    if (currentSelection < firstVisibleLine)
    {
      firstVisibleLine = currentSelection;
    }
  }
  else
  {
    // Jeśli osiągnięto wartość 0, przejdź do najwyższej wartości
    currentSelection = maxSelection();
    firstVisibleLine = currentSelection - maxVisibleLines + 1; // Ustaw pierwszą widoczną linię na najwyższą
  }

  Serial.print("Scroll Up: CurrentSelection = ");
  Serial.println(currentSelection);
}

// Obsługa kółka enkodera 1 podczas dzialania odtwarzacza plików
void handleEncoder1Rotation()
{
  CLK_state1 = digitalRead(CLK_PIN1);
  if (CLK_state1 != prev_CLK_state1 && CLK_state1 == HIGH)
  {
    timeDisplay = false;
    displayActive = true;
    displayStartTime = millis();
    if (digitalRead(DT_PIN1) == HIGH)
    {
      volumeValue--;
      if (volumeValue < 1)
      {
        volumeValue = 1;
      }
    }
    else
    {
      volumeValue++;
      if (volumeValue > 21)
      {
        volumeValue = 21;
      }
    }
    Serial.print("Wartość głośności: ");
    Serial.println(volumeValue);
    audio.setVolume(volumeValue);                // zakres 0...21
    String volumeValueStr = String(volumeValue); // Zamiana liczby VOLUME na ciąg znaków
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_fub14_tf);
    u8g2.drawStr(20, 33, "VOLUME");
    u8g2.drawStr(132, 33, volumeValueStr.c_str());
    u8g2.drawRFrame(21, 42, 214, 14, 3);
    u8g2.drawRBox(23, 44, volumeValue * 10, 10, 2);
    u8g2.sendBuffer();
  }
  prev_CLK_state1 = CLK_state1;
}

// Obsługa kółka enkodera 2 podczas dzialania odtwarzacza plików
void handleEncoder2Rotation()
{
  CLK_state2 = digitalRead(CLK_PIN2);
  if (CLK_state2 != prev_CLK_state2 && CLK_state2 == HIGH)
  {
    folderIndex = currentSelection; // Zaktualizuj indeks folderu
    timeDisplay = false;
    if (digitalRead(DT_PIN2) == HIGH)
    {
      folderIndex--;
      if (folderIndex < 0)
      {
        folderIndex = 0;
      }
      Serial.print("Numer folderu do tyłu: ");
      Serial.println(folderIndex);
      scrollUp();
      displayFolders();
    }
    else
    {
      folderIndex++;
      if (folderIndex > (directoryCount - 1))
      {
        folderIndex = directoryCount - 1;
      }
      Serial.print("Numer folderu do przodu: ");
      Serial.println(folderIndex);

      scrollDown();
      displayFolders();
    }
    displayActive = true;
    displayStartTime = millis();
  }
  prev_CLK_state2 = CLK_state2;
}

// Funkcja przywracająca wyświetlanie danych o utworze po przekroczeniu czasu bezczynności podczas odtwarzania plików audio z karty SD
void backDisplayPlayer()
{
  if (displayActive && (millis() - displayStartTime >= displayTimeout))
  {
    displayPlayer();
    displayActive = false;
    timeDisplay = true;
  }
}

// Funkcja do odtwarzania plików z wybranego folderu
void playFromSelectedFolder()
{
  folderNameString = currentDirectory + directories[folderIndex];
  Serial.println("Odtwarzanie plików z wybranego folderu: " + folderNameString);

  // Otwórz folder
  File root = SD.open(folderNameString);
  PlayedFolderName = folderNameString;                                          // Aktulanie odtwarzaczny folder
  PlayedFolderName = PlayedFolderName.substring(currentDirectory.length() + 1); // wycinamy z nazwy folderu informacje o katalogu gdzie trzymamy cała muzyke i wyswietlamy tylko docelowy katalog

  if (!root)
  {
    Serial.println("2-Błąd otwarcia katalogu!");
    Serial.print("debug--ER_FolderNameString: ");
    Serial.println(folderNameString);
    return;
  }
  Serial.print("debug--FolderNameString: ");
  Serial.println(folderNameString);

  totalFilesInFolder = 0;
  fileIndex = 1; // Zaczynamy odtwarzanie od pierwszego pliku audio w folderze

  // Zliczanie plików audio w folderze
  while (File entry = root.openNextFile())
  {
    String fileName = entry.name();
    Serial.print("debug--fileName: ");
    Serial.println(fileName);
    if (isAudioFile(fileName.c_str()))
    {
      totalFilesInFolder++;
    }
    entry.close(); // Zamykaj każdy plik natychmiast po zakończeniu przetwarzania
  }
  root.rewindDirectory(); // Przewiń katalog na początek

  bool playNextFolder = false; // Flaga kontrolująca przejście do kolejnego folderu

  // Odtwarzanie plików
  while (fileIndex <= totalFilesInFolder && !playNextFolder)
  {
    u8g2.clearBuffer();
    u8g2.setFont(spleen6x12PL);
    u8g2.sendBuffer();
    File entry = root.openNextFile();
    if (!entry)
    {
      break; // Koniec plików w folderze
    }

    String fileName = entry.name();

    // Pomijaj pliki, które nie są w zadeklarowanym formacie audio
    if (!isAudioFile(fileName.c_str()))
    {
      Serial.println("Pominięto plik: " + fileName);
      entry.close(); // Zamknij pominięty plik
      continue;
    }

    fileNameString = fileName;
    Serial.print("Odtwarzanie pliku: ");
    Serial.print(fileIndex); // Numeracja pliku
    Serial.print("/");
    Serial.print(totalFilesInFolder); // Łączna liczba plików w folderze
    Serial.print(" - ");
    Serial.println(fileName);

    // Pełna ścieżka do pliku
    String fullPath = folderNameString + "/" + fileName;

    Serial.print("debug--fullPath: ");
    Serial.println(fullPath);

    // Odtwarzaj plik
    audio.connecttoFS(SD, fullPath.c_str());
    seconds = 0;
    isPlaying = true;
    fileFromBuffer = fileIndex;
    folderFromBuffer = folderIndex;
    entry.close(); // Zamykaj plik po odczytaniu

    // Oczekuj na zakończenie odtwarzania

    while (isPlaying)
    {
      audio.loop(); // Tutaj obsługujemy odtwarzacz w tle
      button1.loop();
      button2.loop();

      // Jeśli skończył się plik, przejdź do następnego
      if (fileEnd)
      {
        fileEnd = false;
        id3tag = false;
        fileIndex++;
        break;
      }

      if (button2.isPressed())
      {
        audio.stopSong();
        // fileIndex++;
        playNextFolder = true;
        id3tag = false;
        break;
      }

      if (button1.isPressed())
      {
        audio.stopSong();
        encoderButton1 = true;
        break;
      }

      handleEncoder1Rotation(); // Obsługa kółka enkodera nr 1
      handleEncoder2Rotation(); // Obsługa kółka enkodera nr 2
      backDisplayPlayer();      // Obsługa bezczynności, przywrócenie wyświetlania danych audio
    }

    // Jeśli encoderButton1 aktywowany, wyjdź z pętli
    if (encoderButton1)
    {
      encoderButton1 = false;
      displayMenu();
      break;
    }

    // Sprawdź, czy zakończono odtwarzanie plików w folderze
    if (fileIndex > totalFilesInFolder)
    {
      Serial.println("To był ostatni plik w folderze, przechodzę do kolejnego folderu");
      playNextFolder = true;
      folderIndex++;
    }
  }

  // Przejdź do kolejnego folderu, jeśli ustawiono flagę
  if (playNextFolder)
  {
    if (folderIndex < directoryCount) // Upewnij się, że folderIndex nie przekroczy dostępnych folderów
    {
      playFromSelectedFolder(); // Wywołanie funkcji tylko raz
    }
    else
    {
      Serial.println("To był ostatni folder.");
    }
  }

  // Po zakończeniu zamknij katalog
  root.close();
}

void readVolumeFromSD()
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

    audio.setVolume(volumeValue); // zakres 0...21
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
  audio.setVolume(volumeValue); // zakres 0...21
  volumeBufferValue = volumeValue;
}

void saveVolumeOnSD()
{
  /*
   u8g2.clearBuffer();
   u8g2.setFont(u8g2_font_fub14_tf); // cziocnka 14x11
   u8g2.drawStr(1, 33, "Saving volume settings"); // 8 znakow  x 11 szer
   u8g2.sendBuffer();
  */
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

void drawSignalPower(uint8_t xpwr, uint8_t ypwr, bool print)
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

void rcInputKey(uint8_t i)
{
  rcInputDigitsMenuEnable = true;
  if (bankMenuEnable == true)
  {

    if (i == 0)
    {
      i = 10;
    }
    bank_nr = i;
    bankMenuDisplay();
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
      u8g2.drawStr(153, x, "_");
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

// Funkcja do zapisywania numeru stacji i numeru banku na karcie SD
void saveStationOnSD()
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

// Funkcja odpowiedzialna za zmianę aktualnie wybranej stacji radiowej.
void changeStation2()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);          // cziocnka 14x11
  u8g2.drawStr(34, 33, "Loading stream..."); // 8 znakow  x 11 szer
  u8g2.sendBuffer();

  mp3 = flac = aac = vorbis = false;
  stationFromBuffer = station_nr;
  stationString.remove(0); // Usunięcie wszystkich znaków z obiektu stationString
  stationNameStream.remove(0);

  // Tworzymy nazwę pliku banku
  String fileName = String("/bank") + (bank_nr < 10 ? "0" : "") + String(bank_nr) + ".txt";

  // Sprawdzamy, czy plik istnieje
  if (!SD.exists(fileName))
  {
    Serial.println("Błąd: Plik banku nie istnieje.");
    return;
  }

  // Otwieramy plik w trybie do odczytu
  File bankFile = SD.open(fileName, FILE_READ);
  if (!bankFile) // jesli brak pliku to...
  {
    Serial.println("Błąd: Nie można otworzyć pliku banku.");
    return;
  }

  // Przechodzimy do odpowiedniego wiersza pliku
  int currentLine = 0;
  String stationUrl = "";
  while (bankFile.available())
  {
    String line = bankFile.readStringUntil('\n');
    currentLine++;

    if (currentLine == station_nr)
    {
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
      break;
    }
  }
  bankFile.close(); // Zamykamy plik po odczycie
  // Sprawdzamy, czy znaleziono stację
  if (stationUrl.isEmpty())
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
    // seconds = 0;
    stationFromBuffer = station_nr;
    bankFromBuffer = bank_nr;
    saveStationOnSD();
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
  // screenRefresh = true;

  // screenRefreshTime = millis();
}

void changeStation()
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

  // Serial.println("-------- GRAMY OBECNIE ---------- ");
  // Serial.print(station_nr - 1);
  // Serial.print(" ");
  // Serial.println(String(station));

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

  // Serial.print("URL: ");
  // Serial.println(stationUrl);

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

    saveStationOnSD(); // Zapisujemy jaki numer stacji i który bank gramy
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

// Funkcja do wyświetlania listy stacji radiowych z opcją wyboru poprzez zaznaczanie w negatywie
void displayStations()
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

void updateTimerFlag()
{
  ActionNeedUpdateTime = true;
}

// Funkcja wywoływana co sekundę przez timer do aktualizacji czasu na wyświetlaczu
void updateTimer()
{
  // Wypełnij spacjami, aby wyczyścić pole
  // u8g2.drawStr(208, 63, "         "); // czyszczenie pola zegara
  // u8g2.drawStr(128, 63, "    "); // czyszczenie pola FLAC/MP3/AAC

  // Zwiększ licznik sekund
  seconds++;

  // Wyświetl aktualny czas w sekundach
  // Konwertuj sekundy na minutę i sekundy
  unsigned int minutes = seconds / 60;
  unsigned int remainingSeconds = seconds % 60;

  u8g2.setDrawColor(1); // Ustaw kolor na biały

  if (timeDisplay == true)
  {
    if ((audio.isRunning() == true) && (displayMode == 0) || (displayMode == 2))
    {
      if (mp3 == true)
      {
        u8g2.drawStr(133, 63, "MP3");
        // Serial.println("Gram MP3");
      }
      if (flac == true)
      {
        u8g2.drawStr(133, 63, "FLC");
        // Serial.println("Gram FLAC");
      }
      if (aac == true)
      {
        u8g2.drawStr(133, 63, "AAC");
        // Serial.println("Gram AAC");
      }
      if (vorbis == true)
      {
        u8g2.drawStr(133, 63, "VBR");
        // Serial.println("Gram AAC");
      }
    }

    if ((currentOption == PLAY_FILES) && (bitratePresent == true))
    {
      // Formatuj czas jako "mm:ss"
      char timeString[10];
      snprintf(timeString, sizeof(timeString), "%02um:%02us", minutes, remainingSeconds);
      u8g2.drawStr(210, 63, timeString);
      u8g2.sendBuffer();
    }

    // if ((currentOption == INTERNET_RADIO) && ((mp3 == true) || (flac == true) || (aac == true) || (vorbis == true)))
    if ((currentOption == INTERNET_RADIO) && (timeDisplay == true) && (audio.isRunning() == true))
    {
      // Struktura przechowująca informacje o czasie
      struct tm timeinfo;

      // Sprawdź, czy udało się pobrać czas z lokalnego zegara czasu rzeczywistego
      if (!getLocalTime(&timeinfo, 5))
      {
        // Wyświetl komunikat o niepowodzeniu w pobieraniu czasu
        Serial.println("Nie udało się uzyskać czasu");
        return; // Zakończ funkcję, gdy nie udało się uzyskać czasu
      }

      // Konwertuj godzinę, minutę i sekundę na stringi w formacie "HH:MM:SS"
      char timeString[9]; // Bufor przechowujący czas w formie tekstowej
      // snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

      if ((displayMode == 0) || (displayMode == 2))
      {
        snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        u8g2.setFont(spleen6x12PL);
        u8g2.drawStr(208, 63, timeString);
      }
      else if (displayMode == 1)
      {
        int xtime = 0;
        u8g2.setFont(u8g2_font_7Segments_26x42_mn);
        snprintf(timeString, sizeof(timeString), "%2d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        u8g2.drawStr(xtime + 7, 45, timeString);

        u8g2.setFont(u8g2_font_fub14_tf); // 14x11
        snprintf(timeString, sizeof(timeString), "%02d", timeinfo.tm_mday);
        u8g2.drawStr(203, 17, timeString);

        String month = "";
        switch (timeinfo.tm_mon)
        {
        case 0:
          month = "JAN";
          break;
        case 1:
          month = "FEB";
          break;
        case 2:
          month = "MAR";
          break;
        case 3:
          month = "APR";
          break;
        case 4:
          month = "MAY";
          break;
        case 5:
          month = "JUN";
          break;
        case 6:
          month = "JUL";
          break;
        case 7:
          month = "AUG";
          break;
        case 8:
          month = "SEP";
          break;
        case 9:
          month = "OCT";
          break;
        case 10:
          month = "NOV";
          break;
        case 11:
          month = "DEC";
          break;
        }
        u8g2.setFont(spleen6x12PL);
        u8g2.drawStr(232, 14, month.c_str());

        String dayOfWeek = "";
        switch (timeinfo.tm_wday)
        {
        case 0:
          dayOfWeek = " Sunday  ";
          break;
        case 1:
          dayOfWeek = " Monday  ";
          break;
        case 2:
          dayOfWeek = " Tuesday ";
          break;
        case 3:
          dayOfWeek = "Wednesday";
          break;
        case 4:
          dayOfWeek = "Thursday ";
          break;
        case 5:
          dayOfWeek = " Friday  ";
          break;
        case 6:
          dayOfWeek = "Saturday ";
          break;
        }

        u8g2.drawRBox(198, 20, 58, 15, 3); // Box z zaokraglonymi rogami, biały pod dniem tygodnia
        u8g2.drawLine(198, 20, 256, 20);   // Linia separacyjna dzien miesiac / dzien tygodnia
        u8g2.setDrawColor(0);
        u8g2.drawStr(201, 31, dayOfWeek.c_str());
        u8g2.setDrawColor(1);
        u8g2.drawRFrame(198, 0, 58, 35, 3); // Ramka na całosci kalendarza

        snprintf(timeString, sizeof(timeString), ":%02d", timeinfo.tm_sec);
        u8g2.drawStr(xtime + 163, 45, timeString);
      }

      // u8g2.sendBuffer(); // nie piszemy po ekranie w tej funkcji tylko przygotowujemy bufor. Nie mozna pisac podczas pracy scrollera
    }
    else if ((currentOption == INTERNET_RADIO) && (timeDisplay == true) && (audio.isRunning() == false))
    {

      // Struktura przechowująca informacje o czasie
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo, 5))
      {
        Serial.println("Nie udało się uzyskać czasu");
        return; // Zakończ funkcję, gdy nie udało się uzyskać czasu
      }
      char timeString[9]; // Bufor przechowujący czas w formie tekstowej

      snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      u8g2.setFont(spleen6x12PL);
      u8g2.drawStr(0, 63, "                         ");

      if (millis() - lastCheckTime >= 1000)
      {
        u8g2.drawStr(0, 63, "... No audio stream ! ...");
        lastCheckTime = millis(); // Zaktualizuj czas ostatniego sprawdzenia
      }
      u8g2.drawStr(208, 63, timeString);
    }
  }
}

void saveEqualizerOnSD()
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

void readEqualizerFromSD()
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
  audio.setTone(toneLowValue, toneMidValue, toneHiValue); // Ustawiamy filtry - zakres regulacji -40 + 6dB jako int8_t ze znakiem
}

// Funkcja do odczytu danych stacji radiowej z karty SD
void readStationFromSD()
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

void vuMeter()
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

void displayRadioScroller() // Funkcja odpwoiedzialna za przewijanie informacji strem tittle lub stringstation
{

  if (displayMode == 0) // Tryb normalny - radio
  {

    if (stationStringScroll.length() > 42)
    {

      xPositionStationString = offset;
      u8g2.setFont(spleen6x12PL);
      u8g2.setDrawColor(1);
      do
      {
        u8g2.drawStr(xPositionStationString, 33, stationStringScroll.c_str());
        xPositionStationString = xPositionStationString + stationStringScrollWidth;
      } while (xPositionStationString < 256);

      offset = offset - 1;
      if (offset < (65535 - stationStringScrollWidth))
      {
        offset = 0;
      }
    }
    else
    {
      xPositionStationString = 0;
      u8g2.setDrawColor(1);
      u8g2.setFont(spleen6x12PL);
      u8g2.drawStr(xPositionStationString, 33, stationStringScroll.c_str());
    }
  }
  else if (displayMode == 1) // Tryb zegara
  {
    if (stationStringScroll.length() > 42)
    {

      xPositionStationString = offset;
      u8g2.setFont(spleen6x12PL);
      u8g2.setDrawColor(1);
      do
      {
        u8g2.drawStr(xPositionStationString, 61, stationStringScroll.c_str());

        xPositionStationString = xPositionStationString + stationStringScrollWidth;
      } while (xPositionStationString < 256);

      offset = offset - 1;
      if (offset < (65535 - stationStringScrollWidth))
      {
        offset = 0;
      }
    }
    else
    {
      xPositionStationString = 0;
      u8g2.setDrawColor(1);
      u8g2.setFont(spleen6x12PL);
      u8g2.drawStr(xPositionStationString, 61, stationStringScroll.c_str());
    }
  }
  else if (displayMode == 2) // Tryb mały tekst
  {

    // Parametry do obługi wyświetlania w 3 kolejnych wierszach z podzialem do pełnych wyrazów
    const int maxLineLength = 41; // Maksymalna długość jednej linii w znakach
    String currentLine = "";      // Bieżąca linia
    int yPosition = 26;           // Początkowa pozycja Y

    // Podziel tekst na wyrazy
    String word;
    int wordStart = 0;

    for (int i = 0; i <= stationStringScroll.length(); i++)
    {
      // Sprawdź, czy dotarliśmy do końca słowa lub do końca tekstu
      if (i == stationStringScroll.length() || stationStringScroll.charAt(i) == ' ')
      {
        // Pobierz słowo
        String word = stationStringScroll.substring(wordStart, i);
        wordStart = i + 1;

        // Sprawdź, czy dodanie słowa do bieżącej linii nie przekroczy maxLineLength
        if (currentLine.length() + word.length() <= maxLineLength)
        {
          // Dodaj słowo do bieżącej linii
          if (currentLine.length() > 0)
          {
            currentLine += " "; // Dodaj spację między słowami
          }
          currentLine += word;
        }
        else
        {
          // Jeśli słowo nie pasuje, wyświetl bieżącą linię i przejdź do nowej linii
          u8g2.setFont(spleen6x12PL);
          u8g2.drawStr(0, yPosition, currentLine.c_str());
          yPosition += 12; // Przesunięcie w dół dla kolejnej linii
          // Zresetuj bieżącą linię i dodaj nowe słowo
          currentLine = word;
        }
      }
    }
    // Wyświetl ostatnią linię, jeśli coś zostało
    if (currentLine.length() > 0)
    {
      u8g2.setFont(spleen6x12PL);
      u8g2.drawStr(0, yPosition, currentLine.c_str());
    }
  }
}

void calcNec() // Funkcja umozliwajaca przeliczanie odwrotne aby "udawac" przyciskami klawiatury komendy piltoa w standardzie NEC
{
  // składamy kod pilota do postaci ADDR/CMD/CMD/ADDR aby miec 4 bajty
  uint8_t CMD = (ir_code >> 8) & 0xFF;
  uint8_t ADDR = ir_code & 0xFF;
  ir_code = ADDR;
  ir_code = (ir_code << 8) | CMD;
  ir_code = (ir_code << 8) | CMD;
  ir_code = (ir_code << 8) | ADDR;
  ADDR = (ir_code >> 24) & 0xFF;          // Pierwszy bajt
  uint8_t IADDR = (ir_code >> 16) & 0xFF; // Drugi bajt (inwersja adresu)
  CMD = (ir_code >> 8) & 0xFF;            // Trzeci bajt (komenda)
  uint8_t ICMD = ir_code & 0xFF;          // Czwarty bajt (inwersja komendy)

  // Dorabiamy brakujące odwórcone bajty
  IADDR = IADDR ^ 0xFF;
  ICMD = ICMD ^ 0xFF;

  // Składamy bajty w jeden ciąg
  ir_code = ICMD;
  ir_code = (ir_code << 8) | ADDR;
  ir_code = (ir_code << 8) | IADDR;
  ir_code = (ir_code << 8) | CMD;
  ir_code = reverse_bits(ir_code, 32); // rotacja bitów do porządku LSB-MSB jak w NEC
}

void handleKeyboard()
{
  // for (;;) {
  //  vTaskDelay(50);
  // int keyboardValue = analogRead(keyboardPin); // Odczyt wartości analogowej przycisku
  uint8_t key = 17;

  for (int i = 0; i < 32; i++)
  {
    keyboardValue = keyboardValue + analogRead(keyboardPin);
  }
  keyboardValue = keyboardValue / 32;

  if (debugKeyboard == 1)
  {
    Serial.print("debug - ADC odczyt: ");
    Serial.print(keyboardValue);
    Serial.print(" flaga przycisk wcisniety:");
    Serial.println(keyboardButtonPressed);
  }
  // Kasujemy flage nacisnietego przycisku tylko jesli nic nie jest wcisnięte
  if (keyboardValue > keyboardButtonNeutral - keyboardButtonThresholdTolerance)
  {
    keyboardButtonPressed = false;
  }
  // Jesli nic nie jest wcisniete i nic nie było nacisnięte analizujemy przycisk
  if (keyboardValue < keyboardButtonNeutral - keyboardButtonThresholdTolerance)
  {
    if (keyboardButtonPressed == false)
    {
      // Sprawdzamy stan klawiatury
      if ((keyboardValue <= keyboardButtonThreshold_1 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_1 - keyboardButtonThresholdTolerance))
      {
        key = 1;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_2 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_2 - keyboardButtonThresholdTolerance))
      {
        key = 2;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_3 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_3 - keyboardButtonThresholdTolerance))
      {
        key = 3;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_4 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_4 - keyboardButtonThresholdTolerance))
      {
        key = 4;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_5 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_5 - keyboardButtonThresholdTolerance))
      {
        key = 5;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_6 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_6 - keyboardButtonThresholdTolerance))
      {
        key = 6;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_7 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_7 - keyboardButtonThresholdTolerance))
      {
        key = 7;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_8 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_8 - keyboardButtonThresholdTolerance))
      {
        key = 8;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_9 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_9 - keyboardButtonThresholdTolerance))
      {
        key = 9;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_0 + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_0 - keyboardButtonThresholdTolerance))
      {
        key = 0;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Memory + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Memory - keyboardButtonThresholdTolerance))
      {
        key = 11;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Shift + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Shift - keyboardButtonThresholdTolerance))
      {
        key = 12;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Auto + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Auto - keyboardButtonThresholdTolerance))
      {
        key = 13;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Band + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Band - keyboardButtonThresholdTolerance))
      {
        key = 14;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Mute + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Mute - keyboardButtonThresholdTolerance))
      {
        key = 15;
        keyboardButtonPressed = true;
      }

      if ((keyboardValue <= keyboardButtonThreshold_Scan + keyboardButtonThresholdTolerance) && (keyboardValue >= keyboardButtonThreshold_Scan - keyboardButtonThresholdTolerance))
      {
        key = 16;
        keyboardButtonPressed = true;
      }

      if ((key < 17) && (keyboardButtonPressed == true))
      {
        Serial.print("ADC odczyt poprawny: ");
        Serial.print(keyboardValue);
        Serial.print(" przycisk: ");
        Serial.println(key);
        keyboardValue = 0;

        if ((debugKeyboard == false) && (key < 10)) // Dla przyciskoq 0-9 wykonujemy akcje jak na pilocie
        {
          rcInputKey(key);
        }
        else if ((debugKeyboard == false) && (key == 11)) // Przycisk Memory wywyłuje menu wyboru Banku
        {
          bankMenuDisplay();
        }
        else if ((debugKeyboard == false) && (key == 12)) // Przycisk Shift zatwierdza zmiane stacji, banku, działa jak "OK" na pilocie
        {
          ir_code = rcCmdOk; // Przycisk Auto TUning udaje OK
          bit_count = 32;
          calcNec(); // przeliczamy kod pilota na kod oryginalny pełen kod NEC

          // if (bankMenuEnable == true)
          // {
          //   station_nr = 1;
          //   fetchStationsFromServer();
          //   bankMenuEnable = false;
          // }
          // changeStation();
          // displayRadio();
          // u8g2.sendBuffer();
        }
        else if ((debugKeyboard == false) && (key == 13)) // Przycisk Auto - przełaczanie tryb Zegar/Radio
        {
          ir_code = rcCmdSrc; // Przycisk Auto TUning udaje Src
          bit_count = 32;
          calcNec(); // przeliczamy kod pilota na kod oryginalny pełen kod NEC
        }
        else if ((debugKeyboard == false) && (key == 14)) // Przycisk Band udaje Back
        {
          ir_code = rcCmdBack; // Udajemy komendy pilota
          bit_count = 32;
          calcNec(); // przeliczamy kod pilota na kod oryginalny pełen kod NEC
        }
        else if ((debugKeyboard == false) && (key == 15)) // Przycisk Mute
        {
          ir_code = rcCmdMute; // Przypisujemy kod polecenia z pilota
          bit_count = 32;      // ustawiamy informacje, ze mamy pelen kod NEC do analizy
          calcNec();           // przeliczamy kod pilota na kod oryginalny pełen kod NEC
        }
        else if ((debugKeyboard == false) && (key == 16)) // Przycisk Memory Scan - zmiana janości wyswietlacza - funkcja "Dimmer"
        {
          ir_code = rcCmdDirect; // Udajemy komendy pilota
          bit_count = 32;
          calcNec(); // przeliczamy kod pilota na kod oryginalny pełen kod NEC
        }

        key = 17; // Reset "KEY" do pozycji poza zakresem klawiatury
      }
    }
  }
  //  }
  // vTaskDelete(NULL);
}

void volumeDisplay()
{
  // volumeBufferValue = volumeValue;
  displayStartTime = millis();
  timeDisplay = false;
  displayActive = true;
  volumeSet = true;
  // volumeMute = false;
  Serial.print("Wartość głośności: ");
  Serial.println(volumeValue);
  audio.setVolume(volumeValue);                // zakres 0...21
  String volumeValueStr = String(volumeValue); // Zamiana liczby VOLUME na ciąg znaków
  u8g2.clearBuffer();
  // u8g2.setFont(DotMatrix13pl);
  u8g2.setFont(u8g2_font_fub14_tf);
  u8g2.drawStr(65, 33, "VOLUME");
  u8g2.drawStr(163, 33, volumeValueStr.c_str());

  u8g2.drawRFrame(21, 42, 214, 14, 3);            // Rysujmey ramke dla progress bara głosnosci
  u8g2.drawRBox(23, 44, volumeValue * 10, 10, 2); // Progress bar głosnosci
  u8g2.sendBuffer();
}

void volumeUp()
{
  // volumeBufferValue = volumeValue;
  volumeSet = true;
  timeDisplay = false;
  displayActive = true;
  volumeMute = false;
  displayStartTime = millis();
  volumeValue++;

  if (volumeValue > 21)
  {
    volumeValue = 21;
  }

  Serial.print("Wartość głośności: ");
  Serial.println(volumeValue);
  audio.setVolume(volumeValue);                // zakres 0...21
  String volumeValueStr = String(volumeValue); // Zamiana liczby VOLUME na ciąg znaków
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);
  u8g2.drawStr(65, 33, "VOLUME");
  u8g2.drawStr(163, 33, volumeValueStr.c_str());
  u8g2.drawRFrame(21, 42, 214, 14, 3);            // Rysujmey ramke dla progress bara głosnosci
  u8g2.drawRBox(23, 44, volumeValue * 10, 10, 2); // Progress bar głosnosci
  u8g2.sendBuffer();
}

void volumeDown()
{
  // volumeBufferValue = volumeValue;
  volumeSet = true;
  timeDisplay = false;
  displayActive = true;
  volumeMute = false;
  displayStartTime = millis();
  volumeValue--;
  if (volumeValue < 1)
  {
    volumeValue = 1;
  }
  Serial.print("Wartość głośności: ");
  Serial.println(volumeValue);
  audio.setVolume(volumeValue);                // zakres 0...21
  String volumeValueStr = String(volumeValue); // Zamiana liczby VOLUME na ciąg znaków
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);
  u8g2.drawStr(65, 33, "VOLUME");
  u8g2.drawStr(163, 33, volumeValueStr.c_str());
  u8g2.drawRFrame(21, 42, 214, 14, 3);            // Rysujmey ramke dla progress bara głosnosci
  u8g2.drawRBox(23, 44, volumeValue * 10, 10, 2); // Progress bar głosnosci
  u8g2.sendBuffer();
}

void bufforAudioInfo()
{
  Serial.print("debug--Bufor Audio pojemność / zapełniony:");
  Serial.print(audio.inBufferSize());
  Serial.print(" / ");
  Serial.println(audio.inBufferFilled());
  // Serial.println(audio.inBufferFree());
  // Serial.println(audio.inBufferSize());
  // Serial.println(audio.inBufferSize() - audio.inBufferFilled());
}

// Funkcja obsługująca przerwanie (reakcja na zmianę stanu pinu)
void IRAM_ATTR pulseISR()
{
  if (digitalRead(recv_pin) == HIGH)
  {
    pulse_start_high = micros(); // Zapis początku impulsu
  }
  else
  {
    pulse_end_high = micros(); // Zapis końca impulsu
    pulse_ready = true;
  }

  if (digitalRead(recv_pin) == LOW)
  {
    pulse_start_low = micros(); // Zapis początku impulsu
  }
  else
  {
    pulse_end_low = micros(); // Zapis końca impulsu
    pulse_ready_low = true;
  }

  // ----------- ANALIZA PULSOW -----------------------------
  if (pulse_ready_low) // spradzamy czy jest stan niski przez 9ms - start ramki
  {
    pulse_duration_low = pulse_end_low - pulse_start_low;

    if (pulse_duration_low > (LEAD_HIGH - TOLERANCE) && pulse_duration_low < (LEAD_HIGH + TOLERANCE))
    {
      pulse_duration_9ms = pulse_duration_low; // przypisz czas trwania puslu Low do zmiennej puls 9ms
      pulse_ready9ms = true;                   // flaga poprawnego wykrycia pulsu 9ms w granicach tolerancji
    }
  }

  // Sprawdzenie, czy impuls jest gotowy do analizy
  if ((pulse_ready == true) && (pulse_ready9ms = true))
  {
    pulse_ready = false;
    pulse_ready9ms = false; // kasujemy flage wykrycia pulsu 9ms

    // Obliczenie czasu trwania impulsu
    pulse_duration = pulse_end_high - pulse_start_high;
    // Serial.println(pulse_duration); odczyt dlugosci pulsow z pilota - debug
    if (!data_start_detected)
    {

      // Oczekiwanie na sygnał 4,5 ms wysoki
      if (pulse_duration > (LEAD_LOW - TOLERANCE) && pulse_duration < (LEAD_LOW + TOLERANCE))
      {
        pulse_duration_4_5ms = pulse_duration;
        // Początek sygnału: 4,5 ms wysoki

        data_start_detected = true; // Ustawienie flagi po wykryciu sygnału wstępnego
        bit_count = 0;              // Reset bit_count przed odebraniem danych
        ir_code = 0;                // Reset kodu IR przed odebraniem danych
      }
    }
    else
    {
      // Sygnały dla bajtów (adresu ADDR, IADDR, komendy CMD, ICMD) zaczynają się po wstępnym sygnale
      if (pulse_duration > (HIGH_THRESHOLD - TOLERANCE) && pulse_duration < (HIGH_THRESHOLD + TOLERANCE))
      {
        ir_code = (ir_code << 1) | 1; // Dodanie "1" do kodu IR
        bit_count++;
        pulse_duration_1690us = pulse_duration;
      }
      else if (pulse_duration > (LOW_THRESHOLD - TOLERANCE) && pulse_duration < (LOW_THRESHOLD + TOLERANCE))
      {
        ir_code = (ir_code << 1) | 0; // Dodanie "0" do kodu IR
        bit_count++;
        pulse_duration_560us = pulse_duration;
      }

      // Sprawdzenie, czy otrzymano pełny 32-bitowy kod IR
      if (bit_count == 32)
      {
        // Rozbicie kodu na 4 bajty
        uint8_t ADDR = (ir_code >> 24) & 0xFF;  // Pierwszy bajt
        uint8_t IADDR = (ir_code >> 16) & 0xFF; // Drugi bajt (inwersja adresu)
        uint8_t CMD = (ir_code >> 8) & 0xFF;    // Trzeci bajt (komenda)
        uint8_t ICMD = ir_code & 0xFF;          // Czwarty bajt (inwersja komendy)

        // Sprawdzenie poprawności (inwersja) bajtów adresu i komendy
        if ((ADDR ^ IADDR) == 0xFF && (CMD ^ ICMD) == 0xFF)
        {
          data_start_detected = false;
          // bit_count = 0;
        }
        else
        {
          ir_code = 0;
          data_start_detected = false;
          // bit_count = 0;
        }
      }
    }
  }
  // runTime2 = esp_timer_get_time();
}

void readRcStoredCodes(uint8_t x) // Odczyt kdów pilota - funkcja eksperymentalna, testowa
{
  displayStartTime = millis(); // Uaktulniamy czas dla funkcji auto-pwrotu z menu
  displayActive = true;        // Wyswietlacz aktywny
  equalizerMenuEnable = false; // Ustawiamy flage menu equalizera
  timeDisplay = false;         // Wyłaczamy zegar

  u8g2.clearBuffer();
  u8g2.setFont(spleen6x12PL);

  /* Strona  1 z 3 */
  if (x == 0)
  {
    u8g2.setCursor(0, 13);
    u8g2.print("Vol +:" + String(rcCmdVolumeUp, HEX) + " Down:" + String(rcCmdArrowDown, HEX) + " Bank-:" + String(rcCmdBankMinus, HEX) + " Equ:" + String(rcCmdAud, HEX));
    u8g2.setCursor(0, 26);
    u8g2.print("Vol -:" + String(rcCmdVolumeDown, HEX) + " Back:" + String(rcCmdBack, HEX) + " Bank+:" + String(rcCmdBankPlus, HEX));
    u8g2.setCursor(0, 39);
    u8g2.print("Left: " + String(rcCmdArrowRight, HEX) + " Ok:  " + String(rcCmdOk, HEX) + " Red:  " + String(rcCmdRed, HEX));
    u8g2.setCursor(0, 52);
    u8g2.print("Right:" + String(rcCmdArrowLeft, HEX) + " Src: " + String(rcCmdSrc, HEX) + " Green:" + String(rcCmdGreen, HEX));
    u8g2.setCursor(0, 63);
    u8g2.print("Up:   " + String(rcCmdArrowUp, HEX) + " Mute:" + String(rcCmdMute, HEX) + " Dim:  " + String(rcCmdDirect, HEX));
  }

  /* Strona  2 z 3 */
  if (x == 1)
  {
    u8g2.setCursor(0, 13);
    u8g2.print("Key 0:" + String(rcCmdKey0, HEX) + "  Key 6:" + String(rcCmdKey0, HEX));
    u8g2.setCursor(0, 26);
    u8g2.print("Key 1:" + String(rcCmdKey1, HEX) + "  Key 7:" + String(rcCmdKey0, HEX));
    u8g2.setCursor(0, 39);
    u8g2.print("Key 2:" + String(rcCmdKey2, HEX) + "  Key 8:" + String(rcCmdKey0, HEX));
    u8g2.setCursor(0, 52);
    u8g2.print("Key 3:" + String(rcCmdKey3, HEX) + "  Key 9:" + String(rcCmdKey0, HEX));
    u8g2.setCursor(0, 63);
    u8g2.print("Key 4:" + String(rcCmdKey0, HEX) + "  Key 0:" + String(rcCmdKey0, HEX));
  }

  /* Strona  3 z 3 */
  if (x == 2)
  {
    u8g2.setCursor(0, 60);
    u8g2.print("Key 5:" + String(rcCmdKey0, HEX) + "  Key 0:" + String(rcCmdKey0, HEX));
  }

  u8g2.sendBuffer();
}

void displayEqualizer() // Funkcja rysująca menu 3-punktowego equalizera
{

  displayStartTime = millis(); // Uaktulniamy czas dla funkcji auto-pwrotu z menu
  equalizerMenuEnable = true;  // Ustawiamy flage menu equalizera
  timeDisplay = false;         // Wyłaczamy zegar
  displayActive = true;        // Wyswietlacz aktywny

  Serial.println("--Equalizer--");
  Serial.print("Wartość tonów Niskich/Low:   ");
  Serial.println(toneLowValue);
  Serial.print("Wartość tonów Średnich/Mid:  ");
  Serial.println(toneMidValue);
  Serial.print("Wartość tonów Wysokich/High: ");
  Serial.println(toneHiValue);

  audio.setTone(toneLowValue, toneMidValue, toneHiValue); // Zakres regulacji -40 + 6dB jako int8_t ze znakiem

  u8g2.setDrawColor(1);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);
  u8g2.drawStr(60, 14, "EQUALIZER");
  // u8g2.drawStr(1, 14, "EQUALIZER");
  u8g2.setFont(spleen6x12PL);
  // u8g2.setCursor(138,12);
  // u8g2.print("P1    P2    P3    P4");
  uint8_t xTone;
  uint8_t yTone;

  // ---- Tony Wysokie ----
  xTone = 0;
  yTone = 28;
  u8g2.setCursor(xTone, yTone);
  if (toneHiValue >= 0)
  {
    u8g2.print("High:  " + String(toneHiValue) + "dB");
  } // Dla wartosci dodatnich piszemy normalnie
  if ((toneHiValue < 0) && (toneHiValue >= -9))
  {
    u8g2.print("High: " + String(toneHiValue) + "dB");
  } // Dla wartosci ujemnych piszemy o 1 znak wczesniej
  if ((toneHiValue < 0) && (toneHiValue < -9))
  {
    u8g2.print("High:" + String(toneHiValue) + "dB");
  } // Dla wartosci ujemnych ponizej -9 piszemy o 2 znak wczesniej

  xTone = 20;
  if (toneSelect == 1)
  {
    u8g2.drawRBox(xTone + 56, yTone - 9, 154, 9, 1); // Rysujemy biały pasek pod regulacją danego tonu
    u8g2.setDrawColor(0);
    u8g2.drawLine(xTone + 57, yTone - 5, xTone + 208, yTone - 5);
    // u8g2.drawLine(xTone+57,yTone-4,xTone+208,yTone-4);
    if (toneHiValue >= 0)
    {
      u8g2.drawRBox((10 * toneHiValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    if (toneHiValue < 0)
    {
      u8g2.drawRBox((2 * toneHiValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    u8g2.setDrawColor(1);
  }
  else
  {
    u8g2.drawLine(xTone + 57, yTone - 5, xTone + 208, yTone - 5);
    // u8g2.drawLine(xTone+57,yTone-4,xTone+208,yTone-4);
    if (toneHiValue >= 0)
    {
      u8g2.drawRBox((10 * toneHiValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    if (toneHiValue < 0)
    {
      u8g2.drawRBox((2 * toneHiValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    // u8g2.drawRBox((3 * toneHiValue) + xTone + 178,yTone-7,10,6,1);
  }

  // ---- Tony średnie ----
  xTone = 0;
  yTone = 46;
  u8g2.setCursor(xTone, yTone);
  if (toneMidValue >= 0)
  {
    u8g2.print("Mid:   " + String(toneMidValue) + "dB");
  } // Dla wartosci dodatnich piszemy normalnie
  if ((toneMidValue < 0) && (toneMidValue >= -9))
  {
    u8g2.print("Mid:  " + String(toneMidValue) + "dB");
  }
  if ((toneMidValue < 0) && (toneMidValue < -9))
  {
    u8g2.print("Mid: " + String(toneMidValue) + "dB");
  }

  xTone = 20;
  if (toneSelect == 2)
  {
    u8g2.drawRBox(xTone + 56, yTone - 9, 154, 9, 1);
    u8g2.setDrawColor(0);
    u8g2.drawLine(xTone + 57, yTone - 5, xTone + 208, yTone - 5);
    // u8g2.drawLine(xTone+57,yTone-4,xTone + 208,yTone-4);
    if (toneMidValue >= 0)
    {
      u8g2.drawRBox((10 * toneMidValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    if (toneMidValue < 0)
    {
      u8g2.drawRBox((2 * toneMidValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    u8g2.setDrawColor(1);
  }
  else
  {
    u8g2.drawLine(xTone + 57, yTone - 5, xTone + 208, yTone - 5);
    // u8g2.drawLine(xTone+57,yTone-4,xTone + 208,yTone-4);
    if (toneMidValue >= 0)
    {
      u8g2.drawRBox((10 * toneMidValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    if (toneMidValue < 0)
    {
      u8g2.drawRBox((2 * toneMidValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    // u8g2.drawRBox((3 * toneMidValue) + xTone + 138,yTone-7,10,6,1);
  }

  // Tony niskie
  xTone = 0;
  yTone = 64;
  u8g2.setCursor(xTone, yTone);
  if (toneLowValue >= 0)
  {
    u8g2.print("Low:   " + String(toneLowValue) + "dB");
  }
  if ((toneLowValue < 0) && (toneLowValue >= -9))
  {
    u8g2.print("Low:  " + String(toneLowValue) + "dB");
  }
  if ((toneLowValue < 0) && (toneLowValue < -9))
  {
    u8g2.print("Low: " + String(toneLowValue) + "dB");
  }
  xTone = 20;
  if (toneSelect == 3)
  {
    u8g2.drawRBox(xTone + 56, yTone - 9, 154, 9, 1);
    u8g2.setDrawColor(0);
    u8g2.drawLine(xTone + 57, yTone - 5, xTone + 208, yTone - 5);
    // u8g2.drawLine(xTone + 57,yTone-4,xTone + 208,yTone-4);
    if (toneLowValue >= 0)
    {
      u8g2.drawRBox((10 * toneLowValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    if (toneLowValue < 0)
    {
      u8g2.drawRBox((2 * toneLowValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    u8g2.setDrawColor(1);
  }
  else
  {
    u8g2.drawLine(xTone + 57, yTone - 5, xTone + 208, yTone - 5);
    // u8g2.drawLine(xTone + 57,yTone-4,xTone + 208,yTone-4);
    if (toneLowValue >= 0)
    {
      u8g2.drawRBox((10 * toneLowValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    if (toneLowValue < 0)
    {
      u8g2.drawRBox((2 * toneLowValue) + xTone + 138, yTone - 7, 10, 5, 1);
    }
    // u8g2.drawRBox((3 * toneLowValue) + xTone + 138,yTone-7,10,6,1);
  }
  u8g2.sendBuffer();
}

void displayBasicInfo()
{
  displayStartTime = millis(); // Uaktulniamy czas dla funkcji auto-powrotu z menu
  timeDisplay = false;         // Wyłaczamy zegar
  displayActive = true;        // Wyswietlacz aktywny
  u8g2.clearBuffer();
  u8g2.setFont(spleen6x12PL);
  u8g2.drawStr(0, 10, "Info:");
  u8g2.setCursor(0, 25);
  u8g2.print("ESP32 SN:" + String(ESP.getEfuseMac()) + ",  FW Ver.:" + String(softwareRev));
  u8g2.setCursor(0, 38);
  u8g2.print("Hostname:" + String(hostname) + ",  WiFi Signal:" + String(WiFi.RSSI()) + "dBm");
  u8g2.setCursor(0, 51);
  u8g2.print("WiFi SSID:" + String(wifiManager.getWiFiSSID()));
  u8g2.setCursor(0, 64);
  u8g2.print("IP:" + currentIP + "  MAC:" + String(WiFi.macAddress()));

  u8g2.sendBuffer();
}

void audioProcessing(void *p)
{
  while (true)
  {
    audio.loop();
    vTaskDelay(1 / portTICK_PERIOD_MS); // Opóźnienie 1 milisekundy
  }
}

void handlePreOtaUpdateCallback()
{
  Update.onProgress([](unsigned int progress, unsigned int total)
                    {
    u8g2.setCursor(1,56); u8g2.printf("Update: %u%%\r", (progress / (total / 100)) );
    //progress = progres / (total / 100) ;
    u8g2.setCursor(80,56); u8g2.print(String(progress) + "/" + String(total));
    u8g2.sendBuffer();
    Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
}

void recoveryModeCheck()
{
  if (digitalRead(SW_PIN2) == 0)
  {
    int8_t recoveryMode = 0;
    int16_t recoveryModeCounter = 0;
    u8g2.clearBuffer();
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(1, 14, "RECOVERY / RESET MODE - release encoder");
    u8g2.sendBuffer();
    delay(2000);
    while (digitalRead(SW_PIN2) == 0)
    {
      ;
    }
    u8g2.drawStr(1, 14, "Please Wait...                         ");
    u8g2.sendBuffer();
    delay(1000);
    u8g2.clearBuffer();

    while (true)
    {
      u8g2.drawStr(1, 14, "[ -- Rotate Enckoder -- ]            ");
      CLK_state2 = digitalRead(CLK_PIN2);
      if (CLK_state2 != prev_CLK_state2 && CLK_state2 == HIGH) // Sprawdzenie, czy stan CLK zmienił się na wysoki
      {
        if (digitalRead(DT_PIN2) == HIGH)
        {
          recoveryMode++;
          if (recoveryMode > 2)
          {
            recoveryMode = 2;
          }
        }
        else
        {
          recoveryMode--;
          if (recoveryMode < 0)
          {
            recoveryMode = 0;
          }
        }
      }

      if (recoveryMode == 0)
      {
        u8g2.drawStr(1, 28, ">> RESET BANK=1, STATION=1 <<");
        u8g2.drawStr(1, 42, "   RESET WIFI SSID, PASSWD   ");
        u8g2.drawStr(1, 56, "   WEB UPDATE PORTAL         ");
      }
      else if (recoveryMode == 1)
      {
        u8g2.drawStr(1, 28, "   RESET BANK=1, STATION=1   ");
        u8g2.drawStr(1, 42, ">> RESET WIFI SSID, PASSWD <<");
        u8g2.drawStr(1, 56, "   WEB UPDATE PORTAL         ");
      }

      else if (recoveryMode == 2)
      {
        u8g2.drawStr(1, 28, "   RESET BANK=1, STATION=1   ");
        u8g2.drawStr(1, 42, "   RESET WIFI SSID, PASSWD   ");
        u8g2.drawStr(1, 56, ">> WEB UPDATE PORTAL       <<");
      }
      u8g2.sendBuffer();

      if (digitalRead(SW_PIN2) == 0)
      {
        if (recoveryMode == 0)
        {
          bank_nr = 1;
          station_nr = 1;
          saveStationOnSD();
          u8g2.clearBuffer();
          u8g2.drawStr(1, 14, "SET BANK=1, STATION=1         ");
          u8g2.drawStr(1, 28, "ESP will RESET in 3sec.       ");
          u8g2.sendBuffer();
          delay(3000);
          ESP.restart();
        }
        else if (recoveryMode == 1)
        {
          u8g2.clearBuffer();
          u8g2.drawStr(1, 14, "WIFI SSID, PASSWD CLEARED   ");
          u8g2.drawStr(1, 28, "ESP will RESET in 3sec.     ");
          u8g2.sendBuffer();
          wifiManager.resetSettings();
          delay(3000);
          ESP.restart();
        }
        else if (recoveryMode == 2)
        {
          u8g2.clearBuffer();
          u8g2.drawStr(1, 14, "WEB  PORTAL STARTED         ");
          u8g2.drawStr(1, 28, "Connect to WiFi ESP-Radio   ");
          u8g2.drawStr(1, 42, "Open http://192.168.4.1     ");
          u8g2.sendBuffer();
          // wifiManager.startWebPortal();
          wifiManager.startConfigPortal("ESP32-Radio");
          delay(3000);
          while (true)
          {
            wifiManager.process();
            wifiManager.setPreOtaUpdateCallback(handlePreOtaUpdateCallback);
          }
        }
      }
      prev_CLK_state2 = CLK_state2;
    }
  }
}

void displayDimmer(bool dimmerON)
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

// Funkcja kasuje wszystkie flagi przebywania w menu, funkcjach itd. Pozwala pwrócic do wyswietlania ekranu głownego
void clearFlags() 
{
  displayDimmer(0);
  displayActive = false;
  timeDisplay = true;
  listedStations = false;
  menuEnable = false;
  volumeSet = false;
  bankMenuEnable = false;
  bankNetworkUpdate = false;
  equalizerMenuEnable = false;
  rcInputDigitsMenuEnable = false;
  rcInputDigit1 = 0xFF; // czyscimy cyfre 1, flaga pustej zmiennej to FF
  rcInputDigit2 = 0xFF; // czyscimy cyfre 2, flaga pustej zmiennej to FF
  currentOption = INTERNET_RADIO;

  station_nr = stationFromBuffer;
  bank_nr = previous_bank_nr;
}

void displayDimmerTimer()
{
  displayDimmerTimeCounter++;
  if (displayActive == true)
  {
    displayDimmerTimeCounter = 0;
    displayDimmer(0);
  }
  if (displayDimmerTimeCounter >= displayAutoDimmerTime)
  {
    displayDimmer(1); // wywolujemy funkcje przyciemnienia z parametrem 1 (załacz)
    displayDimmerTimeCounter = 0;
  }
}

void handleEncoder2StationsVolumeClick()
{
  CLK_state2 = digitalRead(CLK_PIN2);                      // Odczytanie aktualnego stanu pinu CLK enkodera 2
  if (CLK_state2 != prev_CLK_state2 && CLK_state2 == HIGH) // Sprawdzenie, czy stan CLK zmienił się na wysoki
  {
    timeDisplay = false;
    displayActive = true;
    displayStartTime = millis();

    if ((currentOption == INTERNET_RADIO) && (volumeSet == false) && (bankMenuEnable == false)) // Przewijanie listy stacji radiowych
    {
      station_nr = currentSelection + 1;
      if (digitalRead(DT_PIN2) == HIGH)
      {
        station_nr--;
        // if (listedStations == 1) station_nr--;
        if (station_nr < 1)
        {
          station_nr = stationsCount; // 1;
        }
        Serial.print("Numer stacji do tyłu: ");
        Serial.println(station_nr);
        scrollUp();
      }
      else
      {
        station_nr++;
        // if (listedStations == 1) station_nr++;
        if (station_nr > stationsCount)
        {
          station_nr = 1; // stationsCount;
        }
        Serial.print("Numer stacji do przodu: ");
        Serial.println(station_nr);
        scrollDown();
      }
      displayStations();
    }
    else
    {
      if ((currentOption == INTERNET_RADIO) && (bankMenuEnable == false))
      {
        if (digitalRead(DT_PIN2) == HIGH) // pokrecenie enkoderem 2
        {
          volumeDown();
        }
        else
        {
          volumeUp();
        }
        // volumeDisplay();
      }
    }

    if (bankMenuEnable == true) // Przewijanie listy banków stacji radiowych
    {
      if (digitalRead(DT_PIN2) == HIGH)
      {
        bank_nr--;
        if (bank_nr < 1)
        {
          bank_nr = 16;
        }
      }
      else
      {
        bank_nr++;
        if (bank_nr > 16)
        {
          bank_nr = 1;
        }
      }
      bankMenuDisplay();
    }
  }
  prev_CLK_state2 = CLK_state2;

  if ((currentOption == INTERNET_RADIO) && (button2.isReleased()) && (listedStations == true))
  {
    listedStations = false;
    volumeSet = false;
    changeStation();
    displayRadio();
    u8g2.sendBuffer();
    clearFlags();
  }

  if ((currentOption == INTERNET_RADIO) && (button2.isPressed()) && (listedStations == false) && (bankMenuEnable == false))
  {
    displayStartTime = millis();
    timeDisplay = false;
    volumeSet = true;
    displayActive = true;
    volumeDisplay(); // Po nacisnieciu enkodera2 wyswietlamy menu głośnosci
  }

  if ((button2.isPressed()) && (bankMenuEnable == true))
  {
    previous_bank_nr = bank_nr;

    station_nr = 1;
    currentOption = INTERNET_RADIO;

    fetchStationsFromServer();
    changeStation();
    u8g2.clearBuffer();
    displayRadio();

    volumeSet = false;
    bankMenuEnable = false;
  }
}

void handleEncoder2VolumeStationsClick()
{
  CLK_state2 = digitalRead(CLK_PIN2);                      // Odczytanie aktualnego stanu pinu CLK enkodera 2
  if (CLK_state2 != prev_CLK_state2 && CLK_state2 == HIGH) // Sprawdzenie, czy stan CLK zmienił się na wysoki
  {
    timeDisplay = false;
    displayActive = true;
    displayStartTime = millis();

    if ((currentOption == INTERNET_RADIO) && (listedStations == false) && (bankMenuEnable == false)) // Przewijanie listy stacji radiowych
    {
      if (digitalRead(DT_PIN2) == HIGH)
      {
        volumeDown();
      }
      else
      {
        volumeUp();
      }
    }
    else
    {
      if ((currentOption == INTERNET_RADIO) && (bankMenuEnable == false) && (volumeSet == true))
      {
        if (digitalRead(DT_PIN2) == HIGH) // pokrecenie enkoderem 2
        {
          volumeDown();
        }
        else
        {
          volumeUp();
        }
      }
    }

    if ((currentOption == INTERNET_RADIO) && (listedStations == true) && (bankMenuEnable == false))
    {
      station_nr = currentSelection + 1;
      if (digitalRead(DT_PIN2) == HIGH)
      {
        station_nr--;
        if (station_nr < 1)
        {
          station_nr = stationsCount;
        }
        scrollUp();
      }
      else
      {
        station_nr++;
        if (station_nr > stationsCount)
        {
          station_nr = 1;
        } // stationsCount;
        scrollDown();
      }
      displayStations();
    }

    if (bankMenuEnable == true) // Przewijanie listy banków stacji radiowych
    {
      if (digitalRead(DT_PIN2) == HIGH)
      {
        bank_nr--;
        if (bank_nr < 1)
        {
          bank_nr = 16;
        }
      }
      else
      {
        bank_nr++;
        if (bank_nr > 16)
        {
          bank_nr = 1;
        }
      }
      bankMenuDisplay();
    }
  }
  prev_CLK_state2 = CLK_state2;

  if ((currentOption == INTERNET_RADIO) && (button2.isReleased()) && (encoderButton2 == true)) // jestesmy juz w menu listy stacji to zmieniamy stacje po nacisnieciu przycisku
  {
    encoderButton2 = false;
    //  Serial.println("debug--------------------------------> SET ENCODER button 2 FALSE");
  }

  if ((currentOption == INTERNET_RADIO) && (button2.isPressed())) // zmieniamy stację
  {
    if ((encoderButton2 == false) && (listedStations == true) && (bankMenuEnable == false) && (volumeSet == false)) // jestesmy juz w menu listy stacji to zmieniamy stacje po nacisnieciu przycisku
    {

      timeDisplay = true;
      listedStations = false;
      equalizerMenuEnable = false;
      encoderButton2 = true;

      u8g2.clearBuffer();
      changeStation();
      displayRadio();
      // u8g2.sendBuffer();
    }

    else if ((encoderButton2 == false) && (listedStations == false) && (bankMenuEnable == false) && (volumeSet == false)) // wchodzimy do listy
    {
      displayStartTime = millis();
      timeDisplay = false;
      displayActive = true;
      listedStations = true;
      currentSelection = station_nr - 1;

      if (currentSelection >= 0)
      {
        if (currentSelection < firstVisibleLine) // jezeli obecne zaznaczenie ma wartosc mniejsza niz pierwsza wyswietlana linia
        {
          firstVisibleLine = currentSelection;
        }
      }
      else
      { // Jeśli osiągnięto wartość 0, przejdź do najwyższej wartości
        if (currentSelection = maxSelection())
        {
          firstVisibleLine = currentSelection - maxVisibleLines + 1; // Ustaw pierwszą widoczną linię na najwyższą
        }
      }
      displayStations();
      // Serial.println("debug--------------------------------------------------> button 2 PRESSED station list");
    }

    else if ((listedStations == false) && (bankMenuEnable == true) && (volumeSet == false))
    {
      displayStartTime = millis();
      timeDisplay = false;
      displayActive = true;

      previous_bank_nr = bank_nr;
      station_nr = 1;
      currentOption = INTERNET_RADIO;

      listedStations = false;
      volumeSet = false;
      bankMenuEnable = false;
      bankNetworkUpdate = false;

      fetchStationsFromServer();
      changeStation();
      u8g2.clearBuffer();
      displayRadio();
      // u8g2.sendBuffer();
    }
  }
}

void handleEncoder1()
{
  CLK_state1 = digitalRead(CLK_PIN1); // Odczytanie aktualnego stanu pinu CLK enkodera 1
  if (CLK_state1 != prev_CLK_state1 && CLK_state1 == HIGH)
  {
    timeDisplay = false;
    displayActive = true;
    displayStartTime = millis();

    if (menuEnable == true) // Przewijanie menu prawym enkoderem
    {
      int DT_state1 = digitalRead(DT_PIN1);
      switch (currentOption)
      {
      case PLAY_FILES:
        if (DT_state1 == HIGH)
        {
          currentOption = PLAY_FILES;
        }
        else
        {
          currentOption = INTERNET_RADIO;
        }
        break;

      case INTERNET_RADIO:
        if (DT_state1 == HIGH)
        {
          currentOption = PLAY_FILES;
        }
        else
        {
          currentOption = INTERNET_RADIO;
        }
        break;
      }
      displayMenu();
    }
    else // Regulacja głośności
    {
      if (digitalRead(DT_PIN1) == HIGH)
      {
        volumeDown();
      }
      else
      {
        volumeUp();
      }
    }
  }
  prev_CLK_state1 = CLK_state1;

  if ((currentOption == PLAY_FILES) && (button1.isPressed()) && (menuEnable == true))
  {
    if (!SD.begin(SD_CS))
    {
      Serial.println("Błąd inicjalizacji karty SD!");
      return;
    }

    folderIndex = 0;
    currentSelection = 0;
    firstVisibleLine = 1;
    // listDirectories("/music");
    listDirectories(currentDirectory.c_str());
    audio.stopSong();
    playFromSelectedFolder();
  }

  if ((currentOption == INTERNET_RADIO) && (button1.isPressed()) && (menuEnable == true))
  {
    menuEnable = false;
    changeStation();
  }

  if ((button1.isPressed()) && (bankMenuEnable == true))
  {
    bankMenuEnable = false;
    volumeSet = false;
    bankNetworkUpdate = true;
    currentSelection = 0;
    firstVisibleLine = 0;
    station_nr = 1;
    currentOption = INTERNET_RADIO;

    fetchStationsFromServer();
    changeStation();
    bankNetworkUpdate = false;
    u8g2.clearBuffer();
    displayRadio();
  }
}

void drawSwitch(uint8_t x, uint8_t y, bool state) // Ikona przełacznika szeroka (x) na 21, wysoka(y) na 10
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

void displayConfig()
{
  displayStartTime = millis(); // Uaktulniamy czas dla funkcji auto-pwrotu z menu
  equalizerMenuEnable = true;  // Ustawiamy flage menu equalizera
  timeDisplay = false;         // Wyłaczamy zegar
  displayActive = true;        // Wyswietlacz aktywny

  u8g2.setFont(spleen6x12PL);

  // Strona 1
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Menu Config:");
  // drawSwitch(0,15,displayAutoDimmerOn); u8g2.setCursor(25,24); u8g2.print("Display auto dimmer   Auto dimmmer time:" + String(displayAutoDimmerTime) + "s");

  // u8g2.setCursor(0,25); u8g2.print("Display auto dimmer:" + String(ESP.getEfuseMac()) + ",  FW Ver.:" + String(softwareRev));

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

void saveConfig()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub14_tf);            // cziocnka 14x11
  u8g2.drawStr(1, 33, "Saving configuration"); // 8 znakow  x 11 szer
  u8g2.sendBuffer();

  // Sprawdź, czy plik istnieje
  if (SD.exists("/config.txt"))
  {
    Serial.println("Plik config.txt już istnieje.");

    // Otwórz plik do zapisu i nadpisz aktualną wartość konfiguracji
    myFile = SD.open("/config.txt", FILE_WRITE);
    if (myFile)
    {
      // myFile.print << "display auto dimmer on / automatyczne przyciemnianie wyswietlacza =" << displayAutoDimmerOn << '\n';
      myFile.println("#### ESP32 Radio Config File ####");
      myFile.print("display auto dimmer on =");
      myFile.print(displayAutoDimmerOn);
      myFile.println(";");
      myFile.print("display auto dimmer timer =");
      myFile.print(displayAutoDimmerTime);
      myFile.println(";");
      myFile.println("display auto dimmer timer String =" + String(displayAutoDimmerTime) + ";");
      myFile.close();
      Serial.println("Aktualizacja config.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas otwierania pliku config.txt.");
    }
  }
  else
  {
    Serial.println("Plik config.txt nie istnieje. Tworzenie...");

    // Utwórz plik i zapisz w nim aktualne wartości konfiguracji
    myFile = SD.open("/config.txt", FILE_WRITE);
    if (myFile)
    {
      // myFile.println("display auto dimmer on / automatyczne przyciemnianie wyswietlacza =" << autoDimmerOn << '\n');
      // myFile.println("display auto dimmer timer / czas po jakim nastapi automatyczne przyciemnianie wyswietlacza =" + ParseInt(displayAutoDimmerTime));
      myFile.println("#### ESP32 Radio Config File ####");
      myFile.print("display auto dimmer on  =");
      myFile.print(displayAutoDimmerOn);
      myFile.println(";");
      myFile.print("display auto dimmer timer  =");
      myFile.print(displayAutoDimmerTime);
      myFile.println(";");
      myFile.println("display auto dimmer timer  =" + String(displayAutoDimmerTime) + ";");
      myFile.close();
      Serial.println("Utworzono i zapisano config.txt na karcie SD.");
    }
    else
    {
      Serial.println("Błąd podczas tworzenia pliku config.txt.");
    }
  }
}

void readConfig()
{

  Serial.println("Odczyt pliku config.txt z karty");

  // Tworzymy nazwę pliku banku
  String fileName = String("/config.txt");

  // Sprawdzamy, czy plik istnieje
  if (!SD.exists(fileName))
  {
    Serial.println("Błąd: Plik banku nie istnieje.");
    return;
  }

  // Otwieramy plik w trybie do odczytu
  File configFile = SD.open(fileName, FILE_READ);
  if (!configFile) // jesli brak pliku to...
  {
    Serial.println("Błąd: Nie można otworzyć pliku konfiguracji");
    return;
  }
  // Przechodzimy do odpowiedniego wiersza pliku
  int currentLine = 0;
  String configValue = "";
  while (configFile.available()) // & currentLine <= MAX_STATIONS)
  {
    String line = configFile.readStringUntil(';'); //('\n');

    int lineStart = line.indexOf("=") + 1; // Szukamy miejsca, gdzie zaczyna wartość zmiennej
    if ((lineStart != -1))                 //&& (currentLine != 0)) // Pomijamy pierwszą linijkę gdzie jest opis pliku
    {
      configValue = line.substring(lineStart); // Wyciągamy URL od "http"
      configValue.trim();                      // Usuwamy białe znaki na początku i końcu
      Serial.print(" Odczytano zmienna konfiguracji numer:" + String(currentLine) + " wartosc:");
      Serial.println(configValue);
      configArray[currentLine] = configValue.toInt();
    }
    currentLine++;
  }
  Serial.print("Zamykamy plik config na wartosci currentLine:");
  Serial.println(currentLine);

  // Odczyt kontrolny
  for (int i = 0; i < 16; i++)
  {
    Serial.print("wartosc: " + String(i) + " z tablicy konfiguracji:");
    Serial.println(configArray[i]);
  }

  configFile.close(); // Zamykamy plik po odczycie kodow pilota
}

// Funkcja testowa-debug, do odczytu PSRAMu, nie uzywana przez inne funkcje
void readPSRAMstations()
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

void webUrlStationPlay()
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

void stationBankListHtmlMobile()
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
  html += "<p style=\"font-size: 0.8rem;\">Web Radio, mobile, Evo: " + String(softwareRev) + "</p>";
  html += "</center></body></html>";
}

void stationBankListHtmlPC()
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
  html += "<p style=\"font-size: 0.8rem;\">Web Radio, desktop, Evo: " + String(softwareRev) + "</p>" + String("\n");
  html += "</center></body></html>";
}

// ####################################################################################### SETUP ####################################################################################### //
void setup()
{
  // Inicjalizuj komunikację szeregową (Serial)
  Serial.begin(115200);
  Serial.println("---------- START of ESP32 Network Radio -----------");

  psramData = (unsigned char *)ps_malloc(PSRAM_lenght * sizeof(unsigned char));

  if (psramInit())
  {
    Serial.println("debug--pamiec PSRAM zainicjowana poprawnie");
    Serial.print("Dostepna pamiec PSRAM:");
    Serial.println(ESP.getPsramSize());
    Serial.print("Wolna pamiec PSRAM:");
    Serial.println(ESP.getFreePsram());
  }
  else
  {
    Serial.println("debug-- BLAD Pamieci PSRAM");
  }

  // AudioBuffer(16384);
  audioBuffer.changeMaxBlockSize(16384);
  wifiManager.setHostname(hostname);

  EEPROM.begin(128);

  // Ustaw pin CS dla karty SD jako wyjście i ustaw go na wysoki stan
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Konfiguruj piny enkodera jako wejścia
  pinMode(CLK_PIN1, INPUT_PULLUP);
  pinMode(DT_PIN1, INPUT_PULLUP);
  pinMode(CLK_PIN2, INPUT_PULLUP);
  pinMode(DT_PIN2, INPUT_PULLUP);
  // Inicjalizacja przycisków enkoderów jako wejścia
  pinMode(SW_PIN1, INPUT_PULLUP);
  pinMode(SW_PIN2, INPUT_PULLUP);

  pinMode(recv_pin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(recv_pin), pulseISR, CHANGE);

  analogReadResolution(12);       // Set ADC resolution to 12 bits (0-4095 range)
  analogSetAttenuation(ADC_11db); // Set the ADC input attenuation (0dB for 0-3.3V range)
  // analogSetSamples(16);
  // adcAttachPin(keyboardPin)
  //  Odczytaj początkowy stan pinu CLK enkodera
  prev_CLK_state1 = digitalRead(CLK_PIN1);
  prev_CLK_state2 = digitalRead(CLK_PIN2);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT); // Konfiguruj pinout dla interfejsu I2S audio
  audio.setVolume(volumeValue);                 // Ustaw głośność na podstawie wartości zmiennej volumeValue w zakresie 0...21

  // Inicjalizuj interfejs SPI wyświetlacza
  SPI.begin(SPI_SCK_OLED, SPI_MISO_OLED, SPI_MOSI_OLED);
  SPI.setFrequency(1000000);

  // Inicjalizacja SPI z nowymi pinami dla czytnika kart SD
  customSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS); // SCLK = 45, MISO = 21, MOSI = 48, CS = 47
  // Inicjalizuj wyświetlacz i odczekaj 250 milisekund na włączenie
  // u8g2.setBusClock(1000000);
  u8g2.begin();
  delay(250);
  // Powitanie na wyswietlaczu:
  u8g2.sendF("ca", 0xC7, displayBrightness); // Ustawiamy jasność ekranu zgodnie ze zmienna displayBrightness

  u8g2.drawXBMP(0, 5, notes_width, notes_height, notes); // obrazek - nutki
  u8g2.setFont(u8g2_font_fub14_tf);
  u8g2.drawStr(58, 17, "Internet Radio");
  u8g2.setFont(spleen6x12PL);
  u8g2.drawStr(226, 62, softwareRev);
  u8g2.sendBuffer();

  // Inicjalizacja karty SD
  if (!SD.begin(SD_CS, customSPI))
  {
    // Informacja na wyswietlaczu o problemach lub braku karty SD
    Serial.println("Błąd inicjalizacji karty SD!");
    // u8g2.clearBuffer();
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(5, 62, "Error - Please check SD card");
    u8g2.setDrawColor(0);
    u8g2.drawBox(212, 0, 44, 45);
    u8g2.setDrawColor(1);
    u8g2.drawXBMP(220, 3, 30, 40, sdcard); // ikona SD karty
    u8g2.sendBuffer();
    //  while(true) {;;} // Zostajemy tutaj az do resetu i ponownego sprawdzenia karty
    //  return;
    noSDcard = true; // Flaga braku karty SD, będziemy użwyać EEPROM
    delay(2000);
  }
  else
  {
    Serial.println("Karta SD zainicjalizowana pomyślnie.");
  }
  Serial.print("Numer seryjny ESP:");
  Serial.println(ESP.getEfuseMac());

  // audioBuffer.changeMaxBlockSize(16384);

  // u8g2.drawStr(5, 32, "Internet Radio");
  // u8g2.sendBuffer();
  u8g2.setFont(spleen6x12PL);
  u8g2.drawStr(5, 62, "Connecting to network...    ");

  u8g2.sendBuffer();
  u8g2.sendF("ca", 0xC7, displayBrightness); // Ustawiamy jasność ekranu zgodnie ze zmienna displayBrightness
  button2.setDebounceTime(50);               // Ustawienie czasu debouncingu dla przycisku enkodera 2

  // Inicjalizacja WiFiManagera
  wifiManager.setConfigPortalBlocking(false);

  readStationFromSD();
  readEqualizerFromSD(); // ODczytujemy ustawienia filtrów equalizera z karty SD
  readVolumeFromSD();    // odczytujemy nastawę głośnosci staertowej

  /*-------------------- RECOVERY MODE --------------------*/
  recoveryModeCheck();

  previous_bank_nr = bank_nr; // wyrównanie wartości przy stacie radia aby nie podmienic bank_nr na wartość 0 po pierwszym upływie czasu menu
  Serial.print("debug1...wartość bank_nr:");
  Serial.println(bank_nr);
  Serial.print("debug1...wartość previous_bank_nr:");
  Serial.println(previous_bank_nr);
  Serial.print("debug1...wartość station_nr:");
  Serial.println(station_nr);

  // Rozpoczęcie konfiguracji Wi-Fi i połączenie z siecią, jeśli konieczne
  if (wifiManager.autoConnect("ESP32-Radio"))
  {
    Serial.println("Połączono z siecią WiFi");
    // u8g2.clearBuffer();
    // u8g2.setFont(DotMatrix13pl);
    // u8g2.setFont(u8g2_font_fub14_tf);
    // u8g2.drawStr(5, 32, "WiFi Connected");
    currentIP = WiFi.localIP().toString(); // konwersja IP na string
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(5, 62, "                                   "); // czyszczenie lini spacjami
    u8g2.sendBuffer();
    u8g2.drawStr(5, 62, "WiFi Connected IP:"); // wyswietlenie IP
    u8g2.drawStr(115, 62, currentIP.c_str());  // wyswietlenie IP
    u8g2.sendBuffer();
    delay(2000); // odczekaj 2 sek przed wymazaniem numeru IP

    u8g2.setFont(spleen6x12PL);
    u8g2.clearBuffer();
    u8g2.drawStr(10, 25, "Time synchronization...");
    u8g2.sendBuffer();

    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2 );
    configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", ntpServer1, ntpServer2);

    // Serial.print("Syncrhonizacja zegara - status:");
    // Serial.println(sntp_get_sync_status());

    // while (syncStatus != SNTP_SYNC_STATUS_COMPLETED)
    // {
    //  syncStatus = sntp_get_sync_status();
    //}

    timer1.attach(1, updateTimerFlag); // Ustaw timer, aby wywoływał funkcję updateTimer co sekundę
    timer2.attach(1, displayDimmerTimer);

    // timer1.attach(1, updateTimer);   // Ustaw timer, aby wywoływał funkcję updateTimer co sekundę
    // timer2.attach(60, getWeatherData);   // Ustaw timer, aby wywoływał funkcję getWeatherData co 60 sekund
    // timer3.attach(10, switchWeatherData);   // Ustaw timer, aby wywoływał funkcję switchWeatherData co 10 sekund

    uint8_t temp_station_nr = station_nr; // Chowamy na chwile odczytaną stacje z karty SD
    fetchStationsFromServer();
    station_nr = temp_station_nr; // Przywracamy numer po odczycie stacji

    changeStation();

    // getWeatherData();

    // ########################################### WEB Server ######################################################
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      String userAgent = request->header("User-Agent");
      
      if (userAgent.indexOf("Mobile") != -1) // Jestesmy na telefonie 
      {
        stationBankListHtmlMobile();
        html = String(index_html) + html;
      } 
      else //Jestemy na komputerze typu desktop
      {
        stationBankListHtmlPC();
        html = String(index_html) + html;  // Składamy cześć stałą html z częscią generowaną dynamicznie
      }
      
      request->send_P(200, "text/html", html.c_str(), processor); });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        //request->send(SD, "/favicon.png", "image/x-icon");
        request->send(SD, "/favicon.ico", "image/x-icon"); });

    server.on("/icon.png", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        //request->send(SD, "/favicon.png", "image/x-icon");
        request->send(SD, "/icon.png", "image/x-icon"); });

    server.on("/volminus.png", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        //request->send(SD, "/favicon.png", "image/x-icon");
        request->send(SD, "/volminus.png", "image/x-icon"); });

    server.on("/volplus.png", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        //request->send(SD, "/favicon.png", "image/x-icon");
        request->send(SD, "/volplus.png", "image/x-icon"); });

    server.on("/page2", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      stationBankListHtmlPC();
      html = String(index_html) + html;

      request->send_P(200, "text/html", html.c_str(), processor); });

    server.on("/volumeUp", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      volumeUp(); 
      request->send_P(200, "text/html", index_html, processor); });

    server.on("/volumeDown", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      volumeDown(); 
      request->send_P(200, "text/html", index_html, processor); });

    server.on("/stationUp", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      station_nr++;
      if (station_nr > stationsCount) {station_nr = 1;}
      changeStation();
      request->send_P(200, "text/html", index_html, processor); });

    server.on("/stationDown", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      station_nr--;
      if (station_nr < 1) {station_nr = stationsCount;}
      changeStation();
      request->send_P(200, "text/html", index_html, processor); });

    // Send a GET request to <ESP_IP>/slider?value=<inputMessage>
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      String inputMessage1;
      String inputMessage2;
      String inputMessage3;
      String inputMessage4;
      // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
      if (request->hasParam(PARAM_INPUT_1)) // Parametr zmiana głośności
      {
        inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
        sliderValue = inputMessage1;
        volumeValue = sliderValue.toInt();
        
        if (volumeValue < 1) 
        { volumeMute = true;}
        else if (volumeValue > 0) 
        {volumeMute = false;}
        audio.setVolume(volumeValue);
        volumeDisplay();
      }
      else if (request->hasParam(PARAM_INPUT_2)) // Parametr zmiana stacji
      {
        inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
        station_nr = inputMessage2.toInt();
        Serial.print("inputMessage2: ");
        Serial.println(inputMessage2);
               
        ir_code = rcCmdOk; // Przypisujemy kod polecenia z pilota
        bit_count = 32; // ustawiamy informacje, ze mamy pelen kod NEC do analizy 
        calcNec();  // przeliczamy kod pilota na kod oryginalny pełen kod NEC    
                       
          //changeStation();
          //displayRadio();
          //u8g2.sendBuffer();
          //clearFlags();   
      }
      else if (request->hasParam(PARAM_INPUT_3)) //Parametr zmiana Banku
      {
        inputMessage3 = request->getParam(PARAM_INPUT_3)->value();
        bank_nr = inputMessage3.toInt();
        station_nr = 1;
        bankMenuEnable = true;        
        
        //bankMenuDisplay();
        fetchStationsFromServer();
        //changeStation();
        clearFlags();

        ir_code = rcCmdOk; // Przypisujemy kod polecenia z pilota
        bit_count = 32; // ustawiamy informacje, ze mamy pelen kod NEC do analizy 
        calcNec();  // przeliczamy kod pilota na kod oryginalny pełen kod NEC    
        station_nr = 1;

      }
      else if (request->hasParam(PARAM_INPUT_4)) // Parametr URL
      {
        inputMessage4 = request->getParam(PARAM_INPUT_4)->value();
        url2play = inputMessage4.c_str();
        urlToPlay = true;
      }              
      else 
      {
        inputMessage1 = "No message sent";
        inputMessage2 = "No message sent";
        inputMessage3 = "No message sent";
        inputMessage4 = "No message sent";
      }
       Serial.println(inputMessage1);
       Serial.println(inputMessage2);
       Serial.println(inputMessage3);
       Serial.println(inputMessage4);
      
      request->send(200, "text/plain", "OK"); });

    server.begin();
    currentSelection = station_nr - 1;       // ustawiamy stacje na liscie na obecnie odtwarzaczną przy starcie radia
    firstVisibleLine = currentSelection + 1; // pierwsza widoczna lina to grająca stacja przy starcie
    if (currentSelection + 1 >= stationsCount - 1)
    {
      firstVisibleLine = currentSelection - 3;
    }

    displayRadio();
    updateTimer();
  }
  else
  {
    Serial.println("Brak połączenia z siecią WiFi"); // W przypadku braku polaczenia wifi - wyslij komunikat na serial
    u8g2.clearBuffer();
    u8g2.setFont(spleen6x12PL);
    u8g2.drawStr(5, 13, "No network connection"); // W przypadku braku polaczenia wifi - wyswietl komunikat na wyswietlaczu OLED
    u8g2.drawStr(5, 26, "Connect to WiFi: ESP Internet Radio");
    u8g2.drawStr(5, 39, "Open web page http://192.168.4.1");
    u8g2.sendBuffer();
    while (true)
    {
      wifiManager.process();
    } // Nieskonczona petla z procesowaniem Wifi aby nie przejsc do ekranu radia gdy nie ma Wifi
  }

  // int kbd = xTaskCreatePinnedToCore(handleKeyboard, "handle_Keyboard", 2000, NULL, 1, NULL, 1);
  // if(kbd) {Serial.println("Task kbd created...");}
  // else {Serial.printf("Couldn't create task %i", kbd);}

  /*
  int aud = xTaskCreatePinnedToCore(
    audioProcessing, // Funkcja zadania
    "handleAudio",  // Nazwa zadania (opcjonalna)
    16384,            // Rozmiar stosu (w bajtach)
    NULL,            // Parametry przekazywane do zadania (opcjonalne)
    2,               // Priorytet zadania (0 to najniższy, im wyższa liczba, tym wyższy priorytet)
    NULL,            // Uchwyt do zadania (opcjonalny)
    1);              // Rdzeń, na którym zadanie ma być uruchomione (0 lub 1)

  if(aud) {Serial.println("Task aud created...");}
  else {Serial.printf("Couldn't create task %i", aud);}
  */
}

// #######################################################################################  LOOP  ####################################################################################### //
void loop()
{
  runTime1 = esp_timer_get_time();
  wifiManager.process(); // WiFi manager
  audio.loop();          // Wykonuje główną pętlę dla obiektu audio (np. odtwarzanie dźwięku, obsługa audio)
  button1.loop();        // Wykonuje pętlę dla obiektu button1 (sprawdza stan przycisku z enkodera 1)
  button2.loop();        // Wykonuje pętlę dla obiektu button2 (sprawdza stan przycisku z enkodera 2)
  handleButtons();       // Wywołuje funkcję obsługującą przyciski i wykonuje odpowiednie akcje (np. zmiana opcji, wejście do menu)
  // webServer();            // Uruchamiamy Web serwer
  vTaskDelay(1); // Krótkie opóźnienie, oddaje czas procesora innym zadaniom

  /* -------------- KLAWIATURA --------------*/
  /* Odczyt stanu klawiatura ADC pod GPIO 9 */

  //  if (millis() - keyboardLastSampleTime >= keyboardSampleDelay) // Sprawdzenie ADC - klawiatury
  //  {
  //    keyboardLastSampleTime = millis();
  //    handleKeyboard();
  //  }

  if (displayActive == true)
  {
    displayDimmer(0);
  }

  // Obsługa enkodera 1
  handleEncoder1();

  // Obsługa enkodera 2
  if (encoderFunctionOrder == 0)
  {
    handleEncoder2VolumeStationsClick();
  }
  else if (encoderFunctionOrder == 1)
  {
    handleEncoder2StationsVolumeClick();
  }

  /*---------------------  FUNKCJA BACK / POWROTU ze wszystkich opcji Menu, Ustawien, itd ---------------------*/

  if (displayActive && (millis() - displayStartTime >= displayTimeout)) // Przywracanie poprzedniej zawartości ekranu po 6 sekundach
  {
    Serial.print("rcInputDigitsMenuEnable: ");
    Serial.println(rcInputDigitsMenuEnable);

    Serial.print("Station nr: ");
    Serial.println(station_nr);

    Serial.print("Bank nr: ");
    Serial.println(bank_nr);

    //-- Nie zmieniamy automatycznie Banku, funkcja wyłaczona --
    // Sprawdzamy czy nie musimy zmienic banku lub stacji
    /*
    if ((rcInputDigitsMenuEnable == true) && (bank_nr != previous_bank_nr))
    {
      station_nr = 1;
      fetchStationsFromServer();
      bankMenuEnable = false;
      changeStation();
    }
    */
    if (volumeBufferValue != volumeValue)
    {
      saveVolumeOnSD();
      volumeBufferValue = volumeValue;
    }

    if ((rcInputDigitsMenuEnable == true) && (station_nr != stationFromBuffer)) // Jezeli nastapiła zmiana numeru stacji to wczytujemy nową stacje
    {
      changeStation();
    }

    displayDimmer(0);
    displayActive = false;
    timeDisplay = true;
    listedStations = false;
    menuEnable = false;
    volumeSet = false;
    bankMenuEnable = false;
    bankNetworkUpdate = false;
    equalizerMenuEnable = false;
    rcInputDigitsMenuEnable = false;
    rcInputDigit1 = 0xFF; // czyscimy cyfre 1, flaga pustej zmiennej to FF
    rcInputDigit2 = 0xFF; // czyscimy cyfre 2, flaga pustej zmiennej to FF
    currentOption = INTERNET_RADIO;

    station_nr = stationFromBuffer;
    bank_nr = previous_bank_nr;

    displayRadio();
    u8g2.sendBuffer();
  }

  /*
    if (updateTimeAtStart = false)  // aktualizujemy zegar i wyswietlacz ale tylko raz przy starcie zanim wejdziemy do funkcji millis odswiezania scrollera
    {
      displayRadio();
      updateTimer();
      updateTimeAtStart = true;
    }
  */

  /*---------------------  PILOT IR - NEC  ---------------------*/

  if (bit_count == 32) // sprawdzamy czy odczytalismy w przerwaniu pełne 32 bity kodu IR NEC
  {
    if (ir_code != 0) // sprawdzamy czy zmienna ir_code nie jest równa 0
    {

      detachInterrupt(recv_pin); // rozpinay przerwanie
      Serial.print("Kod NEC OK:");
      Serial.print(ir_code, HEX);
      ir_code = reverse_bits(ir_code, 32); // rotacja bitów zmiana z LSB-MSB na MSB-LSB
      Serial.print("  MSB-LSB: ");
      Serial.print(ir_code, HEX);

      uint8_t CMD = (ir_code >> 16) & 0xFF; // Drugi bajt (inwersja adresu)
      uint8_t ADDR = ir_code & 0xFF;        // Czwarty bajt (inwersja komendy)

      Serial.print("  ADR:");
      Serial.print(ADDR, HEX);
      Serial.print(" CMD:");
      Serial.println(CMD, HEX);
      ir_code = ADDR << 8 | CMD; // Łączymy ADDR i CMD w jedną zmienną 0xDDRCMD

      Serial.print("debug-- puls 9ms:");
      Serial.print(pulse_duration_9ms);
      Serial.print("  4.5ms:");
      Serial.print(pulse_duration_4_5ms);
      Serial.print("  1690us:");
      Serial.print(pulse_duration_1690us);
      Serial.print("  690us:");
      Serial.println(pulse_duration_560us);

      displayDimmer(0); // jesli odbierzemy kod z pilota to wyłaczamy przyciemnienie wyswietlacza OLED

      if (ir_code == rcCmdVolumeUp)
      {
        volumeUp();
      } // Przycisk głośniej
      else if (ir_code == rcCmdVolumeDown)
      {
        volumeDown();
      }                                    // Przycisk ciszej
      else if (ir_code == rcCmdArrowRight) // strzałka w prawo - nastepna stacja, bank lub nastawy equalizera
      {
        if (bankMenuEnable == true)
        {
          bank_nr++;
          if (bank_nr > 16)
          {
            bank_nr = 1;
          }
          bankMenuDisplay();
        }
        else if (equalizerMenuEnable == true)
        {
          if (toneSelect == 1)
          {
            toneHiValue++;
          }
          if (toneSelect == 2)
          {
            toneMidValue++;
          }
          if (toneSelect == 3)
          {
            toneLowValue++;
          }

          if (toneHiValue > 6)
          {
            toneHiValue = 6;
          }
          if (toneMidValue > 6)
          {
            toneMidValue = 6;
          }
          if (toneLowValue > 6)
          {
            toneLowValue = 6;
          }
          displayEqualizer();
        }
        else
        {
          station_nr++;
          if (station_nr > stationsCount)
          {
            station_nr = stationsCount;
          }
          changeStation();
          displayRadio();
          u8g2.sendBuffer();
        }
      }
      else if (ir_code == rcCmdArrowLeft) // strzałka w lewo - poprzednia stacja, bank lub nastawy equalizera
      {
        if (bankMenuEnable == true)
        {
          bank_nr--;
          if (bank_nr < 1)
          {
            bank_nr = 16;
          }
          bankMenuDisplay();
        }
        else if (equalizerMenuEnable == true)
        {
          if (toneSelect == 1)
          {
            toneHiValue--;
          }
          if (toneSelect == 2)
          {
            toneMidValue--;
          }
          if (toneSelect == 3)
          {
            toneLowValue--;
          }

          if (toneHiValue < -40)
          {
            toneHiValue = -40;
          }
          if (toneMidValue < -40)
          {
            toneMidValue = -40;
          }
          if (toneLowValue < -40)
          {
            toneLowValue = -40;
          }

          displayEqualizer();
        }
        else
        {
          station_nr--;
          if (station_nr < 1)
          {
            station_nr = 1;
          }
          changeStation();
          displayRadio();
          u8g2.sendBuffer();
        }
      }
      else if ((ir_code == rcCmdArrowUp) && (currentOption == INTERNET_RADIO) && (volumeSet == false) && (equalizerMenuEnable == true))
      {
        toneSelect--;
        if (toneSelect < 1)
        {
          toneSelect = 1;
        }
        displayEqualizer();
      }
      else if ((ir_code == rcCmdArrowUp) && (currentOption == INTERNET_RADIO) && (volumeSet == false) && (equalizerMenuEnable == false)) // Przycisk w góre
      {
        timeDisplay = false;
        displayActive = true;
        displayStartTime = millis();
        station_nr = currentSelection + 1;
        station_nr--;
        if (station_nr < 1)
        {
          station_nr = stationsCount;
        } // jesli dojdziemy do początku listy stacji to przewijamy na koniec

        scrollUp();
        displayStations();
      }
      else if ((ir_code == rcCmdArrowDown) && (currentOption == INTERNET_RADIO) && (volumeSet == false) && (equalizerMenuEnable == true))
      {
        toneSelect++;
        if (toneSelect > 3)
        {
          toneSelect = 3;
        }
        displayEqualizer();
      }
      else if ((ir_code == rcCmdArrowDown) && (currentOption == INTERNET_RADIO) && (volumeSet == false) && (equalizerMenuEnable == false)) // Przycisk w dół
      {
        timeDisplay = false;
        displayActive = true;
        displayStartTime = millis();
        station_nr = currentSelection + 1;

        station_nr++;
        if (station_nr > stationsCount)
        {
          station_nr = 1; // stationsCount;
        }

        Serial.println(station_nr);
        scrollDown();
        displayStations();
      }
      else if (ir_code == rcCmdOk)
      {
        if (bankMenuEnable == true)
        {
          station_nr = 1;
          fetchStationsFromServer(); // Ładujemy stacje z karty lub serwera
          bankMenuEnable = false;
        }
        if (equalizerMenuEnable == true)
        {
          saveEqualizerOnSD();
        } // zapis ustawien equalizera
        // if (volumeSet == true) { saveVolumeOnSD();}                 // zapis ustawien głośnosci po nacisnięciu OK, wyłaczony aby można było przełączyć stacje na www bez czekania
        // if ((equalizerMenuEnable == false) && (volumeSet == false)) // jesli nie zapisywaliśmy equlizer i glonosci to wywolujemy ponizsze funkcje
        if ((equalizerMenuEnable == false)) // jesli nie zapisywaliśmy equlizer
        {
          changeStation();
          clearFlags(); // Czyscimy wszystkie flagi przebywania w różnych menu
          displayRadio();
          u8g2.sendBuffer();
        }
        equalizerMenuEnable = false; // Kasujemy flage ustawiania equalizera
        volumeSet = false;           // Kasujemy flage ustawiania głośnosci
      }
      else if (ir_code == rcCmdKey0)
      {
        rcInputKey(0);
      }
      else if (ir_code == rcCmdKey1)
      {
        rcInputKey(1);
      }
      else if (ir_code == rcCmdKey2)
      {
        rcInputKey(2);
      }
      else if (ir_code == rcCmdKey3)
      {
        rcInputKey(3);
      }
      else if (ir_code == rcCmdKey4)
      {
        rcInputKey(4);
      }
      else if (ir_code == rcCmdKey5)
      {
        rcInputKey(5);
      }
      else if (ir_code == rcCmdKey6)
      {
        rcInputKey(6);
      }
      else if (ir_code == rcCmdKey7)
      {
        rcInputKey(7);
      }
      else if (ir_code == rcCmdKey8)
      {
        rcInputKey(8);
      }
      else if (ir_code == rcCmdKey9)
      {
        rcInputKey(9);
      }
      else if (ir_code == rcCmdBack)
      {
        // Zerujemy wszystkie flagi
        clearFlags();
        /*
        displayActive = false;
        timeDisplay = true;
        listedStations = false;
        menuEnable = false;
        volumeSet = false;
        bankMenuEnable = false;
        bankNetworkUpdate = false;
        rcInputDigitsMenuEnable = false;
        equalizerMenuEnable = false;
        rcInputDigit1 = 0xFF; // czyscimy cyfre 1, flaga pustej zmiennej to FF
        rcInputDigit2 = 0xFF; // czyscimy cyfre 2, flaga pustej zmiennej to FF
        currentOption = INTERNET_RADIO;

        station_nr = stationFromBuffer;
        bank_nr = previous_bank_nr;
        */
        // screenRefresh = true;

        displayRadio();
        u8g2.sendBuffer();
      }
      else if (ir_code == rcCmdMute)
      {
        volumeMute = !volumeMute;
        if (volumeMute == true)
        {
          audio.setVolume(0);
        }
        else if (volumeMute == false)
        {
          audio.setVolume(volumeValue);
        }
        displayRadio();
      }
      else if (ir_code == rcCmdDirect) // Przycisk Direct -> Menu Bank - udpate GitHub, Menu Equalizer - reset wartosci, Radio Display - fnkcja przyciemniania ekranu
      {
        if ((bankMenuEnable == true) && (equalizerMenuEnable == false)) // flage można zmienic tylko bedąc w menu wyboru banku
        {
          bankNetworkUpdate = !bankNetworkUpdate; // zmiana flagi czy aktualizujemy bank z sieci czy karty SD
          bankMenuDisplay();
        }
        if ((bankMenuEnable == false) && (equalizerMenuEnable == true))
        {
          toneHiValue = 0;
          toneMidValue = 0;
          toneLowValue = 0;
          displayEqualizer();
        }
        if ((bankMenuEnable == false) && (equalizerMenuEnable == false) && (volumeSet == false))
        {
          displayBrightness = displayBrightness + 15;
          if (displayBrightness > 15)
          {
            displayBrightness = 0;
          }
          u8g2.sendF("ca", 0xC7, displayBrightness);
        }
      }
      else if (ir_code == rcCmdSrc)
      {
        displayMode++;
        if (displayMode > 2)
        {
          displayMode = 0;
        }
        displayRadio();
        u8g2.sendBuffer();
        timeDisplay = true;
        ActionNeedUpdateTime = true;
        listedStations = false;
        displayActive = false;
      }
      else if (ir_code == rcCmdRed)
      {
        u8g2.setPowerSave(1);
        // u8g2.setContrast(128);

        // saveConfig();
        //{vuMeterOn = !vuMeterOn; displayRadio();}
        // u8g2.sendF("ca", 0xb9, 0x07);
        // u8g2.sendF("ca", 0xb6, 0xFF);
        /*
         Serial.print("EEPROM Stacja: ");
         Serial.println(EEPROM.read(0));
         Serial.print("EEPROM Bank: ");
         Serial.println(EEPROM.read(1));
         Serial.print("EEPROM Głośność: ");
         Serial.println(EEPROM.read(2));
         */
        // readEEPROM();
        // displayBasicInfo();
        // debugAudioBuffor = !debugAudioBuffor;
        // displayAutoDimmerOn = !displayAutoDimmerOn;
      }
      else if (ir_code == rcCmdGreen)
      {
        u8g2.setPowerSave(0);
        // u8g2.setContrast(255);
        // readConfig();

        // u8g2.sendF("ca", 0xc1, 0xff);
        // u8g2.sendF("caaaaaaaaaaaaaaa", 0xb8, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4);

        // saveEEPROM();
        // displayConfig();

        // readPSRAMstations();

        // saveConfig();
        // Serial.println("#### ODCZYT ####");

        // displayRadio();
        // u8g2.sendBuffer();
        // vuMeterMode = !vuMeterMode;} // Zmiana trybu VU meter z przerywanych kresek na ciągłe paski

        // readRcStoredCodes(rcPage); // Sprawdzenie komend pilota - funkcja testowa
        // rcPage++;
        // if (rcPage > 2) {rcPage = 0;}
      }

      else if (ir_code == rcCmdBankMinus)
      {
        if (bankMenuEnable == true)
        {
          bank_nr--;
          if (bank_nr < 1)
          {
            bank_nr = 16;
          }
        }

        bankMenuDisplay();
      }
      else if (ir_code == rcCmdBankPlus)
      {
        if (bankMenuEnable == true)
        {
          bank_nr++;
          if (bank_nr > 16)
          {
            bank_nr = 1;
          }
        }
        bankMenuDisplay();
      }

      else if (ir_code == rcCmdAud)
      {
        // debugAudioBuffor = !debugAudioBuffor;
        // toneSelect = 1;
        displayEqualizer();
      }
      else
      {
        Serial.println("Inny przycisk");
      }
    }
    else
    {
      Serial.println("Błąd - kod pilota NEC jest niepoprawny!");
      Serial.print("debug-- puls 9ms:");
      Serial.print(pulse_duration_9ms);
      Serial.print("  4.5ms:");
      Serial.print(pulse_duration_4_5ms);
      Serial.print("  1690us:");
      Serial.print(pulse_duration_1690us);
      Serial.print("  690us:");
      Serial.println(pulse_duration_560us);
    }
    ir_code = 0;
    bit_count = 0;
    Serial.print("debug-- Czas2 - Czas1 = ");
    // runTime2 = runTime2 - runTime1;
    Serial.println(runTime);

    attachInterrupt(digitalPinToInterrupt(recv_pin), pulseISR, CHANGE);
    Serial.print("debug-- Kontrola stosu:");
    Serial.print(uxTaskGetStackHighWaterMark(NULL));
    Serial.println(" DWORD");
  }

  if ((audioShowStreamtitleRefresh == true) || (audioInfoRefresh == true)) // Zmiana streamtitle lub info bitrate, czestotliwość - wymaga odswiezenia displayRadio
  {
    audioShowStreamtitleRefresh = false;
    audioInfoRefresh = false;
    displayRadio(); // Streamtitle, bitrate, wymaga odswiezenia
  }

  /*---------------------  Odswiezanie VU Meter, Time, Scroller, OLED, WiFi ---------------------*/

  if ((millis() - scrollingStationStringTime > scrollingRefresh) && (bankMenuEnable == false) && (menuEnable == false) && (listedStations == false) && (timeDisplay == true))
  {
    scrollingStationStringTime = millis();

    if (screenRefresh == true) // Dodatkowe odswiezanie ekranu aby usunąc "artefakty", pętla 3x 65ms VU metera
    {
      screenRefreshCount++;
      if (screenRefreshCount > screenRefreshCountValue)
      {
        screenRefresh = false;
        screenRefreshCount = 0;
        // displayRadio();
        // u8g2.sendBuffer();
      }
    }
    if (ActionNeedUpdateTime == true) // Aktualizacja zegara co 1 sek. + status audio buffora
    {
      ActionNeedUpdateTime = false;
      updateTimer();

      if (debugAudioBuffor == true)
      {
        bufforAudioInfo();
        drawSignalPower(194, 63, 1); // Narysuj wskaznik zasiegu WiFi X,Y z wydrukiem na terminalu
      }
      else
      {
        if ((displayMode == 0) || (displayMode == 2))
        {
          drawSignalPower(194, 63, 0);
        } // x, y, 0-bez wydruku mocy sygnału na terminalu , 1-z wydrukiem
        if ((displayMode == 1) && (volumeMute == false))
        {
          drawSignalPower(244, 47, 0);
        }
      }
    }

    displayRadioScroller(); // wykonujemy przewijanie tekstu station stringi przygotowujemy bufor ekranu

    if ((volumeMute == true) || (volumeValue == 0)) // Obsługa wyciszenia dzwięku, wprowadzamy napis MUTE na ekran
    {
      u8g2.setDrawColor(0);
      if (displayMode == 0)
      {
        u8g2.drawStr(0, 48, "> MUTED <");
      }
      if (displayMode == 1)
      {
        u8g2.drawStr(200, 47, "> MUTED <");
      }
      u8g2.setDrawColor(1);
    }

    if (vuMeterOn == true && displayActive == false && displayMode == 0 && volumeMute == false) //&& (flac == false) jesli właczone sa wskazniki VU to rysujemy, dla stacji FLAC wyłaczamy aby nie bylo cieci w streamie
    {
      vuMeter();
    }

    if (urlToPlay == true)
    {
      urlToPlay = false;
      webUrlStationPlay();
      displayRadio();
    }

    // if (displayMode == 2) { displayRadio();}

    u8g2.sendBuffer(); // rysujemy zawartosc Scrollera i VU jesli właczone
  }
  runTime2 = esp_timer_get_time();
  runTime = runTime2 - runTime1;
}
