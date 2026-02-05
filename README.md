# Termosztat – bekötési és használati útmutató

ESP8266 (Wemos D1 mini) alapú termosztát AHTX0 + BMP280 szenzorokkal, SSD1306 I2C OLED kijelzővel (128×32), webes felülettel és relévezérléssel.

**Verziók**
- FW v2.0.0 — MQTT + Home Assistant autodiscovery, állapot és parancs topicok, discovery újraküldés gomb a weben.
- FW v1.1.1 — dokumentáció frissítés, relé minimális bekapcsolási idő konfigurálása 1 percre a telepített `include/secrets.h`-ban (alapértelmezés továbbra is 5 perc).
- FW v1.1.0 — WiFi timeout, EMA szűrés, opcionális PIN a webes vezérléshez, 300 ms gomb-ismétlés, kisebb loop késleltetés, minimális relé bekapcsolási idő (alapértelmezés: 5 perc, állítható).
- FW v1.0.0 — stabil alapfunkciók és kijelzőelrendezés.

## Áttekintés
- MCU: ESP8266 Wemos D1 mini
- Kijelző: SSD1306 I2C, 128×32, cím: 0x3C
- Szenzorok: Adafruit AHTX0 (I2C), BMP280 (I2C)
- Gombok: 3 darab nyomógomb (Fel/Le/MODE)
- Relé: tranzisztoros/optós relémodul (javasolt), IN vezérléssel

## Lábkiosztás (ESP8266 / Wemos D1 mini)
- I2C SDA: D2 (GPIO4)
- I2C SCL: D1 (GPIO5)
- Relé IN: D5 (GPIO14)
- Gombok (javaslat):
  - BTN_UP: D6 (GPIO12), gomb másik lába GND
  - BTN_DOWN: D7 (GPIO13), gomb másik lába GND
  - BTN_MODE: D0 (GPIO16) – ajánlott külső felhúzó (10k → 3V3), és NE nyomd boot közben

Megjegyzés a bootolatlan lábakról:
- Kerüld gombként: D3 (GPIO0), D4 (GPIO2), D8 (GPIO15) – ezek boot strap lábak, nyomógombbal bootproblémát okozhatnak.

## Bekötési lépések
1. Kijelző (SSD1306 I2C, 128×32):
   - VCC → 3V3, GND → GND, SDA → D2, SCL → D1
   - Cím: jellemzően 0x3C. Ha 0x3D, módosítsd az `OLED_ADDR` értékét a kódban.
2. Szenzorok (AHTX0, BMP280):
   - I2C buszon: VCC → 3V3, GND → GND, SDA → D2, SCL → D1
3. Gombok:
   - Egyik láb GND, másik a PIN (D6, D7, D0)
   - `INPUT_PULLUP`-ot használunk; lenyomáskor LOW lesz.
   - Hosszú vezeték esetén tehetsz 100 nF kondit a gomb két lába közé zajszűréshez.
4. Relé:
   - Használj relémodult (opto + tranzisztor), vagy tranzisztoros meghajtást (NPN + dióda a tekercs fölött).
   - Modul VCC → táp (5V-os modulnál 5V, 3.3V-osnál 3V3), GND → GND, IN → D5.
   - Sok relémodul aktív LOW. A kód alapból aktív LOW modullal kompatibilis (`RELAY_ACTIVE_HIGH = false`).

## Szoftver környezet
- PlatformIO
- Konfiguráció: Arduino_projects/Termostat/platformio.ini
  - ESP8266 környezet: `[env:d1_mini]`
  - Könyvtárak: Adafruit AHTX0, BMP280, GFX, SSD1306

## Build, feltöltés, monitor
```bash
C:\.platformio\penv\Scripts\platformio.exe run --environment d1_mini
C:\.platformio\penv\Scripts\platformio.exe run --target upload --environment d1_mini
C:\.platformio\penv\Scripts\platformio.exe device monitor --environment d1_mini
```
- Monitor beállítás: COM5, 115200 baud.
- Bootkor rövid „Thermostat / Booting…” splash, majd fő képernyő.

