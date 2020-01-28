#	$OpenBSD: Makefile.inc,v 1.1 2020/01/28 17:17:06 florian Exp $

.PATH: ${.CURDIR}/lib/isc

SRCS+=	aes.c assertions.c base32.c base64.c netaddr.c buffer.c bufferlist.c
SRCS+=	commandline.c error.c event.c hash.c heap.c hex.c hmacmd5.c hmacsha.c
SRCS+=	inet_aton.c iterated_hash.c lex.c log.c md5.c regex.c sockaddr.c
SRCS+=	task.c result.c netscope.c parseint.c refcount.c region.c timer.c
SRCS+=	safe.c serial.c sha1.c sha2.c symtab.c


