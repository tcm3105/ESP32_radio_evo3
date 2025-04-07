// ###############################################################################################
// ESP32 Radio Evo3 - Interent Radio Player
// Support OLED SSD1322 dipslay and PCB5102A DAC
// ###############################################################################################
// Xsoft@Xsoft.eu 2025 - PlatformIO code refactoring based on ESP32_radio_v2_evo3.16
// Robgold 2025
// Source -> https://github.com/dzikakuna/ESP32_radio_evo3/tree/main/src/ESP32_radio_v2_evo3.16
// Based on project https://github.com/sarunia/ESP32_radio_player_v2

// ###############################################################################################

// Pliki mp3 na karcie w katalogu : /music

#include "Arduino.h" // Standardowy nagłówek Arduino, który dostarcza podstawowe funkcje i definicje
#include "main.h"

// deklaracja wersji oprogramowania i nazwy hosta widocznego w routerach
String softwareRev = "v4.16";    // Wersja oprogramowania radia
String hostname = "ESP32-Radio"; // Definicja nazwy hosta widoczna na zewnątrz

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R2, /* cs=*/CS_OLED, /* dc=*/DC_OLED, /* reset=*/RESET_OLED); // Hardware SPI 3.12inch OLED
// Uncomment only one of the following lines if you need to use a different display type
// U8G2_SSD1363_256X128_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/CS_OLED, /* dc=*/CS_OLED, /* reset=*/RESET_OLED);  // Hardware SPI 3.12inch OLED
// U8G2_SH1122_256X64_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/CS_OLED, /* dc=*/DC_OLED, /* reset=*/RESET_OLED);    // Hardware SPI 2.08inch OLED

// domyślny adres http://192.168.4.1
AsyncWebServer server(80);

// Inicjalizacja WiFiManagera
WiFiManager wifiManager;

// Obiekt do obsługi połączenia WiFi dla klienta HTTP
WiFiClient client;

// Konfiguracja nowego SPI z wybranymi pinami dla czytnika kart SD
SPIClass customSPI = SPIClass(HSPI); // Używamy HSPI, ale z własnymi pinami

File myFile; // Uchwyt pliku

ezButton button1(SW_PIN1); // Utworzenie obiektu przycisku z enkodera 1 ezButton, podłączonego do pinu 4
ezButton button2(SW_PIN2); // Utworzenie obiektu przycisku z enkodera 1 ezButton, podłączonego do pinu 1
Audio audio;               // Obiekt do obsługi funkcji związanych z dźwiękiem i audio
AudioBuffer audioBuffer;

Config configClass;
Tools toolsClass;
FilePlayer filePlayerClass;
StreamPlayer streamPlayerClass;
Ir irClass;
Html htmlClass;

Ticker timer1; // Timer do updateTimerFlag co 1s
Ticker timer2; // Timer do displayDimmerTimer co 60s

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

