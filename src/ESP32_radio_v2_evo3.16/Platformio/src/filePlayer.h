#ifndef FILEPLAYER_H_
#define FILEPLAYER_H_

#include <Arduino.h>
#include "FS.h"          // Biblioteka do obsługi systemu plików

class FilePlayer
{
public:
    void printDirectoriesAndSavePaths(File dir, int numTabs, String currentPath);
    bool isAudioFile(const char *filename);
    void displayFolders();
};

#endif