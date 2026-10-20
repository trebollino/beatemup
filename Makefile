EE_BIN = beatemup.elf
EE_OBJS = main.o
EE_LIBS = -lgskit -ldmakit -lpad -lc -lkernel -lm

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
