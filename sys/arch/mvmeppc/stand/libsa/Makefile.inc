#	$OpenBSD: Makefile.inc,v 1.1 2001/06/26 21:58:06 smurph Exp $

LIB_SA_DIR=${S}/arch/mvmeppc/stand/libsa

LIBSA_DIR!= cd ${LIB_SA_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBSA=${LIBSA_DIR}/libsa.a
SRTOBJ?= ${LIBSA_DIR}/SRT0.o 

$(LIBSA):	.NOTMAIN __always_make_libsa
	@echo making sure the libsa is up to date...
	@(cd ${LIB_SA_DIR}; ${MAKE} "XCFLAGS=${CFLAGS}")

__always_make_libsa:	.NOTMAIN
