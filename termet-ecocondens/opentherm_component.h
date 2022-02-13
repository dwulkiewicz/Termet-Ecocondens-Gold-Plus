#include "esphome.h"
#include "esphome/components/sensor/sensor.h"
#include "OpenTherm.h"
//#include "opentherm_switch.h"
#include "opentherm_climate.h"
#include "opentherm_binary.h"
//#include "opentherm_output.h"

// Pins to OpenTherm Adapter
const int GPIO_BOILER_OT_RX = 4;  //ESP12 było 5; // 21 for ESP32
const int GPIO_BOILER_OT_TX = 16; //ESP12 było 4; // 22 for ESP32

const float minHotWaterTargetTemperature = 30;  		//Odczytane z Termet Ecocondens Gold+
const float maxHotWaterTargetTemperature = 60;			//Odczytane z Termet Ecocondens Gold+
const float defaultHotWaterTargetTemperature = 39;

const float minHeatingWaterTargetTemperature = 40;		//Odczytane z Termet Ecocondens Gold+
const float maxHeatingWaterTargetTemperature = 80;		//Odczytane z Termet Ecocondens Gold+
const float defaultHeatingWaterTargetTemperature = 50;

OpenTherm ot(GPIO_BOILER_OT_RX, GPIO_BOILER_OT_TX);

ICACHE_RAM_ATTR void handleInterrupt() {
	ot.handleInterrupt();
}

class OpenthermComponent: public PollingComponent {
private:
  const char *TAG = "opentherm_component";
  
  float lastHotWaterTargetTemperature = -1;
  float lastHeatingWaterTargetTemperature = -1;
public:
  //Switch *thermostatSwitch = new OpenthermSwitch();
  Sensor *return_temperature_sensor = new Sensor();
  Sensor *boiler_temperature = new Sensor();
  Sensor *pressure_sensor = new Sensor();
  Sensor *modulation_sensor = new Sensor();
  //Sensor *heating_target_temperature_sensor = new Sensor();
  OpenthermClimate *hotWaterClimate = new OpenthermClimate();
  OpenthermClimate *heatingWaterClimate = new OpenthermClimate();
  BinarySensor *flame = new OpenthermBinarySensor();
  
  // Set 6 sec. to give time to read all sensors (and not appear in HA as not available)
  OpenthermComponent(): PollingComponent(6000) {
  }

  void setup() override {
    // This will be called once to set up the component
    // think of it as the setup() call in Arduino
      ESP_LOGD("opentherm_component", "Setup");

      ot.begin(handleInterrupt);
 
      heatingWaterClimate->set_supports_two_point_target_temperature(false);
      heatingWaterClimate->set_temperature_settings(minHeatingWaterTargetTemperature, maxHeatingWaterTargetTemperature, lastHeatingWaterTargetTemperature);
      heatingWaterClimate->setup();

	  hotWaterClimate->set_supports_two_point_target_temperature(false);
      hotWaterClimate->set_temperature_settings(minHotWaterTargetTemperature, maxHotWaterTargetTemperature, lastHotWaterTargetTemperature);
      hotWaterClimate->setup();	
  }

