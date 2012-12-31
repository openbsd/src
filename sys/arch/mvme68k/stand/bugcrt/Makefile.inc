#	$OpenBSD: Makefile.inc,v 1.4 2012/12/31 21:35:32 miod Exp $

BUG_CRT_DIR=${S}/arch/mvme68k/stand/bugcrt

BUGCRT_DIR!= cd ${BUG_CRT_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

BUGCRT=${BUGCRT_DIR}/bugcrtS.o ${BUGCRT_DIR}/bugcrt.o

$(BUGCRT):	.NOTMAIN __always_make_bugcrt
	@echo making sure the bugcrt is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE})

__always_make_bugcrt:	.NOTMAIN
