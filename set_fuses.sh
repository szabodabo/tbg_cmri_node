/opt/arduino/arduino-1.8.9/hardware/tools/avr/bin/avrdude \
	-V -p m328p -c usbtiny \
	-C /opt/arduino/arduino-1.8.9/hardware/tools/avr/etc/avrdude.conf \
	-U lfuse:w:0xE2:m \
	# -U lfuse:w:0x62:m	# CKDIV 8, default
	-U hfuse:w:0xD9:m \  # CKDIV 1, faster
	-U efuse:w:0xFC:m  # brownout 4.3V
