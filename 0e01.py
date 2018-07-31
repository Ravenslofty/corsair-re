#!/usr/bin/python3

devices = {0x40: "K63",
            0x17: "K65 RGB",
            0x07: "K65",
            0x37: "K65 LUX RGB",
            0x39: "K65 RAPIDFIRE",
            0x3f: "K68 RGB",
            0x13: "K70 RGB",
            0x09: "K70",
            0x33: "K70 LUX RGB",
            0x36: "K70 LUX",
            0x48: "K70 RGB MK.2",
            0x38: "K70 RAPIDFIRE RGB",
            0x3a: "K70 RAPIDFIRE",
            0x11: "K95 RGB",
            0x08: "K95",
            0x2d: "K95 PLATINUM RGB",
            0x20: "STRAFE RGB",
            0x49: "STRAFE RGB MK.2",
            0x15: "STRAFE",
            0x44: "STRAFE",
            0x12: "M65 RGB",
            0x2e: "M65 PRO RGB",
            0x14: "SABRE RGB", # Optical
            0x19: "SABRE RGB", # Laser
            0x2f: "SABRE RGB", # New?
            0x32: "SABRE RGB", # Alternate
            0x1e: "SCIMITAR RGB",
            0x3e: "SCIMITAR PRO RGB",
            0x3c: "HARPOON RGB",
            0x34: "GLAIVE RGB",
            0x3d: "K55 RGB",
            0x22: "KATAR",
            0x3b: "MM800 RGB POLARIS",
            0x2a: "VOID RGB"}

packet = [int("0x" + s, 16) for s in input().split(" ")]

if packet[0] != 0x0e or packet[1] != 0x01:
    print("Not a valid init packet.")
    exit(1)

# packet[2] and [3] are ignored

# packets 5 to 7 are unknown

print("Firmware: %d.%d%d" % (packet[9], (packet[8] & 0xF0) >> 4, (packet[8] & 0x0F)))
print("Bootloader: %d%d" % (packet[11], packet[10]))
print("Vendor: %x%x" % (packet[13], packet[12]))
print("Product: %x%x (%s)" % (packet[15], packet[14], devices[packet[14]]))
print("Poll rate: %dms" % (packet[16]))

# packets 17 to 19 are unknown

if packet[20] == 0xc0:
    print("Type: Keyboard")

    print("Layout: ", end='')
    layouts = [
        "ANSI",
        "ISO",
        "ABNT",
        "JIS",
        "Dubeolsik"
    ]
    print(layouts[packet[23]])
elif packet[20] == 0xc1:
    print("Type: Mouse")

    print("Lighting zones: ", end='')
    for i in range(8):
        if (1 << i) & packet[24]:
            print("%d " % i, end='')
    print()
elif packet[20] == 0xc2:
    print("Type: Mousepad")
else:
    print("Type: Unknown")
    

