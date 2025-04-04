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

// Obecnie nie używana
// Funkcja do wyświetlania informacji o pliku audio
void FilePlayer::audio_info(const char *info)
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

        /*
        if (currentOption == PLAY_FILES)
        {
          displayPlayer();
        }
        if (currentOption == INTERNET_RADIO)
        {
          // displayRadio();
          audioInfoRefresh = true;
        }
        */
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

// Obecnie nie używana
// Funkcja do wyświetlania danych ID3 z pliku audio
void FilePlayer::audio_id3data(const char *info)
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
