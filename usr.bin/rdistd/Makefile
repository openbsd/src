#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:38:20 deraadt Exp $

PROG=	rdistd
SRCS=	common.c filesys-os.c filesys.c message.c rdistd.c server.c setargs.c
.PATH:  ${.CURDIR}/../rdist
CFLAGS+=-I${.CURDIR}/../rdist -DOS_H=\"os-openbsd.h\"

LDADD=  -lcompat
DPADD=  ${LIBCOMPAT}

.include <bsd.prog.mk>
