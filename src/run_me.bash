#!/usr/bin/env bash

# Check for superuser permissions.
if [ $(id -u) != "0" ]; then
    sudo $0
fi

# Build the tools.
gcc -o usbsh -Wall -O3 usbsh.c -lusb-1.0
gcc -o sniff -Wall -O3 sniff.c -lusb-1.0

# Loop over all devices.
devices=$(lsusb -d 1b1c: | sed -n "s/.* 1b1c:\(1b..\) .*/\1/p")

# Make output directory
rm -rf output output.tgz

mkdir -p output

# Loop over all devices
OIFS=$IFS
IFS=$'\n'
for dev in $devices; do
    ./sniff $dev > output/$dev.txt
done

IFS=$OIFS

tar czf output.tgz output

rm -rf output

exit 0
