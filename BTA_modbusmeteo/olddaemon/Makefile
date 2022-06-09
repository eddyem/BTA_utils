CFLAGS=-g

bta_meteo_can: bta_meteo_can.c bta_shdata.h CAN/can_io.h CAN/can_io.o
	cc -g bta_meteo_can.c CAN/can_io.o -lcrypt -o bta_meteo_can
#	setroot bta_meteo_can

CAN/can_io.o: CAN/can_io.c CAN/can_io.h CAN/can4linux.h
