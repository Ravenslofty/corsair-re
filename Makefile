all:
	gcc -o sniff -O3 sniff.c -lusb-1.0
	gcc -o usbsh -Wall -O3 usbsh.c -lusb-1.0