void displayMenu()
{
  timeDisplay = false;
  menuEnable = true;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_spleen8x16_mr);
  u8g2.drawStr(120, 20, "MENU");
  u8g2.setDrawColor(1); // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji

  switch (currentOption)
  {
  case PLAY_FILES:
    u8g2.drawBox(0, 27, SCREEN_WIDTH, 15); // Narysuj prostokąt jako tło dla zaznaczonej stacji (x=0, szerokość 256, wysokość 10)
    u8g2.setDrawColor(0);         // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji
    u8g2.drawStr(0, 40, ">          MUSIC PLAYER        <");
    u8g2.setDrawColor(1);
    u8g2.drawStr(0, 60, "            Net radio           ");
    break;
  case INTERNET_RADIO:
    u8g2.drawStr(0, 40, "           Music player         ");
    u8g2.drawBox(0, 47, SCREEN_WIDTH, 15); // Narysuj prostokąt jako tło dla zaznaczonej stacji (x=0, szerokość 256, wysokość 10)
    u8g2.setDrawColor(0);         // Zmień kolor rysowania na czarny dla tekstu zaznaczonej stacji
    u8g2.drawStr(0, 60, ">           NET RADIO          <");
    u8g2.setDrawColor(1);
    break;
  }
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

  static unsigned long lastPressTime = 0;  // Zmienna do kontrolowania debouncingu (ostatni czas naciśnięcia)
  const unsigned long debounceDelay = 100; // Opóźnienie debouncingu

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
        toolsClass.encoderFunctionOrderChange();
        action3Taken = true;
      }

      // Jeśli przycisk jest wciśnięty przez co najmniej 3 sekundy i akcja jeszcze nie była wykonana
      if (millis() - buttonPressTime2 >= buttonLongPressTime2 && !action2Taken && millis() - buttonPressTime2 < buttonSuperLongPressTime2)
      {
        Serial.println("debug--Bank Menu");
        streamPlayerClass.bankMenuDisplay();

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

// Obsługa kółka enkodera 1 podczas dzialania odtwarzacza plików
void handleEncoder1RotationPlayer()
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
      if (volumeValue < 0)
      {
        volumeValue = 0;
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
void handleEncoder2RotationPlayer()
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
      filePlayerClass.displayFolders();
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
      filePlayerClass.displayFolders();
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
    filePlayerClass.displayPlayer();
    displayActive = false;
    timeDisplay = true;
  }
}

// Funkcja do odtwarzania plików z wybranego folderu <------------------<<<<
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
    Serial.println("Błąd otwarcia katalogu!");
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
    if (filePlayerClass.isAudioFile(fileName.c_str()))
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
    if (!filePlayerClass.isAudioFile(fileName.c_str()))
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

      // todo check this
      //  Jeśli skończył się plik, przejdź do następnego
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

      handleEncoder1RotationPlayer(); // Obsługa kółka enkodera nr 1
      handleEncoder2RotationPlayer(); // Obsługa kółka enkodera nr 2
      backDisplayPlayer();            // Obsługa bezczynności, przywrócenie wyświetlania danych audio
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

//=============== Recovery Mode========================

void handlePreOtaUpdateCallback()
{
  Update.onProgress([](unsigned int progress, unsigned int total)
                    {
    u8g2.setCursor(1,56); u8g2.printf("Update: %u%%\r", (progress / (total / 100)) );
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
          configClass.saveStationOnSD();
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
          u8g2.drawStr(1, 14, "WEB PORTAL STARTED          ");
          u8g2.drawStr(1, 28, "                            "); // clear line
          String connect = "Connect to WiFi " + hostname;
          u8g2.drawStr(1, 28, connect.c_str());
          u8g2.drawStr(1, 42, "Open http://192.168.4.1     ");
          u8g2.sendBuffer();
          wifiManager.startConfigPortal(hostname.c_str());
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

void displayDimmerTimer()
{
  displayDimmerTimeCounter++;
  if (displayActive == true)
  {
    displayDimmerTimeCounter = 0;
    toolsClass.displayDimmer(0);
  }
  if (displayDimmerTimeCounter >= displayAutoDimmerTime)
  {
    toolsClass.displayDimmer(1); // wywolujemy funkcje przyciemnienia z parametrem 1 (załacz)
    displayDimmerTimeCounter = 0;
  }
}

// Funkcja kasuje wszystkie flagi przebywania w menu, funkcjach itd. Pozwala pwrócic do wyswietlania ekranu głownego
void clearFlags()
{
  toolsClass.displayDimmer(0);
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
  currentOption = static_cast<MenuOption>(INTERNET_RADIO);

  station_nr = stationFromBuffer;
  bank_nr = previous_bank_nr;
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
      streamPlayerClass.displayStations();
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
      streamPlayerClass.bankMenuDisplay();
    }
  }
  prev_CLK_state2 = CLK_state2;

  if ((currentOption == INTERNET_RADIO) && (button2.isReleased()) && (listedStations == true))
  {
    listedStations = false;
    volumeSet = false;
    streamPlayerClass.changeStation();
    streamPlayerClass.displayRadio();
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
    currentOption = static_cast<MenuOption>(INTERNET_RADIO);

    configClass.fetchStationsFromServer();
    streamPlayerClass.changeStation();
    u8g2.clearBuffer();
    streamPlayerClass.displayRadio();

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
      streamPlayerClass.displayStations();
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
      streamPlayerClass.bankMenuDisplay();
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
      streamPlayerClass.changeStation();
      streamPlayerClass.displayRadio();
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
      streamPlayerClass.displayStations();
      // Serial.println("debug--------------------------------------------------> button 2 PRESSED station list");
    }

    else if ((listedStations == false) && (bankMenuEnable == true) && (volumeSet == false))
    {
      displayStartTime = millis();
      timeDisplay = false;
      displayActive = true;

      previous_bank_nr = bank_nr;
      station_nr = 1;
      currentOption = static_cast<MenuOption>(INTERNET_RADIO);

      listedStations = false;
      volumeSet = false;
      bankMenuEnable = false;
      bankNetworkUpdate = false;

      configClass.fetchStationsFromServer();
      streamPlayerClass.changeStation();
      u8g2.clearBuffer();
      streamPlayerClass.displayRadio();
      // u8g2.sendBuffer();
    }
  }
}

