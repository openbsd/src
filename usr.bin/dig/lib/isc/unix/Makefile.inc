#	$OpenBSD: Makefile.inc,v 1.2 2020/02/12 13:05:04 jsg Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno2result.c socket.c stdio.c net.c stdtime.c
SRCS+=	strerror.c time.c
