PROGRAM = readwfs
LDFLAGS =
SRCS = $(wildcard *.c)
CC = gcc
DEFINES = -D_XOPEN_SOURCE=1111
CXX = gcc
CFLAGS = -Wall -Werror -Wextra $(DEFINES) -std=gnu99
OBJS = $(SRCS:.c=.o)
all : $(PROGRAM)
$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

# some addition dependencies
# %.o: %.c
#        $(CC) $(LDFLAGS) $(CFLAGS) $< -o $@
$(SRCS) : %.c : %.h
	touch $@

%.h: ;

clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(CXX.SRCS)
