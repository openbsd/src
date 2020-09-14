#	$OpenBSD: Makefile.inc,v 1.6 2020/09/14 08:37:09 florian Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno2result.c socket.c net.c
