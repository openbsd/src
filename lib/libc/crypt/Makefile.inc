#	$Id: Makefile.inc,v 1.3 1996/05/30 02:36:32 deraadt Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/crypt ${.CURDIR}/crypt

SRCS+=	crypt.c morecrypt.c md5crypt.c md5c.c

MAN+=	crypt.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3 des_cipher.3
MLINKS+=crypt.3 des_setkey.3
