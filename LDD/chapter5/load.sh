#!/bin/bash
make || exit 1
echo
echo "=====compilation done====="
echo 

module="hello"
device="hello"
mode="664"

insmod ./${module}.ko number=15 name="NEW_NAME" arr=1,2,3 || exit 1
dmesg|tail -15
echo
echo "=====insmod done====="
echo 

rm -f /dev/${device}0

#cat /proc/devices | grep hello |cut -d' ' -f1
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
echo "major=$major"

mknod /dev/${device}0 c $major 0

chmod $mode /dev/${device}0


