#	$OpenBSD: Makefile.inc,v 1.3 1996/05/16 02:30:38 chuck Exp $

LIB_SA_DIR=${S}/arch/${MACHINE}/stand/libsa

LIBSA_DIR!= cd ${LIB_SA_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBSA=${LIBSA_DIR}/libsa.a
SRTOBJ?= ${LIBSA_DIR}/SRT0.o 

$(LIBSA):	.NOTMAIN __always_make_libsa
	@echo making sure the libsa is up to date...
	@(cd ${LIB_SA_DIR}; ${MAKE})

__always_make_libsa:	.NOTMAIN
