/opt/arduino/arduino-1.8.9/hardware/tools/avr/bin/avrdude \
	-V -p m328p -c usbtiny \
	-C /opt/arduino/arduino-1.8.9/hardware/tools/avr/etc/avrdude.conf \
	-U lfuse:w:0xE2:m