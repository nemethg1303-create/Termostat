#include <Arduino.h>
#include <Wire.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <EEPROM.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// ================= WIFI =================
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "REPLACE_ME"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "REPLACE_ME"
#endif
// Optional simple control PIN for web endpoints (set in secrets.h). Empty string disables checking.
#ifndef CONTROL_PIN
#define CONTROL_PIN ""
#endif
const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASS;

// ================= VERSION =================
const char* FW_VERSION = "1.0.0";

// ================= OLED =================
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// (Diagnostics/calibration removed)

// ================= SENSORS =================
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
#define BMP_ADDR 0x77

// ================= IO =================
#define RELAY_PIN D5
#define BTN_UP    D6
#define BTN_DOWN  D7
#define BTN_MODE  D0
// Relay electrical behavior: true if HIGH energizes the relay, false if LOW energizes
const bool RELAY_ACTIVE_HIGH = false;

// ================= EEPROM =================
#define EEPROM_SIZE 32
#define EEPROM_MAGIC 0x42

// ================= TERMOSZTÁT =================
float setTemp = 22.0;
float hysteresis = 0.4;

// fagyvédelem
const float FROST_ON  = 5.0;
const float FROST_OFF = 6.0;

// kijelző timeout (ms) — 5 perc
const uint32_t DISPLAY_TIMEOUT = 300000;

// ================= MODE =================
enum Mode : uint8_t {
  MODE_OFF,
  MODE_AUTO,
  MODE_ON
};

Mode mode = MODE_AUTO;

// ================= STATE =================
#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer server(80);
#elif defined(ARDUINO_ARCH_ESP32)
WebServer server(80);
#endif
bool relayState = false;
uint32_t lastButtonTime = 0;
bool displayOn = true;
// EEPROM save control (wear reduction)
float lastSavedSetTemp = NAN;
Mode lastSavedMode = MODE_AUTO;
bool pendingSave = false;
uint32_t lastChangeMs = 0;
const uint32_t SAVE_DEBOUNCE_MS = 2000; // coalesce rapid changes

// Forward declarations for functions defined later
void setupWeb();
void saveState();
void loadState();
void handleButtons();
void controlHeating();
void updateDisplayPower();
void drawDisplay();
float getTemperature();
void setRelay(bool on);
void scanI2C();
// diagnostics removed
void maybeSaveState();

// =================================================

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("[Thermostat] Booting..."));
  Serial.print(F("[Thermostat] FW ")); Serial.println(FW_VERSION);
  Wire.begin(D2, D1);
  Serial.println(F("[Thermostat] I2C scan starting (expect OLED at 0x3C)"));
  scanI2C();

  pinMode(RELAY_PIN, OUTPUT);
  // Ensure relay starts OFF respecting polarity
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(RELAY_PIN, LOW);
  } else {
    digitalWrite(RELAY_PIN, HIGH);
  }

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);

  EEPROM.begin(EEPROM_SIZE);
  loadState();
  bool dispOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.setTextColor(SSD1306_WHITE);
  display.setRotation(0);
  display.setTextWrap(false);
  // Log detected display geometry
  Serial.print(F("[Thermostat] OLED size: "));
  Serial.print(display.width());
  Serial.print('x');
  Serial.println(display.height());
  Serial.println(dispOK ? F("[Thermostat] OLED OK") : F("[Thermostat] OLED FAIL"));
  // Simple splash tailored for 128x32: title smaller, then Booting... and version
  display.clearDisplay();
  const int16_t W = display.width();
  // Title (classic font size 2), centered at top
  {
    display.setFont();
    display.setTextSize(2);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds("Thermostat", 0, 0, &x1, &y1, &w, &h);
    int16_t cx = (W - (int16_t)w) / 2 - x1;
    display.setCursor(cx, 0);
    display.print(F("Thermostat"));
  }
  // Booting... line (classic font size 1), centered
  {
    display.setFont();
    display.setTextSize(1);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds("Booting...", 0, 0, &x1, &y1, &w, &h);
    int16_t cx = (W - (int16_t)w) / 2 - x1;
    display.setCursor(cx, 16);
    display.print(F("Booting..."));
  }
  // Version line (classic font size 1), centered at bottom band
  {
    display.setFont();
    display.setTextSize(1);
    String v = String("FW v") + String(FW_VERSION);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(v, 0, 0, &x1, &y1, &w, &h);
    int16_t cx = (W - (int16_t)w) / 2 - x1;
    display.setCursor(cx, 24);
    display.print(v);
  }
  display.display();
  bool ahtOK = aht.begin();
  bool bmpOK = bmp.begin(BMP_ADDR);
  Serial.println(ahtOK ? F("[Thermostat] AHTX0 OK") : F("[Thermostat] AHTX0 FAIL"));
  Serial.println(bmpOK ? F("[Thermostat] BMP280 OK") : F("[Thermostat] BMP280 FAIL"));
  Serial.print(F("[Thermostat] Connecting WiFi: ")); Serial.print(ssid);
  WiFi.begin(ssid, pass);
  uint32_t wifiStart = millis();
  const uint32_t WIFI_TIMEOUT_MS = 30000; // 30s timeout
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT_MS) { Serial.print('.'); delay(300); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[Thermostat] WiFi connected, IP: ")); Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("[Thermostat] WiFi connect timeout, continuing offline"));
  }

  setupWeb();
  Serial.println(F("[Thermostat] Setup done"));
}
 
