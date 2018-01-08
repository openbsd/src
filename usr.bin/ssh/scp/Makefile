#	$OpenBSD: Makefile,v 1.19 2018/01/08 15:21:49 markus Exp $

.PATH:		${.CURDIR}/..

SRCS=	scp.c
SRCS+=	atomicio.c cleanup.c fatal.c log.c misc.c progressmeter.c \
	sshbuf-getput-basic.c sshbuf.c ssherr.c utf8.c xmalloc.c

PROG=	scp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>
