#	$OpenBSD: Makefile,v 1.2 1997/03/31 23:06:33 mickey Exp $

CFLAGS	+=$(SACFLAGS)
.include "../Makefile.inc"

S	= ${.CURDIR}/../../../..
ZDST=${.OBJDIR}
.include "${S}/lib/libz/Makefile.inc"

all: ${ZLIB}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>

