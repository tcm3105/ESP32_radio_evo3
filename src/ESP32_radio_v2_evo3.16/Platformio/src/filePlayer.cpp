#include "filePlayer.h"
#include "config.h"

// Obsługa wyświetlacza dla odtwarzanego pliku z karty SD
void FilePlayer::displayPlayer()
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

// Funkcja do wyświetlania folderów na ekranie OLED z uwzględnieniem zaznaczenia
void FilePlayer::displayFolders()
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
      displayPositionX = i; // todo ? "=="
      Serial.println("SystemVOLUME");
    }
  }
  // Przywróć domyślne ustawienia koloru rysowania (biały tekst na czarnym tle)
  u8g2.setDrawColor(1); // Biały kolor rysowania
  u8g2.sendBuffer();
}

// Funkcja do przeszukiwania katalogów i zapisywania ich ścieżek
void FilePlayer::printDirectoriesAndSavePaths(File dir, int numTabs, String currentPath)
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

// Funkcja sprawdza, czy plik jest plikiem audio na podstawie jego rozszerzenia
bool FilePlayer::isAudioFile(const char *filename)
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

// Funkcja do wylistowania katalogów z karty
void FilePlayer::listDirectories(const char *dirname)
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
}