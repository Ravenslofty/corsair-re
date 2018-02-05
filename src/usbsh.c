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

void usage()
{
    error("Usage: usbsh [OPTION]...\n");
    error("\nMandatory arguments:\n");
    error("-p\t\tUSB Product ID to connect to.\n");
    error("-v\t\tUSB Vendor ID to connect to.\n");
    error("\nOptional arguments:\n");
    error("-1, -2, -3\tUse Corsair endpoints for protocols 1, 2 or 3.\n");
    error("-i\t\tUse this endpoint to receive from the USB device.\n");
    error("-o\t\tUse this endpoint to send to the USB device.\n");
    error("-V\t\tVerbose output.\n");
}

int main(int argc, char ** argv) 
{
    if (getuid() != 0) {
        error("You need to run this as root.\n");
        return 1;
    }

    char c;
    while ((c = getopt(argc, argv, ":123i:o:p:v:V")) != -1) {
        switch (c) {
        case '1':
            protocol = PROTO_V1;
            break;
        case '2':
            protocol = PROTO_V2;
            break;
        case '3':
            protocol = PROTO_V3;
            break;
        case 'i':
            protocol = PROTO_OVERRIDE;
            input_endpoint = strtol(optarg, NULL, 0);
            break;
        case 'o':
            protocol = PROTO_OVERRIDE;
            output_endpoint = strtol(optarg, NULL, 0);
            break;
        case 'p':
            product_id = strtol(optarg, NULL, 0);
            break;
        case 'v':
            vendor_id = strtol(optarg, NULL, 0);
            break;
        case 'V':
            verbose = 1;
            break;
        case '?':
            error("Unknown option: -%c\n", optopt);
            usage();
            exit(1);
        case ':':
            error("Option -%c expects an argument.\n", optopt);
            usage();
            exit(1);
        default:
            usage();
            exit(1);
        }
    }

    int retval;

    // Initialise libusb.
    retval = libusb_init(NULL);
    debug("Init: %s\n", libusb_error_name(retval));

    retval = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    debug("Hotplug: %s\n", libusb_error_name(retval));

    // Check USB IDs.
    if (vendor_id == 0) {
        error("USB Vendor ID is zero. You probably forgot to specify -v.\n");
        exit(1);
    }
    if (vendor_id > 0xffff) {
        error("USB Vendor ID is greater than 16 bits.\n");
        exit(1);
    }
    if (product_id == 0) {
        error("USB Product ID is zero. You probably forgot to specify -p.\n");
        exit(1);
    }
    if (product_id > 0xffff) {
        error("USB Product ID is greater than 16 bits.\n");
        exit(1);
    }

    // Initialise.
    init();

    // Send Interrupt
    char buf[256];

    debug("USB > ");

    while (fgets(buf, 256, stdin) != NULL) {
        unsigned char packet[PKLEN] = {0};

        char * ptr = buf;
        int bytes, size;

        sscanf(ptr, "%u%n", &bytes, &size);

        if (bytes > 64) {
            error("Packet too big!\n");
            continue;
        }

        if (!bytes) {
            debug("No bytes specified, exiting.\n");
            break;
        }

        ptr += size;

        for (int count = 0; count < bytes; count++) {
            sscanf(ptr, " %02hhx%n", &packet[count], &size);
            ptr += size;
        }

        int retval = urb_interrupt(packet, 0);

        // Retry if we lost the device.
        if (retval == LIBUSB_ERROR_NO_DEVICE) {
            init();
            retval = urb_interrupt(packet, 0);
        }

        debug("USB > ");
    }

    //Clean up   
    libusb_release_interface(handle, 1);
    libusb_release_interface(handle, 2);

    //Reattach
    retval = libusb_attach_kernel_driver(handle, 0);
    debug("DrvAttach0: %s\n", libusb_error_name(retval));
    retval = libusb_attach_kernel_driver(handle, 1);
    debug("DrvAttach1: %s\n", libusb_error_name(retval));
    retval = libusb_attach_kernel_driver(handle, 2);
    debug("DrvAttach2: %s\n", libusb_error_name(retval));

    libusb_close(handle);

    libusb_exit(NULL);
    return 0;
}
