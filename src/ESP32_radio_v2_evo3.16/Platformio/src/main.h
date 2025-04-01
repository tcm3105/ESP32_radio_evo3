#ifndef MAIN_H_
#define MAIN_H_

#include "Arduino.h"
#include "config.h"
#include "html.h"
#include "fonts.h"
#include "pictures.h"
#include "tools.h"
#include "keyboard.h"

enum MenuOption : int
{
  PLAY_FILES,     // Odtwarzacz plików
  INTERNET_RADIO, // Radio internetowe
};

MenuOption currentOption = static_cast<MenuOption>(INTERNET_RADIO); // Aktualnie wybrana opcja menu (domyślnie radio internetowe)
const char *ntpServer1 = NTP_SERWER1;  // Adres serwera NTP używany do synchronizacji czasu
const char *ntpServer2 = NTP_SERWER2; // Adres serwera NTP używany do synchronizacji czasu
const long gmtOffset_sec = 3600;          // Przesunięcie czasu UTC w sekundach
const int daylightOffset_sec = 3600;      // Przesunięcie czasu letniego w sekundach, dla Polski to 1 godzina

int currentSelection = 0;            // Numer aktualnego wyboru na ekranie OLED
int firstVisibleLine = 0;            // Numer pierwszej widocznej linii na ekranie OLED
uint8_t station_nr = 0;              // Numer aktualnie wybranej stacji radiowej z listy
int stationFromBuffer = 0;           // Numer stacji radiowej przechowywanej w buforze do przywrocenia na ekran po bezczynności
uint8_t bank_nr;                     // Numer aktualnie wybranego banku stacji z listy
uint8_t previous_bank_nr = 0;        // Numer banku przed wejsciem do menu zmiany banku
int bankFromBuffer = 0;              // Numer aktualnie wybranego banku stacji z listy do przywrócenia na ekran po bezczynności
uint8_t screenRefreshCount = 0;      // odświezeanie ekranu dla stajci bez statoin string
uint8_t screenRefreshCountValue = 6; // Zmiena okreslajaca po ilu petlach nastapia ponowne odświezeanie ekranu OLED dla stajcji

int CLK_state1;                       // Aktualny stan CLK enkodera prawego
int prev_CLK_state1;                  // Poprzedni stan CLK enkodera prawego
int CLK_state2;                       // Aktualny stan CLK enkodera lewego
int prev_CLK_state2;                  // Poprzedni stan CLK enkodera lewego
int stationsCount = 0;                // Aktualna liczba przechowywanych stacji w tablicy
int directoryCount = 0;               // Licznik katalogów
int fileIndex = 0;                    // Numer aktualnie wybranego pliku audio ze wskazanego folderu
int fileFromBuffer = 0;               // Numer aktualnie wybranego pliku do przywrócenia na ekran po bezczynności
int folderIndex = 0;                  // Numer aktualnie wybranego folderu podczas przełączenia do odtwarzania z karty SD
int folderFromBuffer = 0;             // Numer aktualnie wybranego folderu do przywrócenia na ekran po bezczynności
int totalFilesInFolder = 0;           // Zmienna przechowująca łączną liczbę plików w folderze
uint8_t volumeValue = 10;             // Wartość głośności, domyślnie ustawiona na 10
uint8_t volumeBufferValue = 0;        // Wartość głośności, domyślnie ustawiona na 10
int maxVisibleLines = 4;              // Maksymalna liczba widocznych linii na ekranie OLED
int bitrateStringInt = 0;             // Deklaracja zmiennej do konwersji Bitrate string na wartosc Int aby podzelic bitrate przez 1000
int buttonLongPressTime1 = 2000;      // Czas reakcji na długie nacisniecie enkoder 1
int buttonLongPressTime2 = 2000;      // Czas reakcji na długie nacisniecie enkoder 2
int buttonShortPressTime2 = 500;      // Czas rekacjinna krótkie nacisniecie enkodera 2
int buttonSuperLongPressTime2 = 4000; // Czas reakcji na super długie nacisniecie enkoder 2
uint8_t stationNameLenghtCut = 24;    // 24-> 25 znakow, 25-> 26 znaków, zmienna określająca jak długa nazwę na nazwa stacji w plikach Bankow liczone od 0- wartosci ustalonej

// ---- Auto dimmer / auto przyciemnianie wyswietlacza ---- //
uint8_t displayDimmerTimeCounter = 0; // Zmienna inkrementowana w przerwaniu timera2 do przycimniania wyswietlacz
uint8_t dimmerDisplayBrightness = 4;  // Wartość przyciemnienia wyswietlacza po czasie niekatywnosci
uint8_t displayBrightness = 15;       // Domyślna maksymalna janość wyswietlacza
uint16_t displayAutoDimmerTime = 10;  // Czas po jakim nastąpi przyciemninie wyswietlacza, liczony w sekundach
bool displayAutoDimmerOn = false;     // Automatyczne przyciemnianie wyswietlacza, domyślnie włączone

