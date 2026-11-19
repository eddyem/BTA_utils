# run `make DEF=...` to add extra defines
CLIENT := dome_client
SERVER := dome_server
LDFLAGS += -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -pthread -lusefull_macros -lssl -lcrypto
DEFINES := $(DEF) -D_GNU_SOURCE -D_XOPEN_SOURCE=1111
SOBJDIR := mkserver
COBJDIR := mkclient
CFLAGS += -O2 -Wall -Wextra -Wno-trampolines -pthread
COMMSRCS := sslsock.c daemon.c cmdlnopts.c main.c
SSRC := server.c motors.c io.c $(COMMSRCS)
CSRC := client.c bta_shdata.c $(COMMSRCS)
SOBJS := $(addprefix $(SOBJDIR)/, $(SSRC:%.c=%.o))
COBJS := $(addprefix $(COBJDIR)/, $(CSRC:%.c=%.o))
SDEPS := $(SOBJS:.o=.d)
CDEPS := $(COBJS:.o=.d)
CC = gcc

TARGFILE := build.target

ifeq ($(shell test -e $(TARGFILE) && printf yes),yes)
	TARGET := $(file < $(TARGFILE))
else
	TARGET := RELEASE
endif

ifeq ($(TARGET), DEBUG)
	.DEFAULT_GOAL := debug
endif

release: CFLAGS += -flto
release: LDFLAGS += -flto
release: TARGET := RELEASE
release: $(TARGFILE) $(CLIENT) $(SERVER)

debug: CFLAGS += -DEBUG -Werror -g
debug: TARGET := DEBUG
debug: $(TARGFILE) $(CLIENT) $(SERVER)

$(TARGFILE): 
	@echo -e "\tTARGET: $(TARGET)\n"
	@echo "$(TARGET)" > $(TARGFILE)

$(CLIENT) : DEFINES += -DCLIENT
$(CLIENT) : LDFLAGS += -lcrypt
$(CLIENT) : $(COBJDIR) $(COBJS)
	@echo -e "\tLD $(CLIENT)"
	$(CC) $(COBJS) -o $(CLIENT) $(LDFLAGS)


$(SERVER) : DEFINES += -DSERVER
$(SERVER) : LDFLAGS += -lmodbus
$(SERVER) : $(SOBJDIR) $(SOBJS)
	@echo -e "\tLD $(SERVER)"
	$(CC) $(SOBJS) -o $(SERVER) $(LDFLAGS)

$(SOBJDIR):
	@mkdir $(SOBJDIR)

$(COBJDIR):
	@mkdir $(COBJDIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(SDEPS)
-include $(CDEPS)
endif

$(COBJDIR)/%.o: %.c 
	@echo -e "\tCC $<"
	$(CC) -MP -MMD -c $(CFLAGS) $(DEFINES) -o $@ $<

$(SOBJDIR)/%.o: %.c 
	@echo -e "\t\tCC $<"
	$(CC) -MP -MMD -c $(CFLAGS) $(DEFINES) -o $@ $<

clean:
	@echo -e "\t\tCLEAN"
	@rm -rf $(SOBJDIR) $(COBJDIR) $(TARGFILE) 2>/dev/null || true

xclean: clean
	@rm -f $(CLIENT) $(SERVER)

.PHONY: clean xclean $(TARGFILE)
