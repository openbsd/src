#	$OpenBSD: Makefile.inc,v 1.6 2003/08/16 17:46:57 deraadt Exp $

LIB_SA_DIR=${S}/arch/mvme68k/stand/libsa

LIBSA_DIR!= cd ${LIB_SA_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBSA=${LIBSA_DIR}/libsa.a
SRTOBJ?= ${LIBSA_DIR}/SRT0.o

$(LIBSA):	.NOTMAIN __always_make_libsa
	@echo making sure the libsa is up to date...
	@(cd ${LIB_SA_DIR}; ${MAKE} "XCFLAGS=${CFLAGS}")

__always_make_libsa:	.NOTMAIN
