LOADLIBES = -lm -lcrypt -lsla
SRCS = bta_json.c bta_print.c daemon.c
CC = gcc
#DEFINES = -DEBUG
CXX = gcc
CPPFLAGS = -Wall -Werror $(DEFINES)
OBJS = $(SRCS:.c=.o)
all : bta_json client_streaming
$(OBJS): bta_json.h bta_shdata.h
bta_json : $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS)  $(LOADLIBES) -o bta_json
client_streaming: client_streaming.o
	$(CC) $(CPPFLAGS) -lm -ljson client_streaming.o -o client_streaming
clean:
	/bin/rm -f *.o *~

