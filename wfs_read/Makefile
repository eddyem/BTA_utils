# run `make DEF=...` to add extra defines
PROGRAM = readwfs
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS = -lm -lpng -lcfitsio
SRCS = $(wildcard *.c)
DEFINES = $(DEF) -D_XOPEN_SOURCE=1111
DEFINES += -DEBUG
CFLAGS = -O2 -fopenmp  -Wall -Werror -Wextra -std=gnu99 -Wno-trampolines
OBJDIR = mk
OBJS = $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.o))
DEPS := $(OBJS:.o=.d)
CC = gcc

all : $(OBJDIR) $(PROGRAM)

$(PROGRAM) : $(OBJS)
	@echo -e "\t\tLD $(PROGRAM)"
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(PROGRAM)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) -MD -c $(CFLAGS) $(DEFINES) -o $@ $<

clean:
	@echo -e "\t\tCLEAN"
	@rm -f $(OBJS)
	@rm -f $(DEPS)
	@rmdir $(OBJDIR) 2>/dev/null || true

xclean: clean
	@rm -f $(PROGRAM)

gentags:
	CFLAGS="$(CFLAGS) $(DEFINES)" geany -g readwfs.c.tags *[hc] 2>/dev/null

.PHONY: gentags clean xclean

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

