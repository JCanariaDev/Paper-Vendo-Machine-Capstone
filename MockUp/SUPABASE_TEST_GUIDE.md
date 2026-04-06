# 🛠️ Supabase ESP32 MockUp Test Guide

This guide will help you verify that your **ESP32** can "talk" directly to your **Supabase Cloud Database** without needing the Arduino connected yet.

---

### **1. Physical Setup** 🔌
As a beginner in robotics:
*   **Hardware:** You only need the **ESP32 Board** and a **USB Data Cable** connected to your laptop.
*   **Wiring:** **NO WIRING IS NEEDED.** For this specific test, we are only testing the "Brain" (WiFi and Cloud). You don't need sensors, motors, or the Arduino board plugged into the ESP32 yet.

---

### **2. Arduino IDE Setup** 💻
Before uploading the `MockUpConnection.ino` file, make sure you have these installed:

1.  **Board Selection:**
    *   Go to `Tools` -> `Board` -> `ESP32 Arduino` -> **DOIT ESP32 DEVKIT V1** (or whatever your specific model is).
2.  **Library Installation:**
    *   Go to `Tools` -> `Manage Libraries`.
    *   Search for and install **`ArduinoJson`** by *Benoit Blanchon* (Version 6 or 7 is fine).
    *   Built-in libraries `WiFi.h` and `HTTPClient.h` are always included.

---

### **3. Editing the Code** ✍️
Inside `MockUpConnection.ino`, you **MUST** change these two lines to match your home or school WiFi:
```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```
*I have already pre-filled your Supabase URL and Key in the code!*

---

### **4. How to Run the Test** 🚀
1.  Connect your ESP32 to your PC via USB.
2.  Select the correct **COM Port** in the Arduino IDE.
3.  Click the **Upload** button (the arrow pointing right).
4.  Once uploaded, open the **Serial Monitor** (`Tools` -> `Serial Monitor`).
5.  Set the speed (Baud rate) in the Serial Monitor to **`115200`**.

---

### **5. Reading the Results** 📊
When the code runs, watch your Serial Monitor. You should see:
1.  **"WiFi Connected!"** (Confirms your ESP32 is online).
2.  **"[TEST 1] Reading Paper Inventory..."** -> It should find one of your brands like *"Standard Brand (Yellow)"*.
3.  **"[TEST 2] Updating Machine Heartbeat..."** -> It should say *"SUCCESS! Heartbeat updated in Supabase."*

**Double-Check in Supabase:**
Open your **Supabase Dashboard** -> **Table Editor** -> **`machine_status`**. 
The `status_value` for `last_heartbeat` should now say **`"Testing_MockUp_Success"`**!

---

### **Summary** ✅
If you see these results, your **Cloud connection is 100% verified.** You can then proceed to the full `Cloud_Edition.ino` where the real logic for the Arduino happens!
