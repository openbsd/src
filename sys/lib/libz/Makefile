#	$OpenBSD: Makefile,v 1.3 1998/09/08 04:07:46 millert Exp $
#	$NetBSD: Makefile,v 1.2 1997/01/22 01:36:30 cgd Exp $

LIB=	z
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I. ${ZCPPFLAGS} -D_ZLIB_PRIVATE

# files to be copied down from libz.
LIBZSRCS= adler32.c crc32.c infblock.c infcodes.c inffast.c \
        inflate.c inftrees.c infutil.c
LIBZHDRS= infblock.h infcodes.h inffast.h inffixed.h inftrees.h infutil.h \
	zconf.h zlib.h zutil.h

# Other stuff
SRCS=	${LIBZSRCS}

# Files to clean up
CLEANFILES+= lib${LIB}.o

install:

.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `lorder ${OBJS} | tsort`

update-sources:
	for file in ${LIBZSRCS} ${LIBZHDRS}; do \
		cp ${.CURDIR}/../../../lib/libz/$$file ${.CURDIR}; \
	done
