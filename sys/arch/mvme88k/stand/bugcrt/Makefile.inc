#	$OpenBSD: Makefile.inc,v 1.5 2006/05/16 22:51:30 miod Exp $

BUG_CRT_DIR=${S}/arch/mvme88k/stand/bugcrt

BUGCRT_DIR!= cd ${BUG_CRT_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

STAGE1=${BUGCRT_DIR}/stage1crt.o
STAGE2=${BUGCRT_DIR}/stage2crt.o
SINGLE=${BUGCRT_DIR}/singlecrt.o

$(STAGE1):	.NOTMAIN __always_make_bugcrt
	@echo making sure stage1crt.o is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE} stage1crt.o)

$(STAGE2):	.NOTMAIN __always_make_bugcrt
	@echo making sure stage2crt.o is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE} stage2crt.o)

$(SINGLE):	.NOTMAIN __always_make_bugcrt
	@echo making sure singlecrt.o is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE} singlecrt.o)

__always_make_bugcrt:	.NOTMAIN
