#	$Id: Makefile.inc,v 1.2 1996/03/25 22:31:44 tholo Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/crypt ${.CURDIR}/crypt

SRCS+=	crypt.c morecrypt.c

MAN+=	crypt.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3 des_cipher.3
MLINKS+=crypt.3 des_setkey.3