  float getReturnTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tret, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }
  
  float getHotWaterTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tdhw, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  float getHotWaterTargetTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSet, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }
  
  float getHeatingWaterTargetTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxTSet, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  } 

  bool setHotWaterTemperature(float temperature) {
	    unsigned int data = ot.temperatureToData(temperature);
      unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, data);
      unsigned long response = ot.sendRequest(request);
      return ot.isValidResponse(response);
  }

  bool setHeatingTemperature(float temperature) {
	    unsigned int data = ot.temperatureToData(temperature);
      unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MaxTSet, data);
      unsigned long response = ot.sendRequest(request);
      return ot.isValidResponse(response);
  }

  float getModulation() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0));
    return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  float getPressure() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0));
    return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  void update() override {

    ESP_LOGD("opentherm_component", "update heatingWaterClimate: %i", heatingWaterClimate->mode);
    ESP_LOGD("opentherm_component", "update hotWaterClimate: %i", hotWaterClimate->mode);
    
    bool enableCentralHeating = heatingWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    bool enableHotWater = hotWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    bool enableCooling = false; // this boiler is for heating only
    
    //Set/Get Boiler Status
    auto response = ot.setBoilerStatus(enableCentralHeating, enableHotWater, enableCooling);
    bool isFlameOn = ot.isFlameOn(response);
    bool isCentralHeatingActive = ot.isCentralHeatingActive(response);
    bool isHotWaterActive = ot.isHotWaterActive(response);
    float return_temperature = getReturnTemperature();
    float hotWater_temperature = getHotWaterTemperature();
    float boilerTemperature = ot.getBoilerTemperature();
    float pressure = getPressure();
    float modulation = getModulation();
    
    // Set heating water temperature      
	// jeżeli pierwsze uruchomienie sterownika lub zmieniono temperaturę na piecu, wtedy ją zczytuję i ustawiam w komponencie Climate
	// jeżeli zmieniono wartość w climate to ustawiam taką na piecu
	float heatingWaterTargetTemperature = getHeatingWaterTargetTemperature();

    ESP_LOGD("opentherm_component", "heatingWaterTargetTemperature: %f", heatingWaterTargetTemperature);	  
    ESP_LOGD("opentherm_component", "lastHeatingWaterTargetTemperature: %f", lastHeatingWaterTargetTemperature);	  
    ESP_LOGD("opentherm_component", "heatingWaterClimate->target_temperature: %f", heatingWaterClimate->target_temperature);	        


    if (lastHeatingWaterTargetTemperature == heatingWaterClimate->target_temperature && lastHeatingWaterTargetTemperature != heatingWaterTargetTemperature) {
	  heatingWaterClimate->target_temperature = heatingWaterTargetTemperature;
	  lastHeatingWaterTargetTemperature = heatingWaterTargetTemperature;
      ESP_LOGD("opentherm_component", "get heatingWaterTargetTemperature from boiler: %f", lastHeatingWaterTargetTemperature);	  
    }
	//zmieniono temperaturę w HA, ustawiam tylko raz wartość na piecu
    else if (lastHeatingWaterTargetTemperature != heatingWaterClimate->target_temperature) {
      setHeatingTemperature(heatingWaterClimate->target_temperature);
	  lastHeatingWaterTargetTemperature = heatingWaterClimate->target_temperature;
      ESP_LOGD("opentherm_component", "set heatingWaterTargetTemperature to boiler: %f", lastHeatingWaterTargetTemperature);	  	  
    }
    else{
      ESP_LOGD("opentherm_component", "heatingWaterTargetTemperature unchanged");	      
    }  

    // Set hot water temperature
	// jeżeli pierwsze uruchomienie sterownika lub zmieniono temperaturę na piecu, wtedy ją zczytuję i ustawiam w komponencie Climate
	// jeżeli zmieniono wartość w climate to ustawiam taką na piecu    
	float hotWaterTargetTemperature = getHotWaterTargetTemperature();
		
    ESP_LOGD("opentherm_component", "hotWaterTargetTemperature: %f", hotWaterTargetTemperature);	  
    ESP_LOGD("opentherm_component", "lastHotWaterTargetTemperature: %f", lastHotWaterTargetTemperature);	  
    ESP_LOGD("opentherm_component", "hotWaterClimate->target_temperature: %f", hotWaterClimate->target_temperature);	        
		
    if (lastHotWaterTargetTemperature == hotWaterClimate->target_temperature && lastHotWaterTargetTemperature != hotWaterTargetTemperature) {
	  hotWaterClimate->target_temperature = hotWaterTargetTemperature;
	  lastHotWaterTargetTemperature = hotWaterTargetTemperature;
      ESP_LOGD("opentherm_component", "read hotWaterTargetTemperature from boiler: %f", lastHotWaterTargetTemperature);	 	  
    }
	//zmieniono temperaturę w HA, ustawiam tylko raz wartość na piecu
    else if (lastHotWaterTargetTemperature != hotWaterClimate->target_temperature) {
      setHotWaterTemperature(hotWaterClimate->target_temperature);
	  lastHotWaterTargetTemperature = hotWaterClimate->target_temperature;
      ESP_LOGD("opentherm_component", "send hotWaterTargetTemperature to boiler: %f", lastHotWaterTargetTemperature);	 		  
    }
    else{
      ESP_LOGD("opentherm_component", "hotWaterTargetTemperature unchanged");	      
    }  

    // Publish sensor values
    flame->publish_state(isFlameOn); 
    return_temperature_sensor->publish_state(return_temperature);
    boiler_temperature->publish_state(boilerTemperature);
    pressure_sensor->publish_state(pressure);
    modulation_sensor->publish_state(modulation);    
	//heating_target_temperature_sensor->publish_state(modulation); 

    // Publish status of thermostat that controls hot water
    hotWaterClimate->current_temperature = hotWater_temperature;
    hotWaterClimate->action = isHotWaterActive ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
    hotWaterClimate->publish_state();
    
    // Publish status of thermostat that controls heating
    heatingWaterClimate->current_temperature = boilerTemperature;
    heatingWaterClimate->action = isCentralHeatingActive ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
    heatingWaterClimate->publish_state();
  }

};