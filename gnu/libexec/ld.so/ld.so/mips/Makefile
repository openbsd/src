.include "../../Version.mk"
.include "../../Config.mk"

CC = $(ELFCC)
AS = $(ELFAS)
LD = $(ELFLD)

CFLAGS += -I.. -I. -DNO_UNDERSCORE -DVERBOSE_DLINKER
CFLAGS += -mabicalls -G0 -fPIC -D__PIC__ #-DDEBUG # -funroll-loops

../%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@
../%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

SRC1S = resolve.S
SRC2S = elfinterp.c

SRCS	= $(SRC1S) $(SRC2S)
OBJ1S	= $(SRC1S:.S=.o)
OBJ2S	= $(SRC2S:.c=.o)
OBJS	= $(OBJ1S) $(OBJ2S)

DLINK_OBJS:= $(addprefix ../, $(OBJS))

lib:: $(DLINK_OBJS)

obj: $(DLINK_OBJS)

asm: $(ASMS)

realclean::
	$(RM) -f .depend core *.o *.a *.s *.i tmp_make *~

clean::
	$(RM) -f core *.o *.a *.s *.i tmp_make *~

depend::
	$(CC) $(CFLAGS) -M $(SRCS) | \
		sed -e 's,^[ 	]*\(.*\.o\)[ 	]*:,../\1:,' > .depend
#	$(MAKE) subdir TARGET=depend

#
# include a dependency file if one exists
#
#ifeq (.depend,$(wildcard .depend))
#include .depend
#endif

