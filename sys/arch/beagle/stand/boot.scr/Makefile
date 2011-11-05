PROG = boot.scr
SRCS = 
NOMAN=

.SUFFIXES: .cmd .scr

.cmd.scr:
	mkuboot -t script -a arm -o linux $< $@


.include <bsd.prog.mk>
