#	$OpenBSD: Makefile.inc,v 1.5 2020/02/24 15:09:14 jsg Exp $

.PATH: ${.CURDIR}/lib/isc

SRCS+=	assertions.c base32.c base64.c netaddr.c buffer.c bufferlist.c
SRCS+=	error.c event.c hash.c heap.c hex.c hmacsha.c
SRCS+=	lex.c log.c regex.c sockaddr.c
SRCS+=	task.c result.c parseint.c refcount.c timer.c
SRCS+=	safe.c serial.c sha1.c sha2.c symtab.c


