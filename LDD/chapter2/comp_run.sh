#!/bin/bash
make
#insmod ./helloworld.ko
#insmod ./helloworld.ko age=15 name="NEW_NAME"
insmod ./helloworld.ko age=15 name="NEW_NAME" arr=1,2,3
dmesg|tail -10
echo
echo "=====insmod done====="
echo 
rmmod helloworld
dmesg|tail -2
