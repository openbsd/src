PROG = boot.scr
SRCS = 

.SUFFIXES: .cmd .scr

.cmd.scr:
	mkuboot -t script -a arm -o linux $< $@


.include <bsd.prog.mk>
