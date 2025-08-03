#ifndef SD_READ_WRITE_H
#define SD_READ_WRITE_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

// Custom pin definitions (fixed by your board)
#define SD_MMC_CMD 38
#define SD_MMC_CLK 39
#define SD_MMC_D0 40

bool initSDMMC();
bool isSDCardInserted();
void checkAndUpdateFromSD();
void saveFirmwareVersion();
String getSavedFirmwareVersion();
void compareFirmwareVersions();

#endif