void loop() {
  server.handleClient();
  handleButtons();
  controlHeating();
  updateDisplayPower();
  maybeSaveState();
  if (displayOn) drawDisplay();
  // Periodic heartbeat to serial for monitoring
  static uint32_t lastLog = 0;
  if (millis() - lastLog > 3000) {
    float t = getTemperature();
    Serial.print(F("[Thermostat]"));
    Serial.print(F(" temp=")); Serial.print(isnan(t) ? F("NAN") : String(t,1));
    Serial.print(F(" set=")); Serial.print(setTemp,1);
    Serial.print(F(" mode=")); Serial.print(mode == MODE_OFF ? F("OFF") : (mode == MODE_AUTO ? F("AUTO") : F("ON")));
    Serial.print(F(" relay=")); Serial.println(relayState ? F("ON") : F("OFF"));
    lastLog = millis();
  }
  // Small cooperative delay to keep WDT happy
  delay(10);
}

// =================================================
// ================= TEMPERATURE ===================
// =================================================
float getTemperature() {
  static float lastGood = NAN;
  static uint32_t lastGoodMs = 0;
  static float filt = NAN;
  const float alpha = 0.2f; // EMA factor

  sensors_event_t h, t;
  bool ok = aht.getEvent(&h, &t);
  float bmpT = bmp.readTemperature();

  float raw = NAN;
  if (!ok && isnan(bmpT)) {
    raw = NAN;
  } else if (!ok) {
    raw = bmpT;
  } else if (isnan(bmpT)) {
    raw = t.temperature;
  } else {
    raw = (t.temperature + bmpT) / 2.0f;
  }

  if (!isnan(raw)) {
    lastGood = raw;
    lastGoodMs = millis();
    if (isnan(filt)) filt = raw;
    else filt = alpha * raw + (1.0f - alpha) * filt;
    return filt;
  }

  // If sensors failed, use last good value for up to 10s
  if (!isnan(lastGood) && millis() - lastGoodMs < 10000) {
    if (isnan(filt)) return lastGood;
    return filt;
  }
  return NAN;
}

// =================================================
// ================= RELAY LOGIC ===================
void setRelay(bool on) {
  relayState = on;
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  } else {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  }
}

void controlHeating() {
  float temp = getTemperature();

  if (mode == MODE_ON) {
    setRelay(true);
    return;
  }

  if (mode == MODE_OFF) {
    if (isnan(temp)) {
      setRelay(true);
      return;
    }
    if (temp < FROST_ON)  setRelay(true);
    if (temp > FROST_OFF) setRelay(false);
    return;
  }

  // MODE_AUTO
  if (isnan(temp)) {
    setRelay(true);
    return;
  }

  if (temp < setTemp - hysteresis) setRelay(true);
  if (temp > setTemp + hysteresis) setRelay(false);
}

// =================================================
// ================= BUTTONS =======================
// =================================================

void wakeDisplay() {
  if (!displayOn) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    displayOn = true;
  }
  lastButtonTime = millis();
}

void handleButtons() {
  // Edge-based debounce to avoid repeated triggers and floating pins
  static uint32_t lastSample = 0;
  if (millis() - lastSample < 20) return; // sample every 20ms
  lastSample = millis();

  static bool upPrev = true;   // pull-up -> idle HIGH
  static bool downPrev = true; // pull-up -> idle HIGH
  static bool modePrev = true; // pull-up -> idle HIGH
  static uint32_t nextUpRepeat = 0;
  static uint32_t nextDownRepeat = 0;
  const uint32_t REPEAT_MS = 300; // hold repeat interval

  bool upCur = digitalRead(BTN_UP) == LOW;
  bool downCur = digitalRead(BTN_DOWN) == LOW;
  bool modeCur = digitalRead(BTN_MODE) == LOW;

  bool changed = false;

  // On press edges for up/down (LOW transition)
  if (upCur && !upPrev) { setTemp += 0.5; changed = true; nextUpRepeat = millis() + REPEAT_MS; }
  if (downCur && !downPrev) { setTemp -= 0.5; changed = true; nextDownRepeat = millis() + REPEAT_MS; }

  // Hold-to-repeat: while pressed, step every REPEAT_MS
  if (upCur) {
    if (nextUpRepeat && millis() >= nextUpRepeat) {
      setTemp += 0.5; changed = true; nextUpRepeat += REPEAT_MS;
    }
  } else {
    nextUpRepeat = 0; // reset when released
  }
  if (downCur) {
    if (nextDownRepeat && millis() >= nextDownRepeat) {
      setTemp -= 0.5; changed = true; nextDownRepeat += REPEAT_MS;
    }
  } else {
    nextDownRepeat = 0; // reset when released
  }

  // Toggle mode on press edge (LOW transition)
  if (modeCur && !modePrev) { mode = (Mode)((mode + 1) % 3); changed = true; }

  upPrev = upCur;
  downPrev = downCur;
  modePrev = modeCur;

  if (changed) {
    wakeDisplay();
    // Defer EEPROM writes to reduce wear
    pendingSave = true;
    lastChangeMs = millis();
  }
}

