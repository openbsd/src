#	$OpenBSD: Makefile.inc,v 1.13 1999/11/17 05:22:36 millert Exp $

.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/crypt ${LIBCSRCDIR}/crypt

SRCS+=	cast.c crypt.c morecrypt.c md5crypt.c arc4random.c blowfish.c
SRCS+=  bcrypt.c skipjack.c

MAN+=	crypt.3 blowfish.3 arc4random.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3 des_cipher.3
MLINKS+=crypt.3 des_setkey.3 blowfish.3 blf_key.3 blowfish.3 blf_enc.3
MLINKS+=blowfish.3 blf_dec.3 blowfish.3 blf_ecb_encrypt.3
MLINKS+=blowfish.3 blf_ecb_decrypt.3 blowfish.3 blf_cbc_encrypt.3
MLINKS+=blowfish.3 blf_cbc_decrypt.3
MLINKS+=arc4random.3 arc4random_stir.3 arc4random.3 arc4random_addrandom.3
