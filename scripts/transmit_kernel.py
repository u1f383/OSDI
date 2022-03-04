#!/usr/bin/python3

with open('/dev/tty.usbserial-0001', "wb", buffering = 0) as tty:
    tty.write(open("../kernel8.img", 'rb').read())