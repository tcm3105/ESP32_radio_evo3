#ifndef CONFIG_H_
#define CONFIG_H_

// definicja pinow czytnika karty SD
#define SD_CS 47   // Pin CS (Chip Select) dla karty SD wybierany jako interfejs SPI
#define SD_SCLK 45 // Pin SCK (Serial Clock) dla karty SD
#define SD_MISO 21 // Pin MISO (Master In Slave Out) dla karty SD
#define SD_MOSI 48 // pin MOSI (Master Out Slave In) dla karty SD

// Definicja pinow dla wyswietlacza OLED
#define SPI_MOSI_OLED 39 // Pin MOSI (Master Out Slave In) dla interfejsu SPI OLED
#define SPI_MISO_OLED 0  // Pin MISO (Master In Slave Out) brak dla wyswietlacza OLED
#define SPI_SCK_OLED 38  // Pin SCK (Serial Clock) dla interfejsu SPI OLED
#define CS_OLED 42       // Pin CS (Chip Select) dla interfejsu OLED
#define DC_OLED 40       // Pin DC (Data/Command) dla interfejsu OLED
#define RESET_OLED 41    // Pin Reset dla interfejsu OLED

// Definicja pinow dla przetwornika PCM5102A
#define I2S_DOUT 13 // Podłączenie do pinu DIN na DAC
#define I2S_BCLK 12 // Podłączenie po pinu BCK na DAC
#define I2S_LRC 14  // Podłączenie do pinu LCK na DAC

#define SCREEN_WIDTH 256 // Szerokość ekranu w pikselach
#define SCREEN_HEIGHT 64 // Wysokość ekranu w pikselach

// Enkoder 1 - uzwyany dla odtwarzacza
#define CLK_PIN1 6 // Podłączenie z pinu 6 do CLK na enkoderze prawym
#define DT_PIN1 5  // Podłączenie z pinu 5 do DT na enkoderze prawym
#define SW_PIN1 4  // Podłączenie z pinu 4 do SW na enkoderze prawym (przycisk)

// Enkoder 2 - uzywany dla radia
#define CLK_PIN2 10 // Podłączenie z pinu 10 do CLK na enkoderze
#define DT_PIN2 11  // Podłączenie z pinu 11 do DT na enkoderze lewym
#define SW_PIN2 1   // Podłączenie z pinu 1 do SW na enkoderze lewym (przycisk)

// IR odbiornik podczerwieni
#define recv_pin 15

#define NTP_SERWER1 "pool.ntp.org"
#define NTP_SERWER2 "time.nist.gov"

// definicja dlugosci ilosci stacji w banku, dlugosci nazwy stacji w PSRAM/EEPROM, maksymalnej ilosci plikow audio (odtwarzacz)
#define MAX_STATIONS 99         // Maksymalna liczba stacji radiowych, które mogą być przechowywane w jednym banku
#define STATION_NAME_LENGTH 200 // Nazwa stacji wraz z bankiem i numerem stacji do wyświetlenia w pierwszej linii na ekranie
#define MAX_FILES 100           // Maksymalna liczba plików lub katalogów w tablicy directoriesz

#define STATIONS_URL1 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank01.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL2 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank02.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL3 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank03.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL4 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank04.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL5 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank05.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL6 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank06.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL7 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank07.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL8 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank08.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL9 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank09.txt"  // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL10 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank10.txt" // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL11 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank11.txt" // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL12 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank12.txt" // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL13 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank13.txt" // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL14 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank14.txt" // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL15 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank15.txt" // Adres URL do pliku z listą stacji radiowych
#define STATIONS_URL16 "https://raw.githubusercontent.com/dzikakuna/ESP32_radio_streams/main/bank16.txt" // Adres URL do pliku z listą stacji radiowych

//================ Definicja portów i pinów dla klaiwatury numerycznej ===========================//

const int keyboardPin = 9; // wejscie klawiatury (ADC)

unsigned long keyboardValue = 0;
unsigned long keyboardLastSampleTime = 0;
unsigned long keyboardSampleDelay = 50;

// ---- Progi przełaczania ADC dla klawiatury matrycowej 5x3 w tunrze Sony ST-120 ---- //
const int keyboardButtonThresholdTolerance = 20; // Tolerancja dla pomiaru ADC
const int keyboardButtonNeutral = 4095;          // Pozycja neutralna
const int keyboardButtonThreshold_0 = 2375;      // Przycisk 0
const int keyboardButtonThreshold_1 = 10;        // Przycisk 1
const int keyboardButtonThreshold_2 = 545;       // Przycisk 2
const int keyboardButtonThreshold_3 = 1390;      // Przycisk 3
const int keyboardButtonThreshold_4 = 1925;      // Przycisk 4
const int keyboardButtonThreshold_5 = 2285;      // Przycisk 5
const int keyboardButtonThreshold_6 = 385;       // Przycisk 6
const int keyboardButtonThreshold_7 = 875;       // Przycisk 7
const int keyboardButtonThreshold_8 = 1585;      // Przycisk 8
const int keyboardButtonThreshold_9 = 2055;      // Przycisk 9
const int keyboardButtonThreshold_Shift = 2455;  // Shift - funkcja Enter/OK
const int keyboardButtonThreshold_Memory = 2170; // Memory - funkcja Bank menu
const int keyboardButtonThreshold_Band = 1640;   // Przycisk Band - funkcja Back
const int keyboardButtonThreshold_Auto = 730;    // Przycisk Auto - przelacza Radio/Zegar
const int keyboardButtonThreshold_Scan = 1760;   // Przycisk Scan - funkcja Dimmer ekranu OLED
const int keyboardButtonThreshold_Mute = 1130;   // Przycisk Mute - funkcja MUTE

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

#endif