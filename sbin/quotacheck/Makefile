#	$OpenBSD: Makefile,v 1.6 1997/09/21 11:37:57 deraadt Exp $

PROG=	quotacheck
SRCS=	quotacheck.c preen.c fsutil.c
CFLAGS+=-I${.CURDIR}/../fsck
MAN=	quotacheck.8
.PATH:	${.CURDIR}/../fsck

.include <bsd.prog.mk>