// volumeUp / volumeDown / PLAY_FILES / INTERNET_RADIO
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
          currentOption = static_cast<MenuOption>(PLAY_FILES);
        }
        else
        {
          currentOption = static_cast<MenuOption>(INTERNET_RADIO);
        }
        break;

      case INTERNET_RADIO:
        if (DT_state1 == HIGH)
        {
          currentOption = static_cast<MenuOption>(PLAY_FILES);
        }
        else
        {
          currentOption = static_cast<MenuOption>(INTERNET_RADIO);
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
    filePlayerClass.listDirectories(currentDirectory.c_str());
    scrollDown();
    filePlayerClass.displayFolders();
    audio.stopSong();
    playFromSelectedFolder();
  }

  if ((currentOption == INTERNET_RADIO) && (button1.isPressed()) && (menuEnable == true))
  {
    menuEnable = false;
    streamPlayerClass.changeStation();
  }

  if ((button1.isPressed()) && (bankMenuEnable == true))
  {
    bankMenuEnable = false;
    volumeSet = false;
    bankNetworkUpdate = true;
    currentSelection = 0;
    firstVisibleLine = 0;
    station_nr = 1;
    currentOption = static_cast<MenuOption>(INTERNET_RADIO);

    configClass.fetchStationsFromServer();
    streamPlayerClass.changeStation();
    bankNetworkUpdate = false;
    u8g2.clearBuffer();
    streamPlayerClass.displayRadio();
  }
}

//=========================== begin audio =========================

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
      filePlayerClass.displayPlayer();
    }
    if (currentOption == INTERNET_RADIO)
    {
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
  Serial.print("streamtitle ");
  Serial.println(info);
  stationString = String(info);
  if (currentOption == INTERNET_RADIO)
  {

    ActionNeedUpdateTime = true;
    if ((volumeSet == false) && (bankMenuEnable == false) && (listedStations == false) && (rcInputDigitsMenuEnable == false) && (equalizerMenuEnable == false))
    {
      audioShowStreamtitleRefresh = true;
    }
  }
}

//=========================== Web server ==========================

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

