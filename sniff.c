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
#define IN_EP  0x82
#define OUT_EP  0x03
#define URB_TIMEOUT 100

static struct libusb_device_handle *handle = NULL;
static int product_id = 0x1b2e;

static void init()
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
    printf("DrvDetach0: %d\n", retval);
    retval = libusb_detach_kernel_driver(handle, 1);
    printf("DrvDetach1: %d\n", retval);

    //Set config
    /*retval = libusb_set_configuration(handle, 1);*/
    int bConf = -1;
    retval = libusb_get_configuration(handle, &bConf);
    printf("ConfGet: %d\n", retval);
    printf("bConf: %d\n", bConf);
    if(bConf != 1)
    {
        printf("Setting config to 1");
        retval = libusb_set_configuration(handle, 1);
        printf("ConfSet: %d\n", retval);        
    }
    //Claim Interfaces
    retval = libusb_claim_interface(handle, 1);
    printf("If1Claim: %d\n", retval);

    retval = libusb_claim_interface(handle, 2);
    printf("If2Claim: %d\n", retval);
}

static int urb_interrupt(unsigned char * question, int verbose)
{
    int retval, len;
    unsigned char answer[PKLEN];
    static unsigned char last_answer[PKLEN];

    /*printf("URB Interrupt Question: ");
    for(int i = 0; i < PKLEN; i++)
        printf("%02x ", question[i]);
    printf("\n");*/

    retval = libusb_interrupt_transfer(handle, OUT_EP, question, PKLEN, &len, URB_TIMEOUT);

    if (retval < 0)
    {
        if (verbose)
            fprintf(stderr, "Interrupt write error %d\n", retval);
        return retval;
    }
    retval = libusb_interrupt_transfer(handle, IN_EP, answer, PKLEN, &len, URB_TIMEOUT);

    if (retval < 0)
    {
        if (verbose)
            fprintf(stderr, "Interrupt read error %d\n", retval);
        return retval;
    }

    if (verbose && len < PKLEN)
        fprintf(stderr, "Interrupt transfer short read (%d)\n", retval);

    if (len == 0 && question[0] == 0x07) {
        return 0;
    }

    if(len >= 0)
    {
        if (!memcmp(answer+4, last_answer+4, 56)) {
            /* Nothing changed :( */
            return -1;
        }
        if (verbose) {
            printf("\nURB Interrupt Answer: ");
            for(int i = 0; i < PKLEN; i++)
                printf("%02x ", answer[i]);
            printf("\n");
        }
        memcpy(last_answer, answer, 64);
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
    printf("Init: %d\n", retval);

    retval = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    printf("Hotplug: %d\n", retval);

    // Get Product ID.
    sscanf(argv[1], "%x", &product_id);

    // Initialise.
    init();

    //Send Interrupt
    unsigned char packet[PKLEN] = {0x07, 0x02, 0};

    for (unsigned char arg1 = 0x00; arg1 <= 0xFF; arg1++) {
        for (unsigned char arg2 = 0x00; arg2 <= 0x03; arg2++) {
            packet[2] = arg1;
            packet[3] = arg2;
            printf("\r%02x%02x%02x%02x", packet[0], packet[1], packet[2], packet[3]);
            fflush(stdout);
            retval = urb_interrupt(packet, 0);
            retval = urb_interrupt(packet, 0);
            if (retval == -4) {
                printf("0702%02x%02x seems to do something.\n", arg1, arg2);
                sleep(3);
                init();
            }
        }
    }

    //Clean up   
    libusb_release_interface(handle, 1);
    libusb_release_interface(handle, 2);

    //Reattach
    retval = libusb_attach_kernel_driver(handle, 0);
    printf("DrvAttach0: %d\n", retval);
    retval = libusb_attach_kernel_driver(handle, 1);
    printf("DrvAttach1: %d\n", retval);

    libusb_close(handle);

    libusb_exit(NULL);
    return 0;
}