uint8_t displayMode = 0; // Tryb wyswietlacza 0-displayRadio z przewijaniem "scroller" / 1-Zegar / 2- tryb 3 stałych linijek tekstu stacji

// ---- Equalzier ---- //
int8_t toneLowValue = 0;          // Wartosc filtra dla tonow niskich
int8_t toneMidValue = 0;          // Wartosc flitra dla tonow srednich
int8_t toneHiValue = 0;           // Wartość filtra dla tonow wysokich
uint8_t toneSelect = 1;           // Zmienna okreslająca, który filtr equalizera regulujemy
bool equalizerMenuEnable = false; // Flaga wyswietlania menu Equalizera

uint8_t rcInputDigit1 = 0xFF; // Pierwsza cyfra w przy wprowadzaniu numeru stacji z pilota
uint8_t rcInputDigit2 = 0xFF; // Druga cyfra w przy wprowadzaniu numeru stacji z pilota

// ---- Config ---- // - prototype function for config storage
uint8_t configArray[16] = {0};
uint8_t rcPage = 0;

// Flagi do monitorowania stanu klawiatury
unsigned long keyboardValue = 0;
unsigned long keyboardLastSampleTime = 0;
unsigned long keyboardSampleDelay = 50;
bool debugKeyboard = false;         // Wyłącza wywoływanie funkcji i zostawia tylko wydruk pomiaru ADC
bool keyboardButtonPressed = false;

// const int maxVisibleLines = 5;  // Maksymalna liczba widocznych linii na ekranie OLED
bool encoderButton1 = false;      // Flaga określająca, czy przycisk enkodera 1 został wciśnięty
bool encoderButton2 = false;      // Flaga określająca, czy przycisk enkodera 2 został wciśnięty
bool encoderFunctionOrder = true; // Flaga okreslająca kolejność funkcji enkodera 2
bool fileEnd = false;             // Flaga sygnalizująca koniec odtwarzania pliku audio
bool displayActive = false;       // Flaga określająca, czy wyświetlacz jest aktywny
bool isPlaying = false;           // Flaga określająca, czy obecnie trwa odtwarzanie
bool mp3 = false;                 // Flaga określająca, czy aktualny plik audio jest w formacie MP3
bool flac = false;                // Flaga określająca, czy aktualny plik audio jest w formacie FLAC
bool aac = false;                 // Flaga określająca, czy aktualny plik audio jest w formacie AAC
bool vorbis = false;              // Flaga określająca, czy aktualny plik audio jest w formacie VORBIS
bool id3tag = false;              // Flaga określająca, czy plik audio posiada dane ID3
bool timeDisplay = true;          // Flaga określająca kiedy pokazać czas na wyświetlaczu, domyślnie od razu po starcie
bool listedStations = false;      // Flaga określająca czy na ekranie jest pokazana lista stacji do wyboru
bool menuEnable = false;          // Flaga określająca czy na ekranie można wyświetlić menu
bool bankMenuEnable = false;      // Flaga określająca czy na ekranie jest wyświetlone menu wyboru banku
bool bitratePresent = false;      // Flaga określająca, czy na serial terminalu pojawiła się informacja o bitrate - jako ostatnia dana spływajaca z info
bool bankNetworkUpdate = false;   // Flaga wyboru aktualizacji banku z sieci lub karty SD - True aktulizacja z NETu
bool volumeMute = false;          // Flaga okreslająca stan funkcji wyciszczenia - Mute

bool volumeSet = false;            // Flaga wejscia menu regulacji głosnosci na enkoderze 2
bool vuMeterOn = true;             // Flaga właczajaca wskazniki VU
bool vuMeterMode = false;          // tryb rysowania vuMeter
bool action3Taken = false;         // Flaga Akcji 3 - załaczenia VU
bool updateTimeAtStart = false;    // AKtualizacja czasu po pierwszym uruchomieniu
bool ActionNeedUpdateTime = false; // Zmiena okresaljaca dla displayRadio potrzebe odczytu aktulizacji czasu
bool debugAudioBuffor = false;     // Wyswietlanie bufora Audio
bool screenRefresh = false;        // Dodatkowe odswiezanie ekranu, właczone przy pierwszym uruchomieniu
bool audioShowStreamtitleRefresh = false;
bool audioInfoRefresh = false;
bool vuMeterOnFlacStations = true;
bool noSDcard = false; // flaga ustawiana przy braku wykrycia karty SD
// bool noSDcardAlert = false;//true;          // Informacja o braku karty na wyswietlaczu, ustawienie na false powoduje, ze radio nie będzie jej wyswietlać

unsigned long debounceDelay = 300;                                               // Czas trwania debouncingu w milisekundach
unsigned long displayTimeout = 5000;                                             // Czas wyświetlania komunikatu na ekranie w milisekundach
unsigned long displayStartTime = 0;                                              // Czas rozpoczęcia wyświetlania komunikatu
unsigned long seconds = 0;                                                       // Licznik sekund timera
unsigned int PSRAM_lenght = MAX_STATIONS * (STATION_NAME_LENGTH) + MAX_STATIONS; // deklaracjia długości pamięci PSRAM
unsigned long lastCheckTime = 0;                                                 // No stream audio blink
uint8_t stationNameStreamWidth = 0;                                              // Test pełnej nazwy stacji
uint8_t displayPositionX = 0;                                                                   // Globalna zmienna pomocnicza

