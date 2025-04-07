#ifndef CONFIG_H_
#define CONFIG_H_

#include <SD.h>
#include <EEPROM.h>
#include <U8g2lib.h> // Biblioteka do obsługi wyświetlaczy
#include "fonts.h"
#include <HTTPClient.h>  // Biblioteka do wykonywania żądań HTTP, umożliwia komunikację z serwerami przez protokół HTTP

// Definicja pinow dla wyswietlacza OLED
#define SCREEN_WIDTH 256 // Szerokość ekranu w pikselach
#define SCREEN_HEIGHT 64 // Wysokość ekranu w pikselach

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

// definicja pinow czytnika karty SD
#define SD_CS 47   // Pin CS (Chip Select) dla karty SD wybierany jako interfejs SPI
#define SD_SCLK 45 // Pin SCK (Serial Clock) dla karty SD
#define SD_MISO 21 // Pin MISO (Master In Slave Out) dla karty SD
#define SD_MOSI 48 // pin MOSI (Master Out Slave In) dla karty SD - rgb LED

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
#define MAX_FILES 100           // Maksymalna liczba plików lub katalogów 

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

// -------------------- Koniec konfiguracji ------------------- //

extern U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2;

// StreamPlayer
extern unsigned long displayStartTime;
extern bool equalizerMenuEnable;
extern bool timeDisplay;
extern bool displayActive;
extern const uint8_t spleen6x12PL[2954] U8G2_FONT_SECTION("spleen6x12PL");
extern bool displayAutoDimmerOn;
extern uint16_t displayAutoDimmerTime;
extern uint8_t displayMode;
extern String stationStringScroll;
extern uint16_t stationStringScrollWidth; 
extern String PlayedFolderName;
extern int fileFromBuffer;
extern int totalFilesInFolder;

extern int directoryCount;
extern String directories[MAX_FILES]; 
extern String fileNameString; 
extern String artistString;   
extern String titleString; 

extern int stationsCount;
extern bool mp3;
extern bool flac;
extern bool aac;
extern bool vorbis;
extern bool id3tag;
extern bool bitratePresent;  
extern String bitrateString;
extern int bitrateStringInt;
extern String sampleRateString;
extern String bitsPerSampleString; 

extern String stationString;
extern uint8_t bank_nr;
extern String stationName;

extern unsigned char *psramData;
extern uint8_t displayPositionX;

extern File myFile;
extern uint8_t station_nr;
extern bool noSDcard;

extern uint8_t volumeValue;             // Wartość głośności, domyślnie ustawiona na 10
extern uint8_t volumeBufferValue; 
extern int8_t toneLowValue;          // Wartosc filtra dla tonow niskich
extern int8_t toneMidValue;          // Wartosc flitra dla tonow srednich
extern int8_t toneHiValue;        // Wartosc filtra dla tonow wysokich

// StreamPlayer
extern bool volumeSet;
extern bool bankMenuEnable;
extern bool bankNetworkUpdate;
extern bool listedStations;
extern int maxVisibleLines;
extern int bankFromBuffer;

extern bool bankChange;
extern int currentSelection;
extern int firstVisibleLine;
extern uint8_t previous_bank_nr;
extern String currentDirectory;

// Ir
extern bool rcInputDigitsMenuEnable;
extern uint8_t rcInputDigit1;
extern uint8_t rcInputDigit2;
extern int stationFromBuffer;
extern unsigned long ir_code;

// Html
extern String html;
extern String url2play;
extern String stationNameStream;
extern uint8_t stationNameLenghtCut;
extern String softwareRev;    // Wersja oprogramowania radia
extern String hostname;

// Tools
extern bool encoderFunctionOrder;
extern uint8_t displayBrightness;
extern uint8_t dimmerDisplayBrightness;
extern uint8_t displayDimmerTimeCounter;
extern uint8_t vuMeterL;
extern uint8_t vuMeterR;  
extern bool volumeMute;
extern bool vuMeterMode;

class Config
{
public:
    void saveConfig();
    void readConfig();
    void displayConfig(); // Add displayConfig method declaration
    void readSDStations();
    void saveStationToPSRAM(const char *station);
    void sanitizeAndSaveStation(const char *station);
    void saveStationOnSD();
    void readStationFromSD();
    void readEqualizerFromSD();
    void saveEqualizerOnSD();
    void readVolumeFromSD();
    void saveVolumeOnSD();
    void fetchStationsFromServer();

private:
    void drawSwitch(uint8_t x, uint8_t y, bool state);
    void readPSRAMstations();
};

#endif