#	$OpenBSD: Makefile,v 1.3 1997/04/27 13:03:11 downsj Exp $

PROG=	rdistd
SRCS=	common.c filesys-os.c filesys.c message.c rdistd.c server.c
.PATH:  ${.CURDIR}/../rdist
CFLAGS+=-I${.CURDIR}/../rdist -DOS_H=\"os-openbsd.h\"

LDADD=  -lcompat
DPADD=  ${LIBCOMPAT}

.include <bsd.prog.mk>
