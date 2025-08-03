# ESP32_ThingsboardSDK

## 📌 SD Card-Based OTA Update Logic

In the `setup()` function, the firmware follows this process:

1. **Check for `firmware.bin` on SD card**  
   The function `checkAndUpdateFromSD()` searches for a firmware update file named `firmware.bin`.

2. **If found:**
   - Perform OTA firmware update using the contents of the file.
   - After a successful update, delete `firmware.bin` from the SD card.

3. **Reboot:**
   - Upon reboot, `checkAndUpdateFromSD()` runs again.
   - This time, it does **not** find `firmware.bin`, so the device proceeds with normal operation (e.g., connecting to Wi-Fi, sending telemetry, subscribing to RPC, etc.).

---

## 🧭 Flowchart

