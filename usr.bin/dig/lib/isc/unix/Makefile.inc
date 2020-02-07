#	$OpenBSD: Makefile.inc,v 1.1 2020/02/07 09:58:54 florian Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c errno.c errno2result.c socket.c stdio.c net.c stdtime.c
SRCS+=	strerror.c time.c
