#	$OpenBSD: Makefile.inc,v 1.1 2001/06/26 21:58:02 smurph Exp $

BUG_CRT_DIR=${S}/arch/mvmeppc/stand/bugcrt

BUGCRT_DIR!= cd ${BUG_CRT_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

STAGE1=${BUGCRT_DIR}/stage1crt.o
STAGE2=${BUGCRT_DIR}/stage2crt.o
SINGLE=${BUGCRT_DIR}/singlecrt.o

$(STAGE1):	.NOTMAIN __always_make_bugcrt
	@echo making sure the stage1crt.o is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE} stage1crt.o)

$(STAGE2):	.NOTMAIN __always_make_bugcrt
	@echo making sure the stage2crt.o is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE} stage2crt.o)

$(SINGLE):	.NOTMAIN __always_make_bugcrt
	@echo making sure the singlecrt.o is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE} singlecrt.o)

__always_make_bugcrt:	.NOTMAIN
