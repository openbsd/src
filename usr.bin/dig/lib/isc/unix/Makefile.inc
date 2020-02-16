#	$OpenBSD: Makefile.inc,v 1.4 2020/02/16 21:12:41 florian Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno2result.c socket.c stdio.c net.c time.c
