# Fan thermostat

Hardware

XY-WFT1 Wifi thermostat (ES8285)
Sonoff SI7021 temperatue and humidity sensor  

based on 
https://github.com/massimozappino/thermostat_esp8266

MQTT topics

Get
LWT: thermostat_pv/LWT
Status: thermostat_pv/status

Set:
Power: thermostat_pv/power/set 0/1
Min temp: thermostat_pv/temperature_min/set
Max temp: thermostat_pv/temperature_max/set

HA MQTT sensors

binary
    - name: "PV box fan"
      state_topic: "thermostat_pv/status"
      payload_on: true
      payload_off: false
      value_template: "{{ value_json.out }}"
      unique_id: "thermostat_pv_out"

sensor

    - name: "PV box internal temperature"
      state_topic: "thermostat_pv/status"
      unit_of_measurement: "℃"
      value_template: '{{ value_json.temperature }}'
      unique_id: "thermostat_pv_temperature"

    - name: "PV box internal humidity"
      state_topic: "thermostat_pv/status"
      unit_of_measurement: "%"
      value_template: '{{ value_json.humidity }}'
      unique_id: "thermostat_pv_humidity"

HA UI card

type: entities
entities:
  - entity: sensor.pv_box_internal_temperature
    name: Temperature
    icon: mdi:thermometer
  - entity: sensor.pv_box_internal_humidity
    icon: mdi:water-percent
    name: Humidity
  - entity: binary_sensor.pv_box_fan
    name: Fan
    icon: mdi:fan
title: PV box
      
