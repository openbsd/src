#	$OpenBSD: Makefile.inc,v 1.4 1996/06/03 22:20:33 niklas Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/crypt ${.CURDIR}/crypt

SRCS+=	crypt.c morecrypt.c md5crypt.c

MAN+=	crypt.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3 des_cipher.3
MLINKS+=crypt.3 des_setkey.3
