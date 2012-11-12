#	$OpenBSD: Makefile,v 1.5 2012/11/12 01:14:41 guenther Exp $

PROG=	rdistd
SRCS=	common.c filesys-os.c filesys.c message.c rdistd.c server.c
.PATH:  ${.CURDIR}/../rdist
CFLAGS+=-I${.CURDIR}/../rdist -DOS_H=\"os-openbsd.h\"
WARNINGS=yes

.include <bsd.prog.mk>
