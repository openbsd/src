#	$OpenBSD: Makefile.inc,v 1.2 2020/02/04 18:45:08 florian Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno.c errno2result.c socket.c stdio.c net.c stdtime.c
SRCS+=	strerror.c time.c
