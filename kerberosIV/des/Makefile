#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:44 tholo Exp $

LIB=	des
SRCS=	3cbc_enc.c 3ecb_enc.c cbc_cksm.c cbc_enc.c cfb_enc.c \
	ecb_enc.c enc_read.c enc_writ.c key_par.c ofb_enc.c \
	pcbc_enc.c qud_cksm.c random_key.c read_pwd.c rnd_keys.c \
	set_key.c str2key.c
CFLAGS+=-I${.CURDIR}
MAN=	des_crypt.3
MLINKS+=des_crypt.3 des_read_password.3 des_crypt.3 des_string_to_key.3 \
	des_crypt.3 des_random_key.3 des_crypt.3 des_set_key.3 \
	des_crypt.3 des_ecb_encrypt.3 des_crypt.3 des_cbc_encrypt.3 \
	des_crypt.3 des_pcbc_encrypt.3 des_crypt.3 des_cbc_cksum.3 \
	des_crypt.3 des_quad_cksum.3

.include <bsd.lib.mk>
