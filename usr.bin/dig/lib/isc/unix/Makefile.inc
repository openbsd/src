#	$OpenBSD: Makefile.inc,v 1.5 2020/02/17 19:45:00 jung Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno2result.c socket.c net.c time.c
