#!/bin/bash

# output logging
echo "\n\n"
echo "[INFO] Service Started at $(date)"

# setup pwd
cd /home/pi/Downloads/Bee-Tracking-main

# setup env variables
export PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python
if [[ ! -e env.sh ]]; then
    # default env file
    cp env.sh.template env.sh
    serial=$(uuid)
    printf "export SDP_DEVICE_SERIAL=$serial\n" >> env.sh
    printf "[INFO] Configuration file not found, new config created with serial number $serial\n\n"
fi

source env.sh

# start program
/home/pi/Downloads/Bee-Tracking-main/Webcam

