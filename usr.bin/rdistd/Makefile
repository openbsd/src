#	$OpenBSD: Makefile,v 1.4 2003/05/06 22:10:11 millert Exp $

PROG=	rdistd
SRCS=	common.c filesys-os.c filesys.c message.c rdistd.c server.c
.PATH:  ${.CURDIR}/../rdist
CFLAGS+=-I${.CURDIR}/../rdist -DOS_H=\"os-openbsd.h\"

.include <bsd.prog.mk>
