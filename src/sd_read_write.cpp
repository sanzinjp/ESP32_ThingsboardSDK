#include "sd_read_write.h"
#include <Update.h>
#include <variable.h>

const char *firmwarePath = "/firmware.bin";

bool initSDMMC() {
  return SD_MMC.begin("/sdcard", true); // true = 1-bit mode
}

bool isSDCardInserted() { return SD_MMC.cardType() != CARD_NONE; }

void saveFirmwareVersion() {
  preferences.begin("fw-info", false); // false = read-write
  preferences.putString("fw_ver", CURRENT_FIRMWARE_VERSION);
  preferences.end();
}

String getSavedFirmwareVersion() {
  preferences.begin("fw-info", true); // true = read-only
  String savedVersion = preferences.getString("fw_ver", "unknown");
  preferences.end();
  return savedVersion;
}

void compareFirmwareVersions() {
  String saved = getSavedFirmwareVersion();
  if (saved != CURRENT_FIRMWARE_VERSION) {
    Serial.println("New firmware version detected.");
    saveFirmwareVersion(); // update saved version
  } else {
    Serial.println("Firmware version unchanged.");
  }
}

void checkAndUpdateFromSD() {

  if (!initSDMMC()) {
    Serial.println("SD_MMC mount failed. Skipping OTA.");
    return;
  }
  if (!isSDCardInserted()) {
    Serial.println("No SD_MMC card found.");
    return;
  }

  File updateBin = SD_MMC.open(firmwarePath);
  if (!updateBin) {
    Serial.println("Already updated or no firmware.bin found.");
    return;
  }

  if (updateBin.isDirectory()) {
    Serial.println("Error: firmware.bin is a directory!");
    updateBin.close();
    return;
  }

  size_t updateSize = updateBin.size();
  if (updateSize == 0) {
    Serial.println("firmware.bin is empty!");
    updateBin.close();
    return;
  }

  Serial.println("Starting OTA update from firmware.bin...");

  if (!Update.begin(updateSize)) {
    Serial.println("Not enough space to begin OTA");
    updateBin.close();
    return;
  }

  size_t written = Update.writeStream(updateBin);
  if (written == updateSize) {
    Serial.printf("Written %d bytes successfully\n", (int)written);
  } else {
    Serial.printf("Written only %d/%d bytes\n", (int)written, (int)updateSize);
  }

  if (Update.end()) {
    if (Update.isFinished()) {
      Serial.println("OTA done! Rebooting...");
      SD_MMC.remove(firmwarePath); // Optional: prevent re-update
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("OTA not finished.");
    }
  } else {
    Serial.printf("OTA error #%d\n", Update.getError());
  }

  updateBin.close();
}