BUG_CRT_DIR=${S}/arch/${MACHINE}/stand/bugcrt

BUGCRT_DIR!= cd ${BUG_CRT_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

BUGCRT=${BUGCRT_DIR}/bugcrt.o

$(BUGCRT):	.NOTMAIN __always_make_bugcrt
	@echo making sure the bugcrt is up to date...
	@(cd ${BUG_CRT_DIR}; ${MAKE})

__always_make_bugcrt:	.NOTMAIN
