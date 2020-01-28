#	$OpenBSD: Makefile.inc,v 1.1 2020/01/28 17:17:06 florian Exp $

.PATH: ${.CURDIR}/lib/isc/unix

SRCS+=	app.c dir.c errno.c errno2result.c socket.c stdio.c net.c stdtime.c
SRCS+=	strerror.c time.c
