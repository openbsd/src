# $OpenBSD: Makefile,v 1.37 2016/11/02 15:23:41 schwarze Exp $

.include <bsd.own.mk>		# for NOMAN

POD2MAN=pod2man --official --release="OpenBSD ${OSREV}" --center=OpenSSL

.ifndef NOMAN
MAN=	\
	ASN1_OBJECT_new.3 \
	ASN1_STRING_length.3 \
	ASN1_STRING_new.3 \
	ASN1_STRING_print_ex.3 \
	ASN1_generate_nconf.3 \
	BF_set_key.3 \
	BIO.3 \
	BIO_ctrl.3 \
	BIO_f_base64.3 \
	BIO_f_buffer.3 \
	BIO_f_cipher.3 \
	BIO_f_md.3 \
	BIO_f_null.3 \
	BIO_find_type.3 \
	BIO_new.3 \
	BIO_push.3 \
	BIO_read.3 \
	BIO_s_accept.3 \
	BIO_s_bio.3 \
	BIO_s_connect.3 \
	BIO_s_fd.3 \
	BIO_s_file.3 \
	BIO_s_mem.3 \
	BIO_s_null.3 \
	BIO_s_socket.3 \
	BIO_set_callback.3 \
	BIO_should_retry.3 \
	BN_BLINDING_new.3 \
	BN_CTX_new.3 \
	BN_CTX_start.3 \
	BN_add.3 \
	BN_add_word.3 \
	BN_bn2bin.3 \
	BN_cmp.3 \
	BN_copy.3 \
	BN_generate_prime.3 \
	BN_mod_inverse.3 \
	BN_mod_mul_montgomery.3 \
	BN_mod_mul_reciprocal.3 \
	BN_new.3 \
	BN_num_bytes.3 \
	BN_rand.3 \
	BN_set_bit.3 \
	BN_swap.3 \
	BN_zero.3 \
	BUF_MEM_new.3 \
	CONF_modules_free.3 \
	CONF_modules_load_file.3 \
	CRYPTO_set_ex_data.3 \
	CRYPTO_set_locking_callback.3 \
	DES_set_key.3 \
	DH_generate_key.3 \
	DH_generate_parameters.3 \
	DH_get_ex_new_index.3 \
	DH_new.3 \
	DH_set_method.3 \
	DH_size.3 \
	DSA_SIG_new.3 \
	DSA_do_sign.3 \
	DSA_dup_DH.3 \
	DSA_generate_key.3 \
	DSA_generate_parameters.3 \
	DSA_get_ex_new_index.3 \
	DSA_new.3 \
	DSA_set_method.3 \
	DSA_sign.3 \
	DSA_size.3 \
	EC_GFp_simple_method.3 \
	EC_GROUP_copy.3 \
	EC_GROUP_new.3 \
	EC_KEY_new.3 \
	EC_POINT_add.3 \
	EC_POINT_new.3 \
	ECDSA_SIG_new.3 \
	ERR.3 \
	ERR_GET_LIB.3 \
	ERR_clear_error.3 \
	ERR_error_string.3 \
	ERR_get_error.3 \
	ERR_load_crypto_strings.3 \
	ERR_load_strings.3 \
	ERR_print_errors.3 \
	ERR_put_error.3 \
	ERR_remove_state.3 \
	ERR_set_mark.3 \
	EVP_AEAD_CTX_init.3 \
	UI_new.3 \
	bn_dump.3 \
	crypto.3 \
	d2i_PKCS8PrivateKey_bio.3 \
	des_read_pw.3 \
	lh_new.3 \

