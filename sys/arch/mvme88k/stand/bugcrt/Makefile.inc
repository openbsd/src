#	$OpenBSD: Makefile.inc,v 1.3 1998/12/15 06:12:50 smurph Exp $

BUG_CRT_DIR=${S}/arch/mvme88k/stand/bugcrt

BUGCRT_DIR!= cd ${BUG_CRT_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

BUGCRT=${BUGCRT_DIR}/bugcrt.o
SDCRT=${BUGCRT_DIR}/sdcrt.o

$(BUGCRT):	.NOTMAIN __always_make_bugcrt
	@echo making sure the bugcrt is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE})

$(SDCRT):	.NOTMAIN __always_make_bugcrt
	@echo making sure the sdcrt is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE})

__always_make_bugcrt:	.NOTMAIN