unsigned long vuMeterTime; // Czas opznienia odswiezania wskaznikow VU w milisekundach
uint8_t vuMeterL;          // Wartosc VU dla L kanału zakres 0-255
uint8_t vuMeterR;          // Wartosc VU dla R kanału zakres 0-255

unsigned long scrollingStationStringTime; // Czas do odswiezania scorllingu
unsigned long scrollingRefresh = 65;      // Czas w ms przewijania tekstu i odswiezania VUmetera
uint16_t stationStringScrollWidth;        // szerokosc Stringu nazwy stacji w funkcji Scrollera
uint16_t xPositionStationString = 0;      // Pozycja początkowa dla przewijania tekstu StationString
uint16_t offset;                          // Zminnna offsetu dla funkcji Scrollera - przewijania streamtitle na ekranie OLED
unsigned char *psramData;                 // zmienna do trzymania danych stacji w pamieci PSRAM

// ---- Serwer Web ---- //
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;
bool bankChange = false;
bool urlToPlay = false;

// ---- Sprawdzenie funkcji pilota, zminnne do pomiaru róznicy czasów ---- //
unsigned long runTime = 0;
unsigned long runTime1 = 0;
unsigned long runTime2 = 0;

String stationStringScroll = "";    // Zmienna przechowująca tekst do przewijania na ekranie
String directories[MAX_FILES];      // Tablica z indeksami i ścieżkami katalogów
String currentDirectory = "/music"; // Ścieżka bieżącego katalogu
String stationName;                 // Nazwa aktualnie wybranej stacji radiowej
String stationString;               // Dodatkowe dane stacji radiowej (jeśli istnieją)
String bitrateString;               // Zmienna przechowująca informację o bitrate
String sampleRateString;            // Zmienna przechowująca informację o sample rate
String bitsPerSampleString;         // Zmienna przechowująca informację o liczbie bitów na próbkę
String artistString;                // Zmienna przechowująca informację o wykonawcy
String titleString;                 // Zmienna przechowująca informację o tytule utworu
String fileNameString;              // Zmienna przechowująca informację o nazwie pliku
String folderNameString;            // Zmienna przechowująca informację o nazwie folderu
String PlayedFolderName;            // Nazwa aktualnie odtwarzanego folderu
String currentIP;
String stationNameStream; // Nazwa stacji wyciągnieta z danych wysylanych przez stream

String header; // Zmienna dla serwera www
String sliderValue = "0";
String html = "";
String url2play = "";

/*---------- Definicja portu i deklaracje zmiennych do obsługi odbiornika IR ----------*/

// Zmienne obsługi  pilota IR
bool pulse_ready = false;                // Flaga sygnału gotowości
unsigned long pulse_start_high = 0;      // Czas początkowy impulsu
unsigned long pulse_end_high = 0;        // Czas końcowy impulsu
unsigned long pulse_duration = 0;        // Czas trwania impulsu
unsigned long pulse_duration_9ms = 0;    // Tylko do analizy - Czas trwania impulsu
unsigned long pulse_duration_4_5ms = 0;  // Tylko do analizy - Czas trwania impulsu
unsigned long pulse_duration_560us = 0;  // Tylko do analizy - Czas trwania impulsu
unsigned long pulse_duration_1690us = 0; // Tylko do analizy - Czas trwania impulsu

bool pulse_ready9ms = false;          // Flaga sygnału gotowości puls 9ms
bool pulse_ready_low = false;         // Flaga sygnału gotowości
unsigned long pulse_start_low = 0;    // Czas początkowy impulsu
unsigned long pulse_end_low = 0;      // Czas końcowy impulsu
unsigned long pulse_duration_low = 0; // Czas trwania impulsu

unsigned long ir_code = 0; // Zmienna do przechowywania kodu IR
int bit_count = 0;         // Licznik bitów w odebranym kodzie

// Progi przełączeń dla sygnałów czasowych pilota IR
const int LEAD_HIGH = 9050;      // 9 ms sygnał wysoki (początkowy)
const int LEAD_LOW = 4500;       // 4,5 ms sygnał niski (początkowy)
const int TOLERANCE = 150;       // Tolerancja (w mikrosekundach)
const int HIGH_THRESHOLD = 1690; // Sygnał "1"
const int LOW_THRESHOLD = 600;   // Sygnał "0"

bool data_start_detected = false; // Flaga dla sygnału wstępnego
bool rcInputDigitsMenuEnable = false;

const char *PARAM_INPUT_1 = "volume";
const char *PARAM_INPUT_2 = "station";
const char *PARAM_INPUT_3 = "bank";
const char *PARAM_INPUT_4 = "url";

char stations[MAX_STATIONS][STATION_NAME_LENGTH + 1]; // Tablica przechowująca linki do stacji radiowych (jedna na stację) +1 dla terminatora null

#endif