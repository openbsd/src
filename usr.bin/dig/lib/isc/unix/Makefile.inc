#	$OpenBSD: Makefile.inc,v 1.3 2020/02/13 21:34:06 jung Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno2result.c socket.c stdio.c net.c stdtime.c time.c
