#	$Id: Makefile,v 1.2 1996/02/10 21:04:17 tholo Exp $

LIB=	des
SRCS=	cbc_cksm.c cbc_enc.c cbc3_enc.c cfb_enc.c cfb64ede.c cfb64enc.c \
	ecb_enc.c ecb3_enc.c ede_enc.c enc_read.c enc_writ.c fcrypt.c \
	ncbc_enc.c ofb_enc.c ofb64ede.c ofb64enc.c pcbc_enc.c qud_cksm.c \
	rand_key.c read_pwd.c rnd_keys.c rpc_enc.c set_key.c str2key.c supp.c
CFLAGS+=-I${.CURDIR} -DPROTO
MAN=	des_crypt.3
MLINKS+=des_crypt.3 des_read_password.3 des_crypt.3 des_read_2password.3 \
	des_crypt.3 des_string_to_key.3 des_crypt.3 des_string_to_2key.3 \
	des_crypt.3 des_read_pw_string.3 des_crypt.3 des_random_key.3 \
	des_crypt.3 des_set_key_schedule.3 \
	des_crypt.3 des_ecb_encrypt.3 des_crypt.3 des_3ecb_encrypt.3 \
	des_crypt.3 des_cbc_encrypt.3 des_crypt.3 des_3cbc_encrypt.3 \
	des_crypt.3 des_pcbc_encrypt.3 des_crypt.3 des_cfb_encrypt.3 \
	des_crypt.3 des_ofb_encrypt.3 des_crypt.3 des_cbc_cksum.3 \
	des_crypt.3 des_quad_cksum.3 des_crypt.3 des_enc_read.3 \
	des_crypt.3 des_enc_write.3 des_crypt.3 des_set_odd_parity.3 \
	des_crypt.3 des_is_weak_key.3

.include <bsd.lib.mk>
