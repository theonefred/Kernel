#!/bin/bash
make || exit 1
echo
echo "=====compilation done====="
echo 

module="jit"
device="jit"
mode="664"

insmod ./${module}.ko $* | exit 1
dmesg|tail -20
echo
echo "=====insmod done====="
echo 

rm -f /dev/${device}0

#cat /proc/devices | grep jit |cut -d' ' -f1
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
echo "major=$major"

mknod /dev/${device}0 c $major 0

chmod $mode /dev/${device}0


