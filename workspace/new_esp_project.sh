#!/bin/bash

PREFIX="esp32"
PRJ_NAME=$PREFIX"_"$1

mkdir $PRJ_NAME 
cp esp32_template/esp32_template.ino $PRJ_NAME/$PRJ_NAME.ino

