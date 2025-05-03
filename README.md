#Fan thermostat

Hardware

XY-WFT1 Wifi thermostat (ES8285)
Sonoff SI7021 temperatue and humidity sensor  

based on 
https://github.com/massimozappino/thermostat_esp8266

MQTT topics

Get
LWT: thermostat_pv/LWT
Status: thermostat/status

Set:
Power: thermostat_pv/power/set 0/1
Min temp: thermostat_pv/temperature_min/set
Max temp: thermostat_pv/temperature_max/set
