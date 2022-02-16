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

const unsigned long publishTime = 180000;

const float minHotWaterTargetTemperature = 30;  		//Odczytane z Termet Ecocondens Gold+
const float maxHotWaterTargetTemperature = 60;			//Odczytane z Termet Ecocondens Gold+
const float defaultHotWaterTargetTemperature = 39;

const float minHeatingWaterTargetTemperature = 40;		//Odczytane z Termet Ecocondens Gold+
const float maxHeatingWaterTargetTemperature = 80;		//Odczytane z Termet Ecocondens Gold+
const float defaultHeatingWaterTargetTemperature = 50;

OpenTherm ot(GPIO_BOILER_OT_RX, GPIO_BOILER_OT_TX);

IRAM_ATTR  void handleInterrupt() {
	ot.handleInterrupt();
}

struct Values {
    bool isFlameOn = false;
	unsigned long isFlameOnPublishTime = 0;
    bool isCentralHeatingActive = false;
	unsigned long isCentralHeatingActivePublishTime = 0;
    bool isHotWaterActive = false;
	unsigned long isHotWaterActivePublishTime = 0;	
	float boilerTemperature = -1; 
	unsigned long boilerTemperaturePublishTime = 0;	
	float hotWaterTemperature = -1; 
	unsigned long hotWaterTemperaturePublishTime = 0;
	float returnTemperature	= -1;
	unsigned long returnTemperaturePublishTime = 0;		
	float pressure = -1; 
	unsigned long pressurePublishTime = 0;
	float modulation = -1;
	unsigned long modulationPublishTime = 0;	
};

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
  
  Values lastValues; 
  
  // Set 1 sec. to give time to read all sensors (and not appear in HA as not available)
  OpenthermComponent(): PollingComponent(1000) {
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
    ESP_LOGD("opentherm_component", "update()");

    Values currValues;
    
    bool enableCentralHeating = heatingWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    //bool enableHotWater = hotWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT; //tylko odczyt
    //bool enableCooling = false; // this boiler is for heating only
    
	
    //Set/Get Boiler Status
    unsigned long response = ot.setBoilerStatus(enableCentralHeating);
    OpenThermResponseStatus responseStatus = ot.getLastResponseStatus();  
       
    if (responseStatus == OpenThermResponseStatus::SUCCESS) {
		currValues.isFlameOn = ot.isFlameOn(response);
		currValues.isCentralHeatingActive = ot.isCentralHeatingActive(response);		
		currValues.isHotWaterActive = ot.isHotWaterActive(response);
		delay(100);
    }
    if (responseStatus == OpenThermResponseStatus::NONE) {
        ESP_LOGD("opentherm_component", "OpenTherm Error: OpenTherm is not initialized");
		return;
    }
    else if (responseStatus == OpenThermResponseStatus::INVALID) {
		//String msg = "Error: Invalid response " + String(response, HEX);
        ESP_LOGD("opentherm_component", "OpenTherm Error: Invalid response");
		return;
    }
    else if (responseStatus == OpenThermResponseStatus::TIMEOUT) {
        ESP_LOGD("opentherm_component", "OpenTherm Error: Response timeout");
		return;
    }	
	
    currValues.returnTemperature = getReturnTemperature();
	delay(100);    
    currValues.hotWaterTemperature = getHotWaterTemperature();
	delay(100);    
    currValues.boilerTemperature = ot.getBoilerTemperature();
	delay(100);    
    currValues.pressure = getPressure();
	delay(100);    
    currValues.modulation = getModulation();
	delay(100);    
    
    // Set heating water temperature      
	// jeżeli pierwsze uruchomienie sterownika lub zmieniono temperaturę na piecu, wtedy ją zczytuję i ustawiam w komponencie Climate
	// jeżeli zmieniono wartość w climate to ustawiam taką na piecu
	float heatingWaterTargetTemperature = getHeatingWaterTargetTemperature();
	delay(100);
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
	delay(100);		
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
	unsigned long milis = millis();
    if (milis - lastValues.isFlameOnPublishTime >= publishTime || currValues.isFlameOn != lastValues.isFlameOn){
		lastValues.isFlameOnPublishTime = milis;		
		lastValues.isFlameOn = currValues.isFlameOn;
		flame->publish_state(currValues.isFlameOn); 
	}
   
    if (milis - lastValues.returnTemperaturePublishTime >= publishTime || currValues.returnTemperature != lastValues.returnTemperature){
		lastValues.returnTemperaturePublishTime = milis;		
		lastValues.returnTemperature = currValues.returnTemperature;
		return_temperature_sensor->publish_state(currValues.returnTemperature); 
	}
   
    if (milis - lastValues.pressurePublishTime >= publishTime || currValues.pressure != lastValues.pressure){
		lastValues.pressurePublishTime = milis;		
		lastValues.pressure = currValues.pressure;
		pressure_sensor->publish_state(currValues.pressure); 
	}  
   
    if (milis - lastValues.modulationPublishTime >= publishTime || currValues.modulation != lastValues.modulation){
		lastValues.modulationPublishTime = milis;		
		lastValues.modulation = currValues.modulation;
		modulation_sensor->publish_state(currValues.modulation); 
	}     

    // Publish status of thermostat that controls hot water
	if (milis - lastValues.hotWaterTemperaturePublishTime >= publishTime || currValues.hotWaterTemperature != lastValues.hotWaterTemperature ||
		milis - lastValues.isHotWaterActivePublishTime >= publishTime || currValues.isHotWaterActive != lastValues.isHotWaterActive){
		lastValues.isHotWaterActivePublishTime = milis;		
		lastValues.isHotWaterActive = currValues.isHotWaterActive;
		lastValues.hotWaterTemperaturePublishTime = milis;		
		lastValues.hotWaterTemperature = currValues.hotWaterTemperature;
		hotWaterClimate->current_temperature = currValues.hotWaterTemperature;
		hotWaterClimate->action = currValues.isHotWaterActive ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
		hotWaterClimate->publish_state();
	}	
    
    // Publish status of thermostat that controls heating
	if (milis - lastValues.boilerTemperaturePublishTime >= publishTime || currValues.boilerTemperature != lastValues.boilerTemperature ||
		milis - lastValues.isCentralHeatingActivePublishTime >= publishTime || currValues.isCentralHeatingActive != lastValues.isCentralHeatingActive){
		lastValues.isCentralHeatingActivePublishTime = milis;		
		lastValues.isCentralHeatingActive = currValues.isCentralHeatingActive;
		lastValues.boilerTemperaturePublishTime = milis;		
		lastValues.boilerTemperature = currValues.boilerTemperature;		
		boiler_temperature->publish_state(currValues.boilerTemperature);
		heatingWaterClimate->current_temperature = currValues.boilerTemperature;
		heatingWaterClimate->action = currValues.isCentralHeatingActive ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
		heatingWaterClimate->publish_state();	
	}	
	
  }

};