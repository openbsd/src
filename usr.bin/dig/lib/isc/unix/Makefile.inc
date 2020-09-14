#	$OpenBSD: Makefile.inc,v 1.7 2020/09/14 08:39:12 florian Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno2result.c socket.c