GENMAN= \
	EVP_BytesToKey.3 \
	EVP_DigestInit.3 \
	EVP_DigestSignInit.3 \
	EVP_DigestVerifyInit.3 \
	EVP_EncryptInit.3 \
	EVP_OpenInit.3 \
	EVP_PKEY_CTX_ctrl.3 \
	EVP_PKEY_CTX_new.3 \
	EVP_PKEY_cmp.3 \
	EVP_PKEY_decrypt.3 \
	EVP_PKEY_derive.3 \
	EVP_PKEY_encrypt.3 \
	EVP_PKEY_get_default_digest.3 \
	EVP_PKEY_keygen.3 \
	EVP_PKEY_new.3 \
	EVP_PKEY_print_private.3 \
	EVP_PKEY_set1_RSA.3 \
	EVP_PKEY_sign.3 \
	EVP_PKEY_verify.3 \
	EVP_PKEY_verify_recover.3 \
	EVP_SealInit.3 \
	EVP_SignInit.3 \
	EVP_VerifyInit.3 \
	HMAC.3 \
	MD5.3 \
	OBJ_nid2obj.3 \
	OPENSSL_VERSION_NUMBER.3 \
	OPENSSL_config.3 \
	OPENSSL_load_builtin_modules.3 \
	OpenSSL_add_all_algorithms.3 \
	PKCS5_PBKDF2_HMAC.3 \
	PEM_read_bio_PrivateKey.3 \
	PEM_write_bio_PKCS7_stream.3 \
	PKCS12_create.3 \
	PKCS12_parse.3 \
	PKCS7_decrypt.3 \
	PKCS7_encrypt.3 \
	PKCS7_sign.3 \
	PKCS7_sign_add_signer.3 \
	PKCS7_verify.3 \
	RAND.3 \
	RAND_add.3 \
	RAND_bytes.3 \
	RAND_cleanup.3 \
	RAND_load_file.3 \
	RAND_set_rand_method.3 \
	RC4.3 \
	RIPEMD160.3 \
	RSA_blinding_on.3 \
	RSA_check_key.3 \
	RSA_generate_key.3 \
	RSA_get_ex_new_index.3 \
	RSA_new.3 \
	RSA_padding_add_PKCS1_type_1.3 \
	RSA_print.3 \
	RSA_private_encrypt.3 \
	RSA_public_encrypt.3 \
	RSA_set_method.3 \
	RSA_sign.3 \
	RSA_sign_ASN1_OCTET_STRING.3 \
	RSA_size.3 \
	SHA1.3 \
	SMIME_read_PKCS7.3 \
	SMIME_write_PKCS7.3 \
	X509_NAME_ENTRY_get_object.3 \
	X509_NAME_add_entry_by_txt.3 \
	X509_NAME_get_index_by_NID.3 \
	X509_NAME_print_ex.3 \
	X509_STORE_CTX_get_error.3 \
	X509_STORE_CTX_get_ex_new_index.3 \
	X509_STORE_CTX_new.3 \
	X509_STORE_CTX_set_verify_cb.3 \
	X509_STORE_set_verify_cb_func.3 \
	X509_VERIFY_PARAM_set_flags.3 \
	X509_new.3 \
	X509_verify_cert.3 \
	bn.3 \
	d2i_ASN1_OBJECT.3 \
	d2i_DHparams.3 \
	d2i_DSAPublicKey.3 \
	d2i_ECPKParameters.3 \
	d2i_RSAPublicKey.3 \
	d2i_X509.3 \
	d2i_X509_ALGOR.3 \
	d2i_X509_CRL.3 \
	d2i_X509_NAME.3 \
	d2i_X509_REQ.3 \
	d2i_X509_SIG.3 \
	dh.3 \
	dsa.3 \
	ec.3 \
	engine.3 \
	evp.3 \
	i2d_PKCS7_bio_stream.3 \
	lh_stats.3 \
	rsa.3 \
	x509.3 \

MAN+=	${GENMAN}

.include <bsd.man.mk>
.else
maninstall:

.endif

.PATH: ${.CURDIR}/../doc
.SUFFIXES: .pod .1 .3 .7
.for sect in 1 3 7
.pod.${sect}:
	@echo '${POD2MAN} --section=${sect} --name=${*:U} $< > $@'
	@${POD2MAN} --section=${sect} --name=${*:U} $< > $@.tmp && mv $@.tmp $@
.endfor

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

clean cleandir:
	rm -f ${GENMAN}
