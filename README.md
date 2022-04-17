# thermostat

This is a controller for my 40 y.o. boiler.

This is based on Sipeed Maix Dock board, Kendryte K210 SoC.

Build and program this demo to flash using kflash[1] tool:

$ make
$ kflash -n -p /dev/ttyUSB0 -t -b 115200 -B dan obj/k210.bin

1. https://github.com/kendryte/kflash.py

To leave the terminal press Ctrl+]
