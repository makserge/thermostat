#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#define STATUS_LED_PIN 2
#define OUT_LED_PIN 12
#define RELAY_PIN 5
#define SENSOR_PIN 13

#define DEVICE_ID "thermostat_pv"

#define EEPROM_MODE_ADDR 0
#define EEPROM_MIN_TEMP_ADDR 10
#define EEPROM_MAX_TEMP_ADDR 20

#define DEFAULT_POWER_MODE false
#define DEFAULT_TARGET_MIN_TEMP 25
#define DEFAULT_TARGET_MAX_TEMP 40
#define MQTT_PUBLISH_INTERVAL 10000

#define MQTT_OFFLINE_MESSAGE "Offline"
#define MQTT_ONLINE_MESSAGE "Online"

#define MQTT_SERVER "192.168.8.100"
#define MQTT_PORT 1883
#define MQTT_USERNAME "*"
#define MQTT_PASSWORD "*"

#define MQTT_LWT_TOPIC DEVICE_ID "/LWT"
#define MQTT_STATUS_TOPIC DEVICE_ID "/status"
#define MQTT_SET_POWER_TOPIC DEVICE_ID "/power/set"
#define MQTT_SET_MIN_TEMP_TOPIC DEVICE_ID "/temperature_min/set"
#define MQTT_SET_MAX_TEMP_TOPIC DEVICE_ID "/temperature_max/set"

boolean powerMode = false;
boolean currentOutputState = false;
float targetMinTemperature = 0.0;
float targetMaxTemperature = 0.0;
float currentTemperature = 0.0;
float currentHumidity = 0.0;
unsigned long lastMQTTMsg = 0;

DynamicJsonDocument doc(6);
JsonObject mqttPayload = doc.createNestedObject("data");
char mqttPayloadString[192];

WiFiManager wm;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiEventHandler onGotIPHandler;
WiFiEventHandler onStationModeConnectedHandler;
WiFiEventHandler onStationModeDisconnectedHandler;

bool loadPowerMode() {
  bool value = DEFAULT_POWER_MODE;
  EEPROM.get(EEPROM_MODE_ADDR, value);
  return value;
}

void savePowerMode(bool value) {
  EEPROM.put(EEPROM_MODE_ADDR, value);
  EEPROM.commit();
}

float loadTargetMinTemperature() {
  float value = 0.00;
  EEPROM.get(EEPROM_MIN_TEMP_ADDR, value);
  if (value > 0) {
    return value;
  }
  return DEFAULT_TARGET_MIN_TEMP;
}

void saveTargetMinTemperature(float temperature) {
  EEPROM.put(EEPROM_MIN_TEMP_ADDR, temperature);
  EEPROM.commit();
}

float loadTargetMaxTemperature() {
  float value = 0.00;
  EEPROM.get(EEPROM_MAX_TEMP_ADDR, value);
  if (value > 0) {
    return value;
  }
  return DEFAULT_TARGET_MAX_TEMP;
}

void saveTargetMaxTemperature(float temperature) {
  EEPROM.put(EEPROM_MAX_TEMP_ADDR, temperature);
  EEPROM.commit();
}

void setupPins() {
  pinMode(SENSOR_PIN, INPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(OUT_LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(STATUS_LED_PIN, HIGH);
}

void loadData() {
  powerMode = loadPowerMode();
  targetMinTemperature = loadTargetMinTemperature();
  targetMaxTemperature = loadTargetMaxTemperature();
}

static void onGotIP(const WiFiEventStationModeGotIP& e) {
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  setupMqttClient();
}

static void onStationConnected(const WiFiEventStationModeConnected& evt) {
  Serial.println("Station connected");
}

static void onStationDisconnected(const WiFiEventStationModeDisconnected& evt) {
  Serial.print("StationModeDisconnected - reason=");
  Serial.println(evt.reason);
  
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  onGotIPHandler = WiFi.onStationModeGotIP(onGotIP);
  onStationModeConnectedHandler = WiFi.onStationModeConnected(onStationConnected);
  onStationModeDisconnectedHandler = WiFi.onStationModeDisconnected(onStationDisconnected);
    
  //reset settings - wipe credentials for testing
  //wm.resetSettings();
  wm.setConfigPortalBlocking(false);

  if (wm.autoConnect("Thermostat")) {
    Serial.println("connected...");
  } else {
    Serial.println("Configportal running");
  }
}

void setupMqttClient() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String receivedMessage;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    receivedMessage += (char)payload[i];    
  }
  Serial.println();

  if (strcmp(topic, MQTT_SET_POWER_TOPIC) == 0) {
    powerMode = (receivedMessage.equals("1"));
    savePowerMode(powerMode);
  }

  if (strcmp(topic, MQTT_SET_MIN_TEMP_TOPIC) == 0) {
    targetMinTemperature = receivedMessage.toFloat();
    if ((targetMinTemperature < 0) || (targetMinTemperature > 100)) {
      targetMinTemperature = DEFAULT_TARGET_MIN_TEMP;
    }
    saveTargetMinTemperature(targetMinTemperature);   
  }

  if (strcmp(topic, MQTT_SET_MAX_TEMP_TOPIC) == 0) {
    targetMaxTemperature = receivedMessage.toFloat();
    if ((targetMaxTemperature < 0) || (targetMaxTemperature > 100)) {
      targetMaxTemperature = DEFAULT_TARGET_MAX_TEMP;
    }
    saveTargetMaxTemperature(targetMaxTemperature);
  }

  lastMQTTMsg = 0;
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  
  setupPins();
  loadData();
  setupWifi();
  
  readSensor();

  Serial.println("Setup done");
}

