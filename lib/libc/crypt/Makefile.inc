#	$OpenBSD: Makefile.inc,v 1.23 2014/11/18 22:17:50 jmc Exp $

.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/crypt ${LIBCSRCDIR}/crypt

SRCS+=	crypt.c crypt2.c cryptutil.c arc4random.c arc4random_uniform.c \
	blowfish.c bcrypt.c

MAN+=	crypt.3 blowfish.3 arc4random.3
MLINKS+=crypt.3 setkey.3 crypt.3 crypt_checkpass.3
MLINKS+=crypt.3 crypt_newhash.3 crypt.3 encrypt.3
MLINKS+=crypt.3 des_setkey.3 crypt.3 des_cipher.3
MLINKS+=crypt.3 bcrypt_gensalt.3 crypt.3 bcrypt.3
MLINKS+=blowfish.3 blf_key.3 blowfish.3 blf_enc.3
MLINKS+=blowfish.3 blf_dec.3 blowfish.3 blf_ecb_encrypt.3
MLINKS+=blowfish.3 blf_ecb_decrypt.3 blowfish.3 blf_cbc_encrypt.3
MLINKS+=blowfish.3 blf_cbc_decrypt.3
MLINKS+=arc4random.3 arc4random_buf.3 arc4random.3 arc4random_uniform.3
