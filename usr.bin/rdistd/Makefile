#	$OpenBSD: Makefile,v 1.6 2014/07/12 03:32:00 guenther Exp $

PROG=	rdistd
SRCS=	common.c filesys-os.c filesys.c message.c rdistd.c server.c
.PATH:  ${.CURDIR}/../rdist
CFLAGS+=-I${.CURDIR}/../rdist
WARNINGS=yes

.include <bsd.prog.mk>
