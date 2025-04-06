#include "filePlayer.h"
#include "config.h"

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