void webServerExecute()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      String userAgent = request->header("User-Agent");
      
      if (userAgent.indexOf("Mobile") != -1) // Jestesmy na telefonie 
      {
        htmlClass.stationBankListHtmlMobile();
        html = String(index_html) + html;
      } 
      else //Jestemy na komputerze
      {
        htmlClass.stationBankListHtmlPC();
        html = String(index_html) + html;  // Składamy cześć stałą html z częscią generowaną dynamicznie
      }
      
      request->send_P(200, "text/html", html.c_str(), processor); });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SD, "/favicon.ico", "image/x-icon"); });

  server.on("/icon.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SD, "/icon.png", "image/x-icon"); });

  server.on("/volminus.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SD, "/volminus.png", "image/x-icon"); });

  server.on("/volplus.png", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SD, "/volplus.png", "image/x-icon"); });

  server.on("/page2", HTTP_GET, [](AsyncWebServerRequest *request)
            {
                htmlClass.stationBankListHtmlPC();
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
      streamPlayerClass.changeStation();
      request->send_P(200, "text/html", index_html, processor); });

  server.on("/stationDown", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      station_nr--;
      if (station_nr < 1) {station_nr = stationsCount;}
      streamPlayerClass.changeStation();
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
        irClass.calcNec();  // przeliczamy kod pilota na kod oryginalny pełen kod NEC    
      }
      else if (request->hasParam(PARAM_INPUT_3)) //Parametr zmiana Banku
      {
        inputMessage3 = request->getParam(PARAM_INPUT_3)->value();
        bank_nr = inputMessage3.toInt();
        station_nr = 1;
        bankMenuEnable = true;        
        
        configClass.fetchStationsFromServer();
        clearFlags();

        ir_code = rcCmdOk; // Przypisujemy kod polecenia z pilota
        bit_count = 32; // ustawiamy informacje, ze mamy pelen kod NEC do analizy 
        irClass.calcNec();  // przeliczamy kod pilota na kod oryginalny pełen kod NEC    
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
}

//=============================== ir ==============================

