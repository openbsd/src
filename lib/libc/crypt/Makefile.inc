#	$OpenBSD: Makefile.inc,v 1.6 1997/02/13 16:59:53 provos Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/crypt ${.CURDIR}/crypt

SRCS+=	crypt.c morecrypt.c md5crypt.c arc4random.c blowfish.c
SRCS+=  bcrypt.c

MAN+=	crypt.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3 des_cipher.3
MLINKS+=crypt.3 des_setkey.3
