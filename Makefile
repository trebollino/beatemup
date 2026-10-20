# USE_AUDIO = 1 activa musica y efectos de sonido. Si da error de compilacion
# por el audio, cambialo a 0 (y el juego compila igual, sin sonido).
USE_AUDIO = 1

EE_BIN = beatemup.elf
EE_OBJS = main.o assets.o

EE_INCS += -I$(PS2DEV)/gsKit/include
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib
EE_LIBS = -lgskit -ldmakit -lpad -lc -lkernel -lm

ifeq ($(USE_AUDIO),1)
EE_OBJS += audio_data.o audsrv_irx.o
EE_INCS += -DUSE_AUDIO
EE_LIBS = -lgskit -ldmakit -lpad -laudsrv -lc -lkernel -lm
endif

all: $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

# convierte el modulo de audio de la PS2 (audsrv.irx) en un archivo .c
.RECIPEPREFIX = >
audsrv_irx.c:
> $(PS2SDK)/bin/bin2c $(PS2SDK)/iop/irx/audsrv.irx audsrv_irx.c audsrv_irx
.RECIPEPREFIX =
