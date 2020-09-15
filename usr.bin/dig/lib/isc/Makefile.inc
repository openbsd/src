#	$OpenBSD: Makefile.inc,v 1.8 2020/09/15 08:19:29 florian Exp $

.PATH: ${.CURDIR}/lib/isc

SRCS+=	assertions.c base32.c base64.c buffer.c bufferlist.c
SRCS+=	error.c event.c hash.c heap.c hex.c hmacsha.c
SRCS+=	lex.c log.c regex.c sockaddr.c
SRCS+=	task.c result.c refcount.c timer.c
SRCS+=	serial.c sha1.c sha2.c symtab.c


