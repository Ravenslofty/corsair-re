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

short should_reset = 0;

void usage(char *cmd)
{
    error("Usage: %s <product ID>\n", cmd);
    error("\nOptional arguments:\n");
    error("\t -h\t\tPrint this help and exit.\n");
    error("\t -r\t\tReset device before sniffing.\n");
    error("\t -V\t\tVerbose output.\n");
}

int main(int argc, char ** argv)
{
    char c;
    int retval;

    // notify user of requested root permissions
    if (geteuid() != 0) {
        printf("You need to run this as root.\n");
        return 1;
    }

    // parse options
    while ((c = getopt(argc, argv, ":hrV")) != -1)
    {
        switch (c)
        {
            case 'V':
                verbose = 1;
                silent = 0;
                break;
            case 'r':
                should_reset = 1;
                break;
            case ':':
                break;
            // handle unknown options and fall through to help text handler
            case '?':
                error("Unknown option: -%c\n", optopt);
            // print help text and fall through to default handler
            case 'h':
                usage(argv[0]);
            // exit the program because of errors/help text printing
            default:
                exit(1);
        }
    }

    if (optind >= argc)
    {
        error("Missing product ID.\n");
        usage(argv[0]);
        exit(1);
    }

    //Init
    retval = libusb_init(NULL);
    debug("Init: %d\n", retval);

    retval = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    debug("Hotplug: %d\n", retval);

    // Get Product ID. (first non-option element)
    sscanf(argv[optind], "%lx", &product_id);
    vendor_id = 0x1b1c;

    // Initialise.
    init();

    // Reset device.
    unsigned char reset[PKLEN] = { 0x07, 0x02, 0 };

    fprintf(stderr, "Please don't touch your device.\n");

    // force reset of device
    if (should_reset)
    {
        fprintf(stderr, "Will now reset the device.");
        urb_interrupt(reset, 0);

        // simulate waiting time
        sleep(1);
        fprintf(stderr, ".");
        sleep(1);
        fprintf(stderr, ".");

        // re-initialize the device after resetting
        init();
    }

    fprintf(stderr, "Will now begin.\n");

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
