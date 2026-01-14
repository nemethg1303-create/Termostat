<<<<<<< HEAD
# Termostat
=======
# Termosztat – Bekötési és használati útmutató

ESP8266 (Wemos D1 mini) alapú termosztát AHTX0 + BMP280 szenzorokkal, SSD1306 I2C OLED kijelzővel (128×32), webes felülettel és relévezérléssel.

**Verzió**
- FW v1.0.0 — stabil alapfunkciók és kijelzőelrendezés.
  - 128×32 OLED elrendezés: bal oldalt nagy hőmérséklet, jobb oldalt 3 sor (MODE, relé státusz, célhő).
  - Web UI: nagy értékek, egységes gombstílus, Fel/Le sorrend cserélve.
  - EEPROM kopásvédelem: halasztott mentés (~2 s), csak valódi változásnál.
  - Gomb ismétlés: Fel/Le nyomva tartva 300 ms-onként léptet.
  - Kijelző kikapcsolás: 5 perc tétlenség után.

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
  - `BTN_UP`: D6 (GPIO12), gomb másik lába GND
  - `BTN_DOWN`: D7 (GPIO13), gomb másik lába GND
  - `BTN_MODE`: D0 (GPIO16) – ajánlott külső felhúzó (10k → 3V3), és NE nyomd boot közben

Megjegyzés a bootolatlan lábakról:
- Kerüld gombként: D3 (GPIO0), D4 (GPIO2), D8 (GPIO15) – ezek boot strap lábak, nyomógombbal bootproblémát okozhatnak.

## Bekötési lépések
1. Kijelző (SSD1306 I2C, 128×32):
   - VCC → 3V3, GND → GND, SDA → D2, SCL → D1
   - A címe tipikusan 0x3C. Ha 0x3D, állítsd át az `OLED_ADDR` értékét a kódban.
2. Szenzorok (AHTX0, BMP280):
   - I2C buszon: VCC → 3V3, GND → GND, SDA → D2, SCL → D1
3. Gombok:
   - Egyik láb GND, másik a PIN (D6, D7, D0)
   - A kód `INPUT_PULLUP`-ot használ; a gomb lenyomás LOW lesz.
   - Hosszú vezeték esetén tehetsz 100 nF kondit a gomb két lába közé a zaj ellen.
4. Relé:
   - Használj relémodult (opto + tranzisztor), vagy tranzisztoros meghajtást (NPN + dióda a tekercs fölött).
   - Modul VCC → megfelelő táp (5V-os modulnál 5V, 3.3V-osnál 3V3), GND → GND, IN → D5.
   - Sok relémodul aktív LOW. A jelenlegi kód aktív HIGH-ra ír (`HIGH` = bekapcsol). Ha a modulod aktív LOW, jelezd, és invertáljuk a vezérlést.

## Szoftver környezet
- PlatformIO
- Konfiguráció: [Arduino_projects/Termostat/platformio.ini](Arduino_projects/Termostat/platformio.ini)
  - ESP8266 környezet: `[env:d1_mini]`
  - Könyvtárak: Adafruit AHTX0, BMP280, GFX, SSD1306

## Build, feltöltés, monitor
```bash
C:\.platformio\penv\Scripts\platformio.exe run --environment d1_mini
C:\.platformio\penv\Scripts\platformio.exe run --target upload --environment d1_mini
C:\.platformio\penv\Scripts\platformio.exe device monitor --environment d1_mini
```
- Monitor beállítás: COM5, 115200 baud.
- Bootkor látszik egy rövid "Thermostat / Booting..." splash, majd a fő képernyő.

## Használat
- OLED elrendezés (128×32):
  - Bal oldalt nagyban: aktuális hőmérséklet (GFX fonttal, középre igazítva)
  - Jobb oldalt 3 sorban: `MODE` (OFF/AUTO/ON), `H:ON/H:OFF`, `C:` célhőmérséklet
- Gombok:
  - Le/Fel: `setTemp` −/+ 0.5 °C (weben is: balra „Le”, jobbra „Fel”)
  - MODE: mód váltás (OFF → AUTO → ON → OFF…)
- Web felület: böngészőben az ESP IP címén a `/` útvonal
  - Vezérlők: `/up`, `/down`, `/mode`
  - Diagnosztika: `/diag` (geometria, alsó sáv ellenőrzés)
  - OLED kalibráció: `/oled?offset=<0..63>&rot=<0..3>&com=<inc|dec>` (váltáskor diag fut)

## EEPROM mentés és kopásvédelem
- A beállítások (célhőmérséklet, mód) EEPROM-ba mentődnek, de a felesleges írások elkerülésére halasztott mentést használunk.
- Működés: minden változáskor ütemezés történik, majd ~2 s nyugalom után mentés, csak ha ténylegesen eltért az utoljára mentett értéktől.
- Előny: gyors egymás utáni gombnyomások nem okoznak fölösleges írásokat.

## Tippek és hibaelhárítás
- Nincs kijelzés / "káosz": ellenőrizd az I2C bekötést és a címet. A kód tartalmaz I2C szkennert, a soros monitoron látszik a talált eszköz (0x3C/0x3D).
- Folyamatos módváltás: valószínűleg lebeg a `BTN_MODE` (D0). Adj külső felhúzót (10k → 3V3), vagy használd másik stabil GPIO-t (pl. D6/D7). Ne nyomd a gombot boot közben.
- WiFi csatlakozás: ha csak pontok látszanak, ellenőrizd SSID/jelszót és a 2.4 GHz kapcsolatot. Igény esetén beépítünk timeoutot és AP fallbacket.
- Relé logika: ha a modulod aktív LOW, jelezd – beállítjuk a kódot, hogy `LOW` legyen a bekapcsolás.

## Konfiguráció és build
- PlatformIO: [Arduino_projects/Termostat/platformio.ini](Arduino_projects/Termostat/platformio.ini)
- Feltöltés és monitor (példa):
```bash
C:\.platformio\penv\Scripts\platformio.exe run --environment d1_mini
C:\.platformio\penv\Scripts\platformio.exe run --target upload --environment d1_mini
C:\.platformio\penv\Scripts\platformio.exe device monitor --environment d1_mini
```

## Fájlok
- Fő program: [Arduino_projects/Termostat/src/main.cpp](Arduino_projects/Termostat/src/main.cpp)
- Beállítások: [Arduino_projects/Termostat/platformio.ini](Arduino_projects/Termostat/platformio.ini)

## Licenc / Megjegyzések
- A projekt függ a megadott Adafruit könyvtáraktól és az ESP8266 Arduino keretrendszertől.
- Kérdés esetén szívesen finomhangolom a pin-kiosztást vagy a relé/paint logikát a konkrét modulodhoz.
>>>>>>> 1f93178 (chore: initialize repo with v1.0.0)
