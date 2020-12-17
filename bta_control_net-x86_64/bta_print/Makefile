# run `make DEF=...` to add extra defines
PROGRAM := bta_print
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all -lcrypt -lm
SRCS := bta_print.c bta_shdata.c
DEFINES := $(DEF) -D_GNU_SOURCE -D_XOPEN_SOURCE=1111
CFLAGS += -O2 -Wall -Werror -Wextra -Wno-trampolines -std=gnu99
CC = gcc
#CXX = g++

all : $(PROGRAM)

$(PROGRAM) : $(SRCS)
	$(CC) $(DEFINES) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $(PROGRAM)

