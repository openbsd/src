#	$Id: Makefile.inc,v 1.1 1995/12/16 12:55:27 deraadt Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/crypt ${.CURDIR}/crypt

SRCS+=	crypt.c morecrypt.c

MAN+=	crypt.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3