## Használat
- OLED elrendezés (128×32):
  - Bal oldalt nagyban: aktuális hőmérséklet (EMA szűrt érték).
  - Jobb oldalt 3 sorban: MODE (OFF/AUTO/ON), H:ON/H:OFF, C: célhő.
- Gombok:
  - Le/Fel: `setTemp` −/+ 0.5 °C, nyomva tartva 300 ms-onként ismétel.
  - MODE: mód váltás (OFF → AUTO → ON → OFF…)
- Web felület: böngészőben az ESP IP címén a `/` útvonal.
  - Vezérlők: `/up`, `/down`, `/mode` (opcionális PIN: `?pin=1234`).

### Minimális relé bekapcsolási idő (anti-chatter)
- Cél: ha a relé bekapcsol, maradjon bekapcsolva legalább X percig, így nem „csetteg” gyorsan ki-be a hiszterézis szélén.
- Alapértelmezés: 5 perc.
- Állítás: másold át az include/secrets_example.h fájlból a megjegyzést az include/secrets.h fájlba, és add hozzá pl. 10 perchez:
  
  ```c
  #define MIN_ON_TIME_MS (10UL * 60UL * 1000UL)
  ```
  
- Működés: a kikapcsolást csak akkor engedi, ha a bekapcsolás óta eltelt legalább a megadott idő. Ez a hiszterézis mellett további védelmet ad a gyakori kapcsolgatás ellen.

## WiFi beállítás és titkok
- Másold az include/secrets_example.h fájlt include/secrets.h néven, és töltsd ki a `WIFI_SSID` / `WIFI_PASS` mezőket.
- A `CONTROL_PIN` opcionális; ha üres, nincs védelem a webes vezérlő végpontokon.
- A secrets.h fájl .gitignore-ban van, így a jelszó nem kerül verziókezelésbe.

## MQTT / Home Assistant
- MQTT broker szükséges (pl. Mosquitto). A Home Assistant MQTT integrációja legyen aktív.
- Autodiscovery alapértelmezetten engedélyezett, a discovery prefix: `homeassistant`.
- Topicok alapja: `thermostat/<deviceId>/...`
- Web felületen elérhető: **Discovery újraküldés** gomb (ha a HA lemaradt róla).

### MQTT beállítások (secrets.h)
```cpp
#define MQTT_HOST "192.168.0.119"
#define MQTT_PORT 1883
#define MQTT_USER "your_user"
#define MQTT_PASS "your_pass"
#define MQTT_BASE_TOPIC "thermostat"
#define MQTT_DISCOVERY_PREFIX "homeassistant"
#define MQTT_DEVICE_NAME "Thermostat"
```

### Home Assistant entitások
- Climate entitás (mód + setpoint + aktuális hőmérséklet)
- Temperature szenzor
- Heating binary sensor

## EEPROM mentés és kopásvédelem
- Célhő és mód mentése EEPROM-ba.
- Halasztott mentés (~2 s) + változás-ellenőrzés: felesleges írások elkerülése.

## Tippek és hibaelhárítás
- OLED sötét / „káosz”: ellenőrizd az I2C bekötést és a címet; induláskor I2C szkennelés fut.
- Folyamatos módváltás: a BTN_MODE (D0) lebeghet – tegyél felhúzót, vagy válts másik GPIO-ra.
- WiFi csatlakozás: ha 30 másodperc után timeout, ellenőrizd az SSID-t/jelszót és a 2.4 GHz hálózatot.
- Relé logika: ha eltérő polaritású modulod van, állítsd át a `RELAY_ACTIVE_HIGH` értékét.

## Fájlok
- Fő program: Arduino_projects/Termostat/src/main.cpp
- Beállítások: Arduino_projects/Termostat/platformio.ini
- Titkok példafájl: Arduino_projects/Termostat/include/secrets_example.h

## Licenc / Megjegyzések
- A projekt függ a megadott Adafruit könyvtáraktól és az ESP8266 Arduino keretrendszertől.
- Kérdés esetén szívesen segítek a finomhangolásban vagy további funkciókban.
