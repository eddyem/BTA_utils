PROGRAM = bta_meteo_modbus
LDFLAGS = -lcrypt 
SRCS = $(wildcard *.c)
CC = gcc
DEFINES = -D_XOPEN_SOURCE=1111 -D_GNU_SOURCE 
DEFINES += -DEBUG
CXX = gcc
CFLAGS = -std=gnu99 -Wall -Werror -Wextra $(DEFINES) -pthread
OBJS = $(SRCS:.c=.o)
all : $(PROGRAM)
$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

# some addition dependencies
# %.o: %.c
#        $(CC) $(LDFLAGS) $(CFLAGS) $< -o $@
#$(SRCS) : %.c : %.h $(INDEPENDENT_HEADERS)
#        @touch $@

clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(CXX.SRCS)
