#	$OpenBSD: Makefile.inc,v 1.3 1997/10/20 00:33:45 deraadt Exp $

BUG_CRT_DIR=${S}/arch/mvme68k/stand/bugcrt

BUGCRT_DIR!= cd ${BUG_CRT_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

BUGCRT=${BUGCRT_DIR}/bugcrt.o

$(BUGCRT):	.NOTMAIN __always_make_bugcrt
	@echo making sure the bugcrt is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE})

__always_make_bugcrt:	.NOTMAIN
