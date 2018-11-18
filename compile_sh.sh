#!/bin/bash

# if [ $attempt -eq 20 ]
# then
# echo "OK"
# fi

mbed compile

while [ $? -ne 0 ]
do
mbed compile

done
