#	$OpenBSD: Makefile.inc,v 1.7 1997/02/16 20:58:11 provos Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/crypt ${.CURDIR}/crypt

SRCS+=	crypt.c morecrypt.c md5crypt.c arc4random.c blowfish.c
SRCS+=  bcrypt.c

MAN+=	crypt.3 blowfish.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3 des_cipher.3
MLINKS+=crypt.3 des_setkey.3 blowfish.3 blf_key.3 blowfish.3 blf_enc.3
MLINKS+=blowfish.3 blf_dec.3
