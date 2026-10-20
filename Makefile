# USE_AUDIO = 1 activa musica y efectos de sonido. Si da error de compilacion
# por el audio, cambialo a 0 (y el juego compila igual, sin sonido).
USE_AUDIO = 1

EE_BIN = beatemup.elf
EE_OBJS = main.o assets.o

EE_INCS += -I$(PS2DEV)/gsKit/include
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib
EE_LIBS = -lgskit -ldmakit -lpad -lc -lkernel -lm

ifeq ($(USE_AUDIO),1)
EE_OBJS += audio_data.o audsrv_irx.o freesd_irx.o
EE_INCS += -DUSE_AUDIO
EE_LIBS = -lgskit -ldmakit -lpad -laudsrv -lc -lkernel -lm
endif

all: $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

# convierte los modulos de audio de la PS2 (audsrv.irx y freesd.irx) en archivos .c
.RECIPEPREFIX = >
audsrv_irx.c:
> $(PS2SDK)/bin/bin2c $(PS2SDK)/iop/irx/audsrv.irx audsrv_irx.c audsrv_irx
freesd_irx.c:
> $(PS2SDK)/bin/bin2c $(PS2SDK)/iop/irx/freesd.irx freesd_irx.c freesd_irx
.RECIPEPREFIX =
