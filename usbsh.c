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

#define PKLEN 64
#define URB_TIMEOUT 100

#define debug(...) do { if (verbose) { printf(__VA_ARGS__); } } while (0)
#define error(...) do { fprintf(stderr, __VA_ARGS__); } while (0)

struct libusb_device_handle *handle = NULL;
long vendor_id = 0;
long product_id = 0;

enum { PROTO_AUTODETECT, PROTO_V1, PROTO_V2, PROTO_V3, PROTO_OVERRIDE };

int protocol = PROTO_AUTODETECT;
long input_endpoint = 0;
long output_endpoint = 0;
int verbose = 0;

void set_endpoints()
{
    if (vendor_id != 0x1b1c) {
        if (input_endpoint == 0) {
            error("Input endpoint is zero and device is not Corsair.\n");
            error("You probably forgot to specify -i.\n");
            exit(1);
        }
        if (output_endpoint == 0) {
            error("Output endpoint is zero and device is not Corsair.\n");
            error("You probably forgot to specify -o.\n");
            exit(1);
        }
        return;
    }

    // Protocol version autodetect
    struct libusb_config_descriptor* config;
    libusb_get_config_descriptor(libusb_get_device(handle), 0, &config);

    if (protocol == PROTO_AUTODETECT) {
        struct libusb_config_descriptor* config;
        libusb_get_config_descriptor(libusb_get_device(handle), 0, &config);

        debug("Detecting protocol version... ");
        switch (config->bNumInterfaces) {
        case 2: // Version 3 has a HID endpoint, and a Corsair I/O endpoint.
            debug("3 - support is beta.\n");
            protocol = PROTO_V3;
            break;
        case 3: // Version 2 has a HID endpoint, a Corsair IN and a Corsair OUT endpoint.
            debug("2\n");
            protocol = PROTO_V2;
            break;
        case 4: // Version 1 we aren't quite sure of, but it has four endpoints.
            debug("1 - support is beta.\n");
            protocol = PROTO_V1;
            break;
        default:
            error("Failed to autodetect protocol; please file a bug.\n");
            libusb_free_config_descriptor(config);
            exit(1);
        }
    }

    if (protocol == PROTO_V3) {
        input_endpoint = 0x81;
        output_endpoint = 0x02;
    } else if (protocol == PROTO_V2) {
        input_endpoint = 0x82;
        output_endpoint = 0x03;
    } else if (protocol == PROTO_V1) {
        input_endpoint = 0x84;
        output_endpoint = 0x04;
    }

    debug("Using endpoints %lx and %lx for communication.\n", input_endpoint, output_endpoint);

    libusb_free_config_descriptor(config);
} 

void init()
{
    int retval;
    //Open device
    handle = libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);

    if (handle == NULL) {
        error("libusb_open_device_with_vid_pid returned NULL: check your product ID.\n");
        exit(1);
    }

    //Enable auto driver detach
    //libusb_set_auto_detach_kernel_driver(handle, 1);
    //Detach kernel driver:
    retval = libusb_detach_kernel_driver(handle, 0);
    debug("DrvDetach0: %s\n", libusb_error_name(retval));
    retval = libusb_detach_kernel_driver(handle, 1);
    debug("DrvDetach1: %s\n", libusb_error_name(retval));

    //Set config
    int bConf = -1;
    retval = libusb_get_configuration(handle, &bConf);
    debug("ConfGet: %s\n", libusb_error_name(retval));
    debug("bConf: %d\n", bConf);
    if(bConf != 1) {
        debug("Setting config to 1");
        retval = libusb_set_configuration(handle, 1);
        debug("ConfSet: %s\n", libusb_error_name(retval));
    }

    // Claim Interfaces
    retval = libusb_claim_interface(handle, 1);
    debug("If1Claim: %s\n", libusb_error_name(retval));

    retval = libusb_claim_interface(handle, 2);
    debug("If2Claim: %s\n", libusb_error_name(retval));

    set_endpoints();
}

int urb_interrupt(unsigned char * question)
{
    int retval, len;
    unsigned char answer[PKLEN];

    debug("URB Interrupt Question: ");
    for(int i = 0; i < PKLEN; i++)
        debug("%02x ", question[i]);
    debug("\n");

    retval = libusb_interrupt_transfer(handle, output_endpoint, question, PKLEN, &len, URB_TIMEOUT);

    if (retval < 0) {
        if (verbose)
            error("Interrupt write error: %s\n", libusb_error_name(retval));
        return retval;
    }
    retval = libusb_interrupt_transfer(handle, input_endpoint, answer, PKLEN, &len, URB_TIMEOUT);

    if (retval < 0) {
        if (verbose)
            error("Interrupt read error: %s\n", libusb_error_name(retval));
        return retval;
    }

    if (verbose && len < PKLEN)
        error("Interrupt transfer short read (%s)\n", libusb_error_name(retval));

    if (protocol != PROTO_OVERRIDE && len == 0 && question[0] == 0x07) {
        return 0;
    }

    if (len >= 0) {
        debug("\nURB Interrupt Answer: ");
        for (int i = 0; i < PKLEN; i++)
            printf("%02x ", answer[i]);
        printf("\n");
        return 0;
    }

    return -1;
}

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

        int retval = urb_interrupt(packet);

        // Retry if we lost the device.
        if (retval == LIBUSB_ERROR_NO_DEVICE) {
            init();
            retval = urb_interrupt(packet);
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