bool waitState(int pin, bool state) {
  uint64_t timeout = micros();
  while (micros() - timeout < 100) {
    if (digitalRead(pin) == state) {
      return true;
    }      
    delayMicroseconds(1);
  }
  return false;
}

void readSensor(int pin) {
  uint8_t data[5] = {0};

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(500);
  digitalWrite(pin, HIGH);
  delayMicroseconds(20);
  pinMode(pin, INPUT);

  uint32_t i = 0;
  if (waitState(pin, 0) && waitState(pin, 1) && waitState(pin, 0)) {
    for (i = 0; i < 40; i++) {
      if (!waitState(pin, 1)) {
        break;
      }
      delayMicroseconds(35);
      if (digitalRead(pin) == HIGH) {
        data[i / 8] |= (1 << (7 - i % 8));
      }
      if (!waitState(pin, 0)) {
        break;
      }
    }
  }

  uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
  if ((i == 40)&& (data[4] == checksum)) {
    double tempInternal = (((data[2] & 0x7F) << 8) | data[3]) * 0.1;
    if (data[2] & 0x80) {
      tempInternal *= -1;
    }
    currentTemperature = tempInternal;  
    currentHumidity = ((data[0] << 8) | data[1]) * 0.1;
  }
}

void readSensor() {
  readSensor(SENSOR_PIN);
  Serial.print("Temperature: ");
  Serial.print(currentTemperature);
  Serial.print("C, Humidity: ");
  Serial.print(currentHumidity);
  Serial.println("%");
}

void setOutput(bool state) {
  Serial.print("Output State: ");
  Serial.println(state);
  digitalWrite(RELAY_PIN, state);
  digitalWrite(OUT_LED_PIN, state);
}

void checkTargetTemperature() {
  if (powerMode && (currentTemperature >= targetMaxTemperature)) {
    currentOutputState = true;
    setOutput(currentOutputState);
    return;
  }
  if (!powerMode || (currentTemperature <= targetMinTemperature)) {
    currentOutputState = false;
    setOutput(currentOutputState);
  }
}

void sendValues() {
  mqttPayload["device_id"] = DEVICE_ID;
  if (isnan(currentHumidity) || isnan(currentTemperature)) {
    mqttPayload["temperature"] = (char*)NULL;
    mqttPayload["humidity"] = (char*)NULL;
  }
  else {
    mqttPayload["temperature"] = String(currentTemperature, 1);
    mqttPayload["humidity"] = String(currentHumidity, 1);
  }
  mqttPayload["power"] = powerMode;
  mqttPayload["out"] = currentOutputState;
  mqttPayload["minTemperature"] = targetMinTemperature;
  mqttPayload["maxTemperature"] = targetMaxTemperature;
  
  serializeJson(mqttPayload, mqttPayloadString);

  mqttClient.publish(MQTT_STATUS_TOPIC, mqttPayloadString);
}

void reconnectMqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_LWT_TOPIC, 0, true, MQTT_OFFLINE_MESSAGE)) {
      Serial.println("connected");

      mqttClient.subscribe(MQTT_SET_POWER_TOPIC);
      mqttClient.subscribe(MQTT_SET_MIN_TEMP_TOPIC);
      mqttClient.subscribe(MQTT_SET_MAX_TEMP_TOPIC);

      mqttClient.publish(MQTT_LWT_TOPIC, MQTT_ONLINE_MESSAGE, true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  wm.process();
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  mqttClient.loop();
  unsigned long now = millis();
  if (now - lastMQTTMsg > MQTT_PUBLISH_INTERVAL) {
    lastMQTTMsg = now;
    readSensor();
    checkTargetTemperature();
    sendValues();
  }
}