// =================================================
// ================= DISPLAY =======================
// =================================================

void updateDisplayPower() {
  if (displayOn && millis() - lastButtonTime > DISPLAY_TIMEOUT) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    displayOn = false;
  }
}

void drawDisplay() {
  display.clearDisplay();

  float temp = getTemperature();

  // Use runtime geometry to adapt to 128x64 vs 128x32
  const int16_t W = display.width();
  const int16_t H = display.height();

  // Layout: left column = big temperature, right column = 3 status lines
  const int16_t MARGIN = 2;
  const int16_t GAP = 2;
  const uint8_t infoSize = 1;            // 128x32: klasszikus font méret
  const int16_t lineH = 8 * infoSize;    // klasszikus font sor magasság

  // Oszlopszélességek: nagyobb bal oldal, kompakt jobb oldal
  const int16_t rightW = 44;             // ~44px elég 3 rövid sorhoz
  const int16_t leftW  = W - rightW - GAP - MARGIN;
  const int16_t leftX = MARGIN;
  const int16_t rightX = W - MARGIN - rightW + 10; // jobbra tolás +10px

  // Prepare temperature string
  char buf[8];
  if (isnan(temp)) snprintf(buf, sizeof(buf), "--.-");
  else snprintf(buf, sizeof(buf), "%.1f", temp);

  // Choose the largest font that fits inside left column
  const GFXfont *fonts[3] = { &FreeSansBold24pt7b, &FreeSansBold18pt7b, &FreeSansBold12pt7b };
  int16_t x1 = 0, y1 = 0; uint16_t w = 0, h = 0;
  int16_t tempBaseline = -1;
  int16_t tempX = leftX;
  for (uint8_t i = 0; i < 3; i++) {
    display.setFont(fonts[i]);
    display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    bool fitsWidth = ((int16_t)w <= (leftW - 2 * MARGIN));
    bool fitsHeight = ((int16_t)h <= (H - 2 * MARGIN));
    if (fitsWidth && fitsHeight) {
      // Center horizontally within left column
      tempX = leftX + (leftW - (int16_t)w) / 2 - x1;
      // Center vertically in total height
      tempBaseline = (H / 2) - (y1 + (int16_t)h) / 2;
      // Shift down by 10px as requested
      tempBaseline = min(H - 2, tempBaseline + 10);
      break;
    }
  }
  if (tempBaseline < 0) {
    display.setFont(&FreeSansBold12pt7b);
    display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tempX = leftX + max(0, (leftW - (int16_t)w) / 2 - x1);
    tempBaseline = (H / 2) - (y1 + (int16_t)h) / 2;
    tempBaseline = min(H - 2, tempBaseline + 10);
  }

  // Draw left temperature
  display.setCursor(tempX, tempBaseline);
  display.print(buf);

  // Draw right column: MODE, relay status, setpoint (3 lines)
  display.setFont();
  display.setTextSize(infoSize);
  // Sorok, felfelé emelve 4px, hogy ne lógjon ki az alja
  const int16_t y1b = (MARGIN + lineH) - 4;
  const int16_t y2b = y1b + lineH;
  const int16_t y3b = y2b + lineH;

  // Mode
  display.setCursor(rightX, y1b);
  display.print(mode == MODE_OFF ? "OFF" : (mode == MODE_AUTO ? "AUTO" : "ON"));

  // Relay status
  display.setCursor(rightX, y2b);
  {
    display.print(relayState ? "H:ON" : "H:OFF");
  }

  // Setpoint
  display.setCursor(rightX, y3b);
  display.print("C:");
  display.print(setTemp, 1);

  display.display();
}

// =================================================
// ================= DIAG ==========================
// =================================================

// diagnostics removed

// diagnostics removed

// =================================================
// ================= EEPROM ========================
// =================================================

