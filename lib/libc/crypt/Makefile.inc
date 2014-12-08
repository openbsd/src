#	$OpenBSD: Makefile.inc,v 1.26 2014/12/08 20:46:04 tedu Exp $

.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/crypt ${LIBCSRCDIR}/crypt

SRCS+=	crypt.c cryptutil.c arc4random.c arc4random_uniform.c \
	blowfish.c bcrypt.c

MAN+=	crypt.3 crypt_checkpass.3 blowfish.3 arc4random.3
MLINKS+=crypt.3 bcrypt_gensalt.3 crypt.3 bcrypt.3
MLINKS+=crypt_checkpass.3 crypt_newhash.3
MLINKS+=blowfish.3 blf_key.3 blowfish.3 blf_enc.3
MLINKS+=blowfish.3 blf_dec.3 blowfish.3 blf_ecb_encrypt.3
MLINKS+=blowfish.3 blf_ecb_decrypt.3 blowfish.3 blf_cbc_encrypt.3
MLINKS+=blowfish.3 blf_cbc_decrypt.3
MLINKS+=arc4random.3 arc4random_buf.3 arc4random.3 arc4random_uniform.3
