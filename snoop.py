import re
import subprocess
import sys
import time

product = 0x1b2e

def detect():
    result = subprocess.run(["lsusb", "-d", "1b1c:"],
            universal_newlines=True,
            stdout=subprocess.PIPE).stdout
    
    match = re.match(r"Bus \d+ Device \d+: ID 1b1c:(.+) .+", result)

    if not match:
        print("You have no Corsair devices plugged in.", file=sys.stderr)
        exit(1)

    return match.groups()

def run(cmd):
    global product
    result = subprocess.run(["./usbsh", "-v", "0x1b1c", "-p", hex(product)],
            universal_newlines=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            input=cmd)
    return result

def search(cmds, depth, maxdepth):
    """ Perform a depth-first search of the commands. """
    # Abort if we've gone too deep.
    if depth >= maxdepth:
        return

    # Known "don't care" values
    if len(cmds) == 2 and cmds[1] in {0, 1, 14}:
        return

    valid = []
    results = []
    count = 0

    # First pass: hunt for valid (i.e. responsive) packets.
    for byte in range(256):
        cmd = " ".join([str(len(cmds) + 1)] + [hex(n)[2:] for n in cmds] + [hex(byte)[2:]])
        result = run(cmd)

        if "error" not in result.stdout:
            print(cmd[2:], ":", result.stdout, end='')
            results.append(result.stdout[11:])
            valid.append(byte)

            if len(results) >= 2:
                count = 0
                for r in results:
                    if r == results[-1]:
                        count += 1
                    else:
                        count = 0
                if count >= 16:
                    break
           
            print(cmds + [byte], file=sys.stderr)
            search(cmds + [byte], depth + 1, maxdepth)

        if "run this as root" in result.stdout:
            print(result.stdout)
            sys.exit(1)

for dev in detect():
    product = int("0x" + dev, 16)

    try:
        # First reset the device to synchronise it.
        run("2 07 02")

        print("Don't touch your device", file=sys.stderr)
        time.sleep(2)

        print("Beginning search", file=sys.stderr)
        count = search([0x0e], 1, 4)
        print("Done!", file=sys.stderr)

    finally:
        # Then reset when we're done.
        run("2 07 02")
