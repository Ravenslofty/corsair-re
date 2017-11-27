#!/usr/bin/python3

packet = [int("0x" + s, 16) for s in input().split(" ")]

if packet[0] != 0x0e or packet[1] != 0x01:
    print("Not a valid init packet.")
    exit(1)

# packet[2] and [3] are ignored

if packet[4]:
    print("Lighting: 24 bit")
else:
    print("Lighting: 9 bit")

# packets 5 to 7 are unknown

print("Firmware: %d.0%d" % (packet[9], packet[8])) 
print("Bootloader: %d%d" % (packet[11], packet[10]))
print("Vendor: %x%x" % (packet[13], packet[12]))
print("Product: %x%x" % (packet[15], packet[14]))
print("Poll rate: %dms" % (packet[16]))

# packets 17 to 19 are unknown

if packet[20] == 0xc0:
    print("Type: Keyboard")
elif packet[20] == 0xc1:
    print("Type: Mouse")
else:
    print("Type: Unknown")

