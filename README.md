# ESPHome Termet EcoCondens Gold Plus

Projekt na bazie ESPHome OpenTherm https://github.com/rsciriano/ESPHome-OpenTherm

#Biblioteki:
- OpenTherm boiler using [ESPHome](https://esphome.io/) 
- [Ihor Melnyk](http://ihormelnyk.com/opentherm_adapter) or 

#Hardware:
- [DIYLESS](https://diyless.com/product/esp8266-thermostat-shield) OpenTherm Adapter

#Instalacja
- Copy the content of this repository to your ESPHome folder
- Make sure the pin numbers are right, check the file opentherm_component.h in the esphome-opentherm folder.
- Edit the opentherm.yaml file:
    - Make sure the board and device settings are correct for your device
    - Set the sensor entity_id with the external temperature sensor's name from Home Assistant. (The ESPHome sensor name is temperature_sensor).
- Flash the ESP and configure in Home Assistant. It should be auto-discovered by the ESPHome Integration.
