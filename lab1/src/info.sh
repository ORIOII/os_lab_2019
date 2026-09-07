#!/bin/bash

FULL_PATH=$(pwd)
DATE=$(date +"%d.%m.%Y")
TIME=$(date +"%H:%M:%S")
ENV_PATH=$PATH

echo "Current path: $FULL_PATH"
echo "Current date and time: $DATE $TIME"
echo "#PATH Data: $ENV_PATH"