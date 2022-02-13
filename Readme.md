# ESPHome Termet EcoCondens Gold Plus

Projekt na bazie ESPHome OpenTherm https://github.com/rsciriano/ESPHome-OpenTherm

#Biblioteki:
- OpenTherm boiler using [ESPHome](https://esphome.io/) 
- opentherm_adapter [Ihor Melnyk](http://ihormelnyk.com/opentherm_adapter) 

#Hardware:
- OpenTherm Adapter [DIYLESS](https://diyless.com/product/esp8266-thermostat-shield)
- opentherm_adapter [Ihor Melnyk](http://ihormelnyk.com/arduino_opentherm_controller)

#Instalacja
- Skopiuj wszystkie pliki do folderu ESPHome
- Upewnij się, że numery pinów są prawidłowe, w razie konieczności sprawdź plik opentherm_component.h w folderze termet-ecocondens.
- Edytuj plik termet-ecocondens.yaml:
     - Upewnij się, że ustawienia są prawidłowe dla Twojego urządzenia
- Flash ESP i skonfiguruj w Home Assistant. Powinien zostać automatycznie wykryty przez ESPHome Integration.
