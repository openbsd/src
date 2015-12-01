#	$OpenBSD: Makefile,v 1.19 2015/12/01 07:50:07 deraadt Exp $

PROG=	kbd

SRCS=	main.c kbd_wscons.c
CFLAGS+=-DNOKVM
.PATH:  ${.CURDIR}/../../../sbin/kbd

.include <bsd.prog.mk>
