#	$OpenBSD: Makefile.inc,v 1.1 2001/06/26 21:58:09 smurph Exp $

LIB_Z_DIR=${S}/arch/mvmeppc/stand/libz

LIBZ_DIR!= cd ${LIB_Z_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBZ=${LIBZ_DIR}/libz.a

$(LIBZ):	.NOTMAIN __always_make_libz
	@echo making sure the libz is up to date...
	@(cd ${LIB_Z_DIR}; ${MAKE} "XCFLAGS=${CFLAGS}")

__always_make_libz:	.NOTMAIN
