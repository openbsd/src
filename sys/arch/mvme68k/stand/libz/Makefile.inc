#	$OpenBSD: Makefile.inc,v 1.2 1997/10/20 00:33:47 deraadt Exp $

LIB_Z_DIR=${S}/arch/mvme68k/stand/libz

LIBZ_DIR!= cd ${LIB_Z_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBZ=${LIBZ_DIR}/libz.a

$(LIBZ):	.NOTMAIN __always_make_libz
	@echo making sure the libz is up to date...
	@(cd ${LIB_Z_DIR}; ${MAKE} "XCFLAGS=${CFLAGS}")

__always_make_libz:	.NOTMAIN
