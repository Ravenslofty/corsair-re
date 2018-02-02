all:
	gcc -o usbsh -Wall -O3 usbsh.c -lusb-1.0
	sudo ./snoop.py > output.txt
