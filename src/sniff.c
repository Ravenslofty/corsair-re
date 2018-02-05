/*     USB Shell for interactive debugging.   */
/*     Tasos Sahanidis <code@tasossah.com>    */
/*  Dan Ravensloft <dan.ravensloft@gmail.com> */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#include "common.c" // Yes, I'm including the source code file for now.

int main(int argc, char ** argv) 
{
    if (argc != 2) {
        printf("Usage: %s <product ID>\n", argv[0]);
        return 1;
    }

    if (getuid() != 0) {
        printf("You need to run this as root.\n");
        return 1;
    }

    int retval;

    //Init
    retval = libusb_init(NULL);
    debug("Init: %d\n", retval);

    retval = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    debug("Hotplug: %d\n", retval);

    // Get Product ID.
    sscanf(argv[1], "%lx", &product_id);
    vendor_id = 0x1b1c;
    verbose = 0;
    silent = 1;

    // Initialise.
    init();

    // Reset device.
    unsigned char reset[PKLEN] = { 0x07, 0x02, 0 };

    printf("Please don't touch your device.\n");
    printf("Will now reset the device.\n");
    urb_interrupt(reset, 0);

    sleep(2);

    init();

    printf("Will now begin.\n");

    //Send Interrupt
    unsigned char packet[PKLEN] = { 0x0e, 0x00, 0 };

    for (unsigned short arg1 = 0x00; arg1 <= 0xFF; arg1++) {
        for (unsigned short arg2 = 0x00; arg2 <= 0xFF; arg2++) {
            packet[1] = arg1 & 0xFF;
            packet[2] = arg2 & 0xFF;
            fprintf(stderr, "\r%02x%02x%02x%02x", packet[0], packet[1], packet[2], packet[3]);
            retval = urb_interrupt(packet, 1);
            if (retval == 0) {
                fprintf(stderr, " seems to do something.\n");
                retval = urb_interrupt(packet, 0);
            }
            else if (retval != -1000) {
                break;
            }
            fflush(stdout);
            fflush(stderr);
        }
    }

    fprintf(stderr, "\n");

    // Reset the device again and let the OS clean up.
    urb_interrupt(reset, 0);

    return 0;
}
