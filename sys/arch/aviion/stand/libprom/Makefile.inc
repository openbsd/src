#	$OpenBSD: Makefile.inc,v 1.1 2006/05/16 22:48:18 miod Exp $

LIB_PROM_DIR=${S}/arch/aviion/stand/libprom

LIBPROM_DIR!= cd ${LIB_PROM_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBPROM=${LIBPROM_DIR}/libprom.a

$(LIBPROM):	.NOTMAIN __always_make_libprom
	@echo making sure the libprom is up to date...
	@(cd ${LIB_PROM_DIR}; ${MAKE})

__always_make_libprom:	.NOTMAIN