void saveState() {
  EEPROM.write(0, EEPROM_MAGIC);
  EEPROM.put(1, setTemp);
  EEPROM.write(5, mode);
  EEPROM.commit();
  lastSavedSetTemp = setTemp;
  lastSavedMode = mode;
  pendingSave = false;
}

void loadState() {
  if (EEPROM.read(0) == EEPROM_MAGIC) {
    EEPROM.get(1, setTemp);
    mode = (Mode)EEPROM.read(5);
  }
  lastSavedSetTemp = setTemp;
  lastSavedMode = mode;
}
// Periodic save ticker
void maybeSaveState() {
  if (!pendingSave) return;
  if (millis() - lastChangeMs < SAVE_DEBOUNCE_MS) return;
  if (lastSavedMode != mode || lastSavedSetTemp != setTemp) {
    saveState();
  } else {
    pendingSave = false;
  }
}

// =================================================
// ================= WEB ===========================
// =================================================

void setupWeb() {
  server.on("/", []() {
    float temp = getTemperature();
    bool heatOn = relayState;
    String modeStr = String(mode == MODE_OFF ? "OFF" : (mode == MODE_AUTO ? "AUTO" : "ON"));
    String relayStr = String(heatOn ? "ON" : "OFF");
    String relayColor = String(heatOn ? "#2ecc71" : "#e74c3c"); // green/red

    String html =
      "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>"
      "body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:20px;}"
      "h1{margin:0 0 16px 0;font-size:28px;}"
      ".grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));}"
      ".card{background:#1c1c1c;border-radius:10px;padding:16px;box-shadow:0 2px 6px rgba(0,0,0,.3);}"
      ".label{font-size:14px;opacity:.8;margin-bottom:6px;}"
      ".value{font-size:42px;font-weight:700;line-height:1.1;}"
      ".btn{display:inline-block;padding:12px 16px;margin:6px 8px 0 0;font-size:20px;border-radius:8px;text-decoration:none;color:#fff;background:#2d6cdf;}"
      ".relay{font-size:24px;font-weight:700;}"
      ".footer{margin-top:16px;font-size:12px;opacity:.6;}"
      "</style></head><body>";

    html += "<h1>Termosztat</h1>";
    html += "<div class='grid'>";

    // Temperature card
    html += "<div class='card'><div class='label'>Hőmérséklet</div><div class='value'>";
    html += String(isnan(temp) ? "--.-" : String(temp, 1));
    html += " °C</div></div>";

    // Setpoint card with buttons
    html += "<div class='card'><div class='label'>Célhőmérséklet</div><div class='value'>" + String(setTemp,1) + " °C</div>";
    html += "<div><a class='btn' href='/down'>Le -0.5</a><a class='btn' href='/up'>Fel +0.5</a></div></div>";

    // Mode card with toggle button
    html += "<div class='card'><div class='label'>Mód</div><div class='value'>" + modeStr + "</div>";
    html += "<div><a class='btn' href='/mode'>Mód váltás</a></div></div>";

    // Relay state card with color
    html += "<div class='card'><div class='label'>Fűtés relé</div><div class='relay' style='color:" + relayColor + "'>Relay: " + relayStr + "</div></div>";

    html += "</div><div class='footer'>FW v" + String(FW_VERSION) + "</div></body></html>";
    server.send(200, "text/html", html);
  });

  // Optional PIN check for control endpoints
  auto checkPin = []() -> bool {
    String pinArg = server.hasArg("pin") ? server.arg("pin") : String("");
    String required = String(CONTROL_PIN);
    if (required.length() == 0) return true; // no pin required
    return pinArg == required;
  };

  server.on("/up", [checkPin](){ if (!checkPin()) { server.send(403, "text/plain", "Forbidden"); return; } setTemp += 0.5; pendingSave = true; lastChangeMs = millis(); server.sendHeader("Location","/"); server.send(303); });
  server.on("/down", [checkPin](){ if (!checkPin()) { server.send(403, "text/plain", "Forbidden"); return; } setTemp -= 0.5; pendingSave = true; lastChangeMs = millis(); server.sendHeader("Location","/"); server.send(303); });
  server.on("/mode", [checkPin](){ if (!checkPin()) { server.send(403, "text/plain", "Forbidden"); return; } mode = (Mode)((mode + 1) % 3); pendingSave = true; lastChangeMs = millis(); server.sendHeader("Location","/"); server.send(303); });

  server.begin();
}

// =================================================
// ================= I2C SCAN ======================
// =================================================

void scanI2C() {
  byte count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("[I2C] Found device at 0x"));
      Serial.println(addr, HEX);
      count++;
      delay(2);
    }
  }
  Serial.print(F("[I2C] Total devices: "));
  Serial.println(count);
}

