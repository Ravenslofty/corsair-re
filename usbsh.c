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

#define VENDOR_ID 0x1b1c

#define PKLEN 64
#define URB_TIMEOUT 100

struct libusb_device_handle *handle = NULL;
int product_id = 0x1b2e;

int input_endpoint = 0x82;
int output_endpoint = 0x03;

void init()
{
    int retval;
    //Open device
    handle = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, product_id);

    if (handle == NULL) {
        printf("libusb_open_device_with_vid_pid returned NULL: check your product ID.\n");
        exit(1);
    }

    //Enable auto driver detach
    //libusb_set_auto_detach_kernel_driver(handle, 1);
    //Detach kernel driver:
    retval = libusb_detach_kernel_driver(handle, 0);
    printf("DrvDetach0: %s\n", libusb_error_name(retval));
    retval = libusb_detach_kernel_driver(handle, 1);
    printf("DrvDetach1: %s\n", libusb_error_name(retval));

    //Set config
    int bConf = -1;
    retval = libusb_get_configuration(handle, &bConf);
    printf("ConfGet: %s\n", libusb_error_name(retval));
    printf("bConf: %d\n", bConf);
    if(bConf != 1)
    {
        printf("Setting config to 1");
        retval = libusb_set_configuration(handle, 1);
        printf("ConfSet: %s\n", libusb_error_name(retval));
    }
    //Claim Interfaces
    retval = libusb_claim_interface(handle, 1);
    printf("If1Claim: %s\n", libusb_error_name(retval));

    retval = libusb_claim_interface(handle, 2);
    printf("If2Claim: %s\n", libusb_error_name(retval));

    // Protocol version autodetect
    struct libusb_config_descriptor* config;
    libusb_get_config_descriptor(libusb_get_device(handle), 0, &config);

    printf("Detecting protocol version... ");
    int protover = 2;
    switch (config->bNumInterfaces) {
    case 2: // Version 3 has a HID endpoint, and a Corsair I/O endpoint.
        printf("3 - support is beta.\n");
        input_endpoint = 0x81;
        output_endpoint = 0x02;
        break;
    case 3: // Version 2 has a HID endpoint, a Corsair IN and a Corsair OUT endpoint.
        printf("2\n");
        input_endpoint = 0x82;
        output_endpoint = 0x03;
        break;
    case 4: // Version 1 we aren't quite sure of, but it has four endpoints.
        printf("1 - support is beta.\n");
        input_endpoint = 0x84;
        output_endpoint = 0x04;
        break;
    default:
        printf("unknown - this probably isn't supported.\n");
        libusb_free_config_descriptor(config);
        exit(1);
    }

    printf("Using endpoints %x and %x for communication.\n", input_endpoint, output_endpoint);

    libusb_free_config_descriptor(config);
}

int urb_interrupt(unsigned char * question, int verbose)
{
    int retval, len;
    unsigned char answer[PKLEN];

    printf("URB Interrupt Question: ");
    for(int i = 0; i < PKLEN; i++)
        printf("%02x ", question[i]);
    printf("\n");

    retval = libusb_interrupt_transfer(handle, output_endpoint, question, PKLEN, &len, URB_TIMEOUT);

    if (retval < 0)
    {
        if (verbose)
            fprintf(stderr, "Interrupt write error: %s\n", libusb_error_name(retval));
        return retval;
    }
    retval = libusb_interrupt_transfer(handle, input_endpoint, answer, PKLEN, &len, URB_TIMEOUT);

    if (retval < 0)
    {
        if (verbose)
            fprintf(stderr, "Interrupt read error: %s\n", libusb_error_name(retval));
        return retval;
    }

    if (verbose && len < PKLEN)
        fprintf(stderr, "Interrupt transfer short read (%s)\n", libusb_error_name(retval));

    if (len == 0 && question[0] == 0x07) {
        return 0;
    }

    if(len >= 0)
    {
        if (verbose) {
            printf("\nURB Interrupt Answer: ");
            for(int i = 0; i < PKLEN; i++)
                printf("%02x ", answer[i]);
            printf("\n");
        }
    }
    else
        return -1;

    return 0;
}

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
    printf("Init: %s\n", libusb_error_name(retval));

    retval = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    printf("Hotplug: %s\n", libusb_error_name(retval));

    // Get Product ID.
    sscanf(argv[1], "%x", &product_id);

    // Initialise.
    init();

    //Send Interrupt

    char buf[256];

    printf("USB > ");

    while (fgets(buf, 256, stdin) != NULL) {
        unsigned char packet[PKLEN] = {0};

        char * ptr = buf;
        int bytes, size;

        sscanf(ptr, "%u%n", &bytes, &size);

        if (bytes > 64) {
            printf("Packet too big!\n");
            continue;
        }

        if (!bytes) {
            printf("No bytes specified, exiting.\n");
            break;
        }

        ptr += size;

        for (int count = 0; count < bytes; count++) {
            sscanf(ptr, " %02hhx%n", &packet[count], &size);
            ptr += size;
        }

        int retval = urb_interrupt(packet, 1);

        // Retry if we lost the device.
        if (retval == LIBUSB_ERROR_NO_DEVICE) {
            init();
            retval = urb_interrupt(packet, 1);
        }

        printf("USB > ");
    }

    //Clean up   
    libusb_release_interface(handle, 1);
    libusb_release_interface(handle, 2);

    //Reattach
    retval = libusb_attach_kernel_driver(handle, 0);
    printf("DrvAttach0: %s\n", libusb_error_name(retval));
    retval = libusb_attach_kernel_driver(handle, 1);
    printf("DrvAttach1: %s\n", libusb_error_name(retval));
    retval = libusb_attach_kernel_driver(handle, 2);
    printf("DrvAttach2: %s\n", libusb_error_name(retval));

    libusb_close(handle);

    libusb_exit(NULL);
    return 0;
}
