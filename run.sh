#!/bin/bash
export DDS_DOMAIN_ID=0
export MQTT_HOST=127.0.0.1

source /fastdds-install/setup.bash && /monitor/build/monitor $DDS_DOMAIN_ID $MQTT_HOST
