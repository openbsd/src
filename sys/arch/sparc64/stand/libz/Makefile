#	$OpenBSD: Makefile,v 1.3 2001/09/06 19:12:34 jason Exp $

LIB=	z

.PATH:  ${.CURDIR}/../../../../lib/libz

CLEANFILES+= machine

CFLAGS= -nostdinc ${CEXTRAFLAGS} ${AFLAGS} -O2 -D_ZLIB_PRIVATE -I${.CURDIR}/../../../.. -I${.CURDIR}/../../../../lib/libz -I${.CURDIR}/.

SRCS=	adler32.c crc32.c infblock.c infcodes.c inffast.c \
	inflate.c inftrees.c infutil.c

${OBJS}:	${.CURDIR}/../Makefile.inc

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
.endif

install:

NOPIC=
NOPROFILE=

.include <bsd.lib.mk>
