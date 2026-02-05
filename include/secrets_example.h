#pragma once

// Másold ezt a fájlt secrets.h néven, és töltsd ki az értékeket.
// A secrets.h nincs verziókezelve, így a valódi SSID/jelszó nem kerül fel GitHubra.

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

// Opcionális egyszerű PIN a webes vezérlő végpontokhoz.
// Ha üresen hagyod, nem lesz ellenőrzés.
#ifndef CONTROL_PIN
#define CONTROL_PIN ""
#endif

// MQTT settings (optional)
#ifndef MQTT_HOST
#define MQTT_HOST "homeassistant.local"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef MQTT_USER
#define MQTT_USER ""
#endif

#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif

#ifndef MQTT_BASE_TOPIC
#define MQTT_BASE_TOPIC "thermostat"
#endif

#ifndef MQTT_DISCOVERY_PREFIX
#define MQTT_DISCOVERY_PREFIX "homeassistant"
#endif

#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "Thermostat"
#endif