void irExecute()
{
  if (bit_count == 32) // sprawdzamy czy odczytalismy w przerwaniu pełne 32 bity kodu IR NEC
  {
    if (ir_code != 0) // sprawdzamy czy zmienna ir_code nie jest równa 0
    {

      detachInterrupt(recv_pin); // rozpinay przerwanie
      Serial.print("Kod NEC OK:");
      Serial.print(ir_code, HEX);
      ir_code = toolsClass.reverse_bits(ir_code, 32); // rotacja bitów zmiana z LSB-MSB na MSB-LSB
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

      toolsClass.displayDimmer(0); // jesli odbierzemy kod z pilota to wyłaczamy przyciemnienie wyswietlacza OLED

      if (ir_code == rcCmdVolumeUp)
      {
        volumeUp();
      } // Przycisk głośniej
      else if (ir_code == rcCmdVolumeDown)
      {
        volumeDown();
      } // Przycisk ciszej
      else if (ir_code == rcCmdArrowRight) // strzałka w prawo - nastepna stacja, bank lub nastawy equalizera
      {
        if (bankMenuEnable == true)
        {
          bank_nr++;
          if (bank_nr > 16)
          {
            bank_nr = 1;
          }
          streamPlayerClass.bankMenuDisplay();
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
          streamPlayerClass.changeStation();
          streamPlayerClass.displayRadio();
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
          streamPlayerClass.bankMenuDisplay();
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
          streamPlayerClass.changeStation();
          streamPlayerClass.displayRadio();
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
        streamPlayerClass.displayStations();
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
        streamPlayerClass.displayStations();
      }
      else if (ir_code == rcCmdOk)
      {
        if (bankMenuEnable == true)
        {
          station_nr = 1;
          configClass.fetchStationsFromServer(); // Ładujemy stacje z karty lub serwera
          bankMenuEnable = false;
        }
        if (equalizerMenuEnable == true)
        {
          configClass.saveEqualizerOnSD();
        } // zapis ustawien equalizera
        if ((equalizerMenuEnable == false)) // jesli nie zapisywaliśmy equlizer
        {
          streamPlayerClass.changeStation();
          clearFlags(); // Czyscimy wszystkie flagi przebywania w różnych menu
          streamPlayerClass.displayRadio();
          u8g2.sendBuffer();
        }
        equalizerMenuEnable = false; // Kasujemy flage ustawiania equalizera
        volumeSet = false;           // Kasujemy flage ustawiania głośnosci
      }
      else if (ir_code == rcCmdKey0)
      {
        irClass.rcInputKey(0);
      }
      else if (ir_code == rcCmdKey1)
      {
        irClass.rcInputKey(1);
      }
      else if (ir_code == rcCmdKey2)
      {
        irClass.rcInputKey(2);
      }
      else if (ir_code == rcCmdKey3)
      {
        irClass.rcInputKey(3);
      }
      else if (ir_code == rcCmdKey4)
      {
        irClass.rcInputKey(4);
      }
      else if (ir_code == rcCmdKey5)
      {
        irClass.rcInputKey(5);
      }
      else if (ir_code == rcCmdKey6)
      {
        irClass.rcInputKey(6);
      }
      else if (ir_code == rcCmdKey7)
      {
        irClass.rcInputKey(7);
      }
      else if (ir_code == rcCmdKey8)
      {
        irClass.rcInputKey(8);
      }
      else if (ir_code == rcCmdKey9)
      {
        irClass.rcInputKey(9);
      }
      else if (ir_code == rcCmdBack)
      {
        clearFlags();
        streamPlayerClass.displayRadio();
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
        streamPlayerClass.displayRadio();
      }
      else if (ir_code == rcCmdDirect) // Przycisk Direct -> Menu Bank - udpate GitHub, Menu Equalizer - reset wartosci, Radio Display - fnkcja przyciemniania ekranu
      {
        if ((bankMenuEnable == true) && (equalizerMenuEnable == false)) // flage można zmienic tylko bedąc w menu wyboru banku
        {
          bankNetworkUpdate = !bankNetworkUpdate; // zmiana flagi czy aktualizujemy bank z sieci czy karty SD
          streamPlayerClass.bankMenuDisplay();
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
        streamPlayerClass.displayRadio();
        u8g2.sendBuffer();
        timeDisplay = true;
        ActionNeedUpdateTime = true;
        listedStations = false;
        displayActive = false;
      }
      else if (ir_code == rcCmdRed)
      {
        u8g2.setPowerSave(1);
      }
      else if (ir_code == rcCmdGreen)
      {
        u8g2.setPowerSave(0);
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

        streamPlayerClass.bankMenuDisplay();
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
        streamPlayerClass.bankMenuDisplay();
      }

      else if (ir_code == rcCmdAud)
      {
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
  u8g2.begin();
  delay(250);
  // Powitanie na wyswietlaczu:
  u8g2.sendF("ca", 0xC7, displayBrightness); // Ustawiamy jasność ekranu zgodnie ze zmienna displayBrightness

  u8g2.drawXBMP(0, 5, notes_width, notes_height, notes); // obrazek - nutki
  u8g2.setFont(u8g2_font_fub14_tf);
  u8g2.drawStr(58, 17, "Internet Radio");
  u8g2.setFont(spleen6x12PL);
  u8g2.drawStr(226, 62, softwareRev.c_str());
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

  u8g2.setFont(spleen6x12PL);
  u8g2.drawStr(5, 62, "Connecting to network...    ");

  u8g2.sendBuffer();
  u8g2.sendF("ca", 0xC7, displayBrightness); // Ustawiamy jasność ekranu zgodnie ze zmienna displayBrightness
  button2.setDebounceTime(50);               // Ustawienie czasu debouncingu dla przycisku enkodera 2

  // Inicjalizacja WiFiManagera
  wifiManager.setConfigPortalBlocking(false);

  configClass.readStationFromSD();
  configClass.readEqualizerFromSD();                      // ODczytujemy ustawienia filtrów equalizera z karty SD
  audio.setTone(toneLowValue, toneMidValue, toneHiValue); // Ustawiamy filtry - zakres regulacji -40 + 6dB jako int8_t ze znakiem

  configClass.readVolumeFromSD(); // odczytujemy nastawę głośnosci staertowej
  audio.setVolume(volumeValue);   // zakres 0...21

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
  if (wifiManager.autoConnect(hostname.c_str()))
  {
    Serial.println("Połączono z siecią WiFi");
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

    timer1.attach(1, updateTimerFlag);    // Ustaw timer, aby wywoływał funkcję updateTimer co sekundę
    timer2.attach(1, displayDimmerTimer); // Ustaw timer, aby wywoływał funkcję displayDimmerTimer co sekundę

    uint8_t temp_station_nr = station_nr; // Chowamy na chwile odczytaną stacje z karty SD
    configClass.fetchStationsFromServer();
    station_nr = temp_station_nr; // Przywracamy numer po odczycie stacji

    streamPlayerClass.changeStation();

    webServerExecute();

    currentSelection = station_nr - 1;       // ustawiamy stacje na liscie na obecnie odtwarzaczną przy starcie radia
    firstVisibleLine = currentSelection + 1; // pierwsza widoczna lina to grająca stacja przy starcie
    if (currentSelection + 1 >= stationsCount - 1)
    {
      firstVisibleLine = currentSelection - 3;
    }

    streamPlayerClass.displayRadio();
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

  vTaskDelay(1); // Krótkie opóźnienie, oddaje czas procesora innym zadaniom

  if (displayActive == true)
  {
    toolsClass.displayDimmer(0);
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

    if (volumeBufferValue != volumeValue)
    {
      configClass.saveVolumeOnSD();
      volumeBufferValue = volumeValue;
    }

    if ((rcInputDigitsMenuEnable == true) && (station_nr != stationFromBuffer)) // Jezeli nastapiła zmiana numeru stacji to wczytujemy nową stacje
    {
      streamPlayerClass.changeStation();
    }

    toolsClass.displayDimmer(0);
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
    currentOption = static_cast<MenuOption>(INTERNET_RADIO);

    station_nr = stationFromBuffer;
    bank_nr = previous_bank_nr;

    streamPlayerClass.displayRadio();
    u8g2.sendBuffer();
  }

  /*---------------------  PILOT IR - NEC  ---------------------*/
  irExecute();

  //---------------------  Zmiana stream title lub audio info - wymaga odswiezenia ---------------------*/
  if ((audioShowStreamtitleRefresh == true) || (audioInfoRefresh == true))
  {
    audioShowStreamtitleRefresh = false;
    audioInfoRefresh = false;
    streamPlayerClass.displayRadio(); // Streamtitle, bitrate, wymaga odswiezenia
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
      }
    }
    if (ActionNeedUpdateTime == true) // Aktualizacja zegara co 1 sek. + status audio buffora
    {
      ActionNeedUpdateTime = false;
      updateTimer();

      if (debugAudioBuffor == true)
      {
        Serial.print("debug--Bufor Audio pojemność / zapełniony:");
        Serial.print(audio.inBufferSize());
        Serial.print(" / ");
        Serial.println(audio.inBufferFilled());
        toolsClass.drawSignalPower(194, 63, 1); // Narysuj wskaznik zasiegu WiFi X,Y z wydrukiem na terminalu
      }
      else
      {
        if ((displayMode == 0) || (displayMode == 2))
        {
          toolsClass.drawSignalPower(194, 63, 0);
        } // x, y, 0-bez wydruku mocy sygnału na terminalu , 1-z wydrukiem
        if ((displayMode == 1) && (volumeMute == false))
        {
          toolsClass.drawSignalPower(244, 47, 0);
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
      toolsClass.vuMeter();
    }

    if (urlToPlay == true)
    {
      urlToPlay = false;
      htmlClass.webUrlStationPlay();
      streamPlayerClass.displayRadio();
    }

    u8g2.sendBuffer(); // rysujemy zawartosc Scrollera i VU jesli właczone
  }

  runTime2 = esp_timer_get_time();
  runTime = runTime2 - runTime1;
}
