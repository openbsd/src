# $OpenBSD: Makefile,v 1.19 2015/02/23 17:43:24 schwarze Exp $

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

GENMAN= \
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
	bn_internal.3 \
	crypto.3 \
	d2i_ASN1_OBJECT.3 \
	d2i_DHparams.3 \
	d2i_DSAPublicKey.3 \
	d2i_ECPKParameters.3 \
	d2i_PKCS8PrivateKey.3 \
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
	ecdsa.3 \
	engine.3 \
	evp.3 \
	i2d_PKCS7_bio_stream.3 \
	lh_stats.3 \
	lhash.3 \
	rsa.3 \
	ui.3 \
	ui_compat.3 \
	x509.3 \

MAN+=	${GENMAN}

#MAN+=	BIO_new_CMS.3 \
#	CMS_add0_cert.3 \
#	CMS_add1_recipient_cert.3 \
#	CMS_compress.3 \
#	CMS_decrypt.3 \
#	CMS_encrypt.3 \
#	CMS_final.3 \
#	CMS_get0_RecipientInfos.3 \
#	CMS_get0_SignerInfos.3 \
#	CMS_get0_type.3 \
#	CMS_get1_ReceiptRequest.3 \
#	CMS_sign.3 \
#	CMS_sign_add1_signer.3 \
#	CMS_sign_receipt.3 \
#	CMS_uncompress.3 \
#	CMS_verify.3 \
#	CMS_verify_receipt.3 \
#	PEM_write_bio_CMS_stream.3 \
#	SMIME_read_CMS.3 \
#	SMIME_write_CMS.3 \
#	i2d_CMS_bio_stream.3 \

# XXX PLEASE LEAVE THESE AS TWO PER LINE. DO NOT WRAP.
# XXX SCRIPTS IN PORTABLE DEPEND ON THIS.
MLINKS+=\
	ASN1_OBJECT_new.3 ASN1_OBJECT_free.3 \
	ASN1_STRING_length.3 ASN1_STRING_cmp.3 \
	ASN1_STRING_length.3 ASN1_STRING_data.3 \
	ASN1_STRING_length.3 ASN1_STRING_dup.3 \
	ASN1_STRING_length.3 ASN1_STRING_length_set.3 \
	ASN1_STRING_length.3 ASN1_STRING_set.3 \
	ASN1_STRING_length.3 ASN1_STRING_to_UTF8.3 \
	ASN1_STRING_length.3 ASN1_STRING_type.3 \
	ASN1_STRING_new.3 ASN1_STRING_free.3 \
	ASN1_STRING_new.3 ASN1_STRING_type_new.3 \
	ASN1_STRING_print_ex.3 ASN1_STRING_print.3 \
	ASN1_STRING_print_ex.3 ASN1_STRING_print_ex_fp.3 \
	ASN1_generate_nconf.3 ASN1_generate_v3.3 \
	BF_set_key.3 BF_cbc_encrypt.3 \
	BF_set_key.3 BF_cfb64_encrypt.3 \
	BF_set_key.3 BF_decrypt.3 \
	BF_set_key.3 BF_ecb_encrypt.3 \
	BF_set_key.3 BF_encrypt.3 \
	BF_set_key.3 BF_ofb64_encrypt.3 \
	BF_set_key.3 BF_options.3 \
	BIO_ctrl.3 BIO_callback_ctrl.3 \
	BIO_ctrl.3 BIO_ctrl_pending.3 \
	BIO_ctrl.3 BIO_ctrl_wpending.3 \
	BIO_ctrl.3 BIO_eof.3 \
	BIO_ctrl.3 BIO_flush.3 \
	BIO_ctrl.3 BIO_get_close.3 \
	BIO_ctrl.3 BIO_get_info_callback.3 \
	BIO_ctrl.3 BIO_int_ctrl.3 \
	BIO_ctrl.3 BIO_pending.3 \
	BIO_ctrl.3 BIO_ptr_ctrl.3 \
	BIO_ctrl.3 BIO_reset.3 \
	BIO_ctrl.3 BIO_seek.3 \
	BIO_ctrl.3 BIO_set_close.3 \
	BIO_ctrl.3 BIO_set_info_callback.3 \
	BIO_ctrl.3 BIO_tell.3 \
	BIO_ctrl.3 BIO_wpending.3 \
	BIO_f_buffer.3 BIO_get_buffer_num_lines.3 \
	BIO_f_buffer.3 BIO_set_buffer_read_data.3 \
	BIO_f_buffer.3 BIO_set_buffer_size.3 \
	BIO_f_buffer.3 BIO_set_read_buffer_size.3 \
	BIO_f_buffer.3 BIO_set_write_buffer_size.3 \
	BIO_f_cipher.3 BIO_get_cipher_ctx.3 \
	BIO_f_cipher.3 BIO_get_cipher_status.3 \
	BIO_f_cipher.3 BIO_set_cipher.3 \
	BIO_f_md.3 BIO_get_md.3 \
	BIO_f_md.3 BIO_get_md_ctx.3 \
	BIO_f_md.3 BIO_set_md.3 \
	BIO_find_type.3 BIO_method_type.3 \
	BIO_find_type.3 BIO_next.3 \
	BIO_new.3 BIO_free.3 \
	BIO_new.3 BIO_free_all.3 \
	BIO_new.3 BIO_set.3 \
	BIO_new.3 BIO_vfree.3 \
	BIO_push.3 BIO_pop.3 \
	BIO_read.3 BIO_gets.3 \
	BIO_read.3 BIO_puts.3 \
	BIO_read.3 BIO_write.3 \
	BIO_s_accept.3 BIO_do_accept.3 \
	BIO_s_accept.3 BIO_get_accept_port.3 \
	BIO_s_accept.3 BIO_get_bind_mode.3 \
	BIO_s_accept.3 BIO_new_accept.3 \
	BIO_s_accept.3 BIO_set_accept_bios.3 \
	BIO_s_accept.3 BIO_set_accept_port.3 \
	BIO_s_accept.3 BIO_set_bind_mode.3 \
	BIO_s_accept.3 BIO_set_nbio_accept.3 \
	BIO_s_bio.3 BIO_ctrl_get_read_request.3 \
	BIO_s_bio.3 BIO_ctrl_get_write_guarantee.3 \
	BIO_s_bio.3 BIO_ctrl_reset_read_request.3 \
	BIO_s_bio.3 BIO_destroy_bio_pair.3 \
	BIO_s_bio.3 BIO_get_read_request.3 \
	BIO_s_bio.3 BIO_get_write_buf_size.3 \
	BIO_s_bio.3 BIO_get_write_guarantee.3 \
	BIO_s_bio.3 BIO_make_bio_pair.3 \
	BIO_s_bio.3 BIO_new_bio_pair.3 \
	BIO_s_bio.3 BIO_set_write_buf_size.3 \
	BIO_s_bio.3 BIO_shutdown_wr.3 \
	BIO_s_connect.3 BIO_do_connect.3 \
	BIO_s_connect.3 BIO_get_conn_hostname.3 \
	BIO_s_connect.3 BIO_get_conn_int_port.3 \
	BIO_s_connect.3 BIO_get_conn_ip.3 \
	BIO_s_connect.3 BIO_get_conn_port.3 \
	BIO_s_connect.3 BIO_new_connect.3 \
	BIO_s_connect.3 BIO_set_conn_hostname.3 \
	BIO_s_connect.3 BIO_set_conn_int_port.3 \
	BIO_s_connect.3 BIO_set_conn_ip.3 \
	BIO_s_connect.3 BIO_set_conn_port.3 \
	BIO_s_connect.3 BIO_set_nbio.3 \
	BIO_s_fd.3 BIO_get_fd.3 \
	BIO_s_fd.3 BIO_new_fd.3 \
	BIO_s_fd.3 BIO_set_fd.3 \
	BIO_s_file.3 BIO_append_filename.3 \
	BIO_s_file.3 BIO_get_fp.3 \
	BIO_s_file.3 BIO_new_file.3 \
	BIO_s_file.3 BIO_new_fp.3 \
	BIO_s_file.3 BIO_read_filename.3 \
	BIO_s_file.3 BIO_rw_filename.3 \
	BIO_s_file.3 BIO_set_fp.3 \
	BIO_s_file.3 BIO_write_filename.3 \
	BIO_s_mem.3 BIO_get_mem_data.3 \
	BIO_s_mem.3 BIO_get_mem_ptr.3 \
	BIO_s_mem.3 BIO_new_mem_buf.3 \
	BIO_s_mem.3 BIO_set_mem_buf.3 \
	BIO_s_mem.3 BIO_set_mem_eof_return.3 \
	BIO_s_socket.3 BIO_new_socket.3 \
	BIO_set_callback.3 BIO_debug_callback.3 \
	BIO_set_callback.3 BIO_get_callback.3 \
	BIO_set_callback.3 BIO_get_callback_arg.3 \
	BIO_set_callback.3 BIO_set_callback_arg.3 \
	BIO_should_retry.3 BIO_get_retry_BIO.3 \
	BIO_should_retry.3 BIO_get_retry_reason.3 \
	BIO_should_retry.3 BIO_retry_type.3 \
	BIO_should_retry.3 BIO_should_io_special.3 \
	BIO_should_retry.3 BIO_should_read.3 \
	BIO_should_retry.3 BIO_should_write.3 \
	BN_BLINDING_new.3 BN_BLINDING_convert.3 \
	BN_BLINDING_new.3 BN_BLINDING_convert_ex.3 \
	BN_BLINDING_new.3 BN_BLINDING_create_param.3 \
	BN_BLINDING_new.3 BN_BLINDING_free.3 \
	BN_BLINDING_new.3 BN_BLINDING_get_thread_id.3 \
	BN_BLINDING_new.3 BN_BLINDING_invert.3 \
	BN_BLINDING_new.3 BN_BLINDING_invert_ex.3 \
	BN_BLINDING_new.3 BN_BLINDING_get_flags.3 \
	BN_BLINDING_new.3 BN_BLINDING_set_flags.3 \
	BN_BLINDING_new.3 BN_BLINDING_set_thread_id.3 \
	BN_BLINDING_new.3 BN_BLINDING_thread_id.3 \
	BN_BLINDING_new.3 BN_BLINDING_update.3 \
	BN_CTX_new.3 BN_CTX_free.3 \
	BN_CTX_new.3 BN_CTX_init.3 \
	BN_CTX_start.3 BN_CTX_end.3 \
	BN_CTX_start.3 BN_CTX_get.3 \
	BN_add.3 BN_div.3 \
	BN_add.3 BN_exp.3 \
	BN_add.3 BN_gcd.3 \
	BN_add.3 BN_mod.3 \
	BN_add.3 BN_mod_add.3 \
	BN_add.3 BN_mod_exp.3 \
	BN_add.3 BN_mod_mul.3 \
	BN_add.3 BN_mod_sqr.3 \
	BN_add.3 BN_mod_sub.3 \
	BN_add.3 BN_mul.3 \
	BN_add.3 BN_nnmod.3 \
	BN_add.3 BN_sqr.3 \
	BN_add.3 BN_sub.3 \
	BN_add_word.3 BN_div_word.3 \
	BN_add_word.3 BN_mod_word.3 \
	BN_add_word.3 BN_mul_word.3 \
	BN_add_word.3 BN_sub_word.3 \
	BN_bn2bin.3 BN_bin2bn.3 \
	BN_bn2bin.3 BN_bn2dec.3 \
	BN_bn2bin.3 BN_bn2hex.3 \
	BN_bn2bin.3 BN_bn2mpi.3 \
	BN_bn2bin.3 BN_dec2bn.3 \
	BN_bn2bin.3 BN_hex2bn.3 \
	BN_bn2bin.3 BN_mpi2bn.3 \
	BN_bn2bin.3 BN_print.3 \
	BN_bn2bin.3 BN_print_fp.3 \
	BN_cmp.3 BN_is_odd.3 \
	BN_cmp.3 BN_is_one.3 \
	BN_cmp.3 BN_is_word.3 \
	BN_cmp.3 BN_is_zero.3 \
	BN_cmp.3 BN_ucmp.3 \
	BN_copy.3 BN_dup.3 \
	BN_generate_prime.3 BN_GENCB_call.3 \
	BN_generate_prime.3 BN_GENCB_set.3 \
	BN_generate_prime.3 BN_GENCB_set_old.3 \
	BN_generate_prime.3 BN_is_prime.3 \
	BN_generate_prime.3 BN_is_prime_ex.3 \
	BN_generate_prime.3 BN_is_prime_fasttest.3 \
	BN_generate_prime.3 BN_is_prime_fasttest_ex.3 \
	BN_mod_mul_montgomery.3 BN_MONT_CTX_copy.3 \
	BN_mod_mul_montgomery.3 BN_MONT_CTX_free.3 \
	BN_mod_mul_montgomery.3 BN_MONT_CTX_init.3 \
	BN_mod_mul_montgomery.3 BN_MONT_CTX_new.3 \
	BN_mod_mul_montgomery.3 BN_MONT_CTX_set.3 \
	BN_mod_mul_montgomery.3 BN_from_montgomery.3 \
	BN_mod_mul_montgomery.3 BN_to_montgomery.3 \
	BN_mod_mul_reciprocal.3 BN_RECP_CTX_free.3 \
	BN_mod_mul_reciprocal.3 BN_RECP_CTX_init.3 \
	BN_mod_mul_reciprocal.3 BN_RECP_CTX_new.3 \
	BN_mod_mul_reciprocal.3 BN_RECP_CTX_set.3 \
	BN_mod_mul_reciprocal.3 BN_div_recp.3 \
	BN_new.3 BN_clear.3 \
	BN_new.3 BN_clear_free.3 \
	BN_new.3 BN_free.3 \
	BN_new.3 BN_init.3 \
	BN_num_bytes.3 BN_num_bits.3 \
	BN_num_bytes.3 BN_num_bits_word.3 \
	BN_rand.3 BN_pseudo_rand.3 \
	BN_rand.3 BN_pseudo_rand_range.3 \
	BN_rand.3 BN_rand_range.3 \
	BN_set_bit.3 BN_clear_bit.3 \
	BN_set_bit.3 BN_is_bit_set.3 \
	BN_set_bit.3 BN_lshift.3 \
	BN_set_bit.3 BN_lshift1.3 \
	BN_set_bit.3 BN_mask_bits.3 \
	BN_set_bit.3 BN_rshift.3 \
	BN_set_bit.3 BN_rshift1.3 \
	BN_zero.3 BN_get_word.3 \
	BN_zero.3 BN_one.3 \
	BN_zero.3 BN_set_word.3 \
	BN_zero.3 BN_value_one.3 \
	BUF_MEM_new.3 BUF_MEM_free.3 \
	BUF_MEM_new.3 BUF_MEM_grow.3 \
	BUF_MEM_new.3 BUF_strdup.3 \
	CONF_modules_free.3 CONF_modules_finish.3 \
	CONF_modules_free.3 CONF_modules_unload.3 \
	CONF_modules_load_file.3 CONF_modules_load.3 \
	CRYPTO_set_ex_data.3 CRYPTO_get_ex_data.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_THREADID_cmp.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_THREADID_cpy.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_THREADID_current.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_THREADID_get_callback.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_THREADID_hash.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_THREADID_set_callback.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_add.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_add_lock.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_destroy_dynlockid.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_get_new_dynlockid.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_lock.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_num_locks.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_r_lock.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_r_unlock.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_set_dynlock_create_callback.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_set_dynlock_destroy_callback.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_set_dynlock_lock_callback.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_set_id_callback.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_w_lock.3 \
	CRYPTO_set_locking_callback.3 CRYPTO_w_unlock.3 \
	DES_set_key.3 DES_cbc_cksum.3 \
	DES_set_key.3 DES_cfb64_encrypt.3 \
	DES_set_key.3 DES_cfb_encrypt.3 \
	DES_set_key.3 DES_crypt.3 \
	DES_set_key.3 DES_ecb2_encrypt.3 \
	DES_set_key.3 DES_ecb3_encrypt.3 \
	DES_set_key.3 DES_ecb_encrypt.3 \
	DES_set_key.3 DES_ede2_cbc_encrypt.3 \
	DES_set_key.3 DES_ede2_cfb64_encrypt.3 \
	DES_set_key.3 DES_ede2_ofb64_encrypt.3 \
	DES_set_key.3 DES_ede3_cbc_encrypt.3 \
	DES_set_key.3 DES_ede3_cbcm_encrypt.3 \
	DES_set_key.3 DES_ede3_cfb64_encrypt.3 \
	DES_set_key.3 DES_ede3_ofb64_encrypt.3 \
	DES_set_key.3 DES_enc_read.3 \
	DES_set_key.3 DES_enc_write.3 \
	DES_set_key.3 DES_fcrypt.3 \
	DES_set_key.3 DES_is_weak_key.3 \
	DES_set_key.3 DES_key_sched.3 \
	DES_set_key.3 DES_ncbc_encrypt.3 \
	DES_set_key.3 DES_ofb64_encrypt.3 \
	DES_set_key.3 DES_ofb_encrypt.3 \
	DES_set_key.3 DES_pcbc_encrypt.3 \
	DES_set_key.3 DES_quad_cksum.3 \
	DES_set_key.3 DES_random_key.3 \
	DES_set_key.3 DES_set_key_checked.3 \
	DES_set_key.3 DES_set_key_unchecked.3 \
	DES_set_key.3 DES_set_odd_parity.3 \
	DES_set_key.3 DES_string_to_2keys.3 \
	DES_set_key.3 DES_string_to_key.3 \
	DES_set_key.3 DES_xcbc_encrypt.3 \
	DH_generate_key.3 DH_compute_key.3 \
	DH_generate_parameters.3 DH_check.3 \
	DH_generate_parameters.3 DH_generate_parameters_ex.3 \
	DH_get_ex_new_index.3 DH_get_ex_data.3 \
	DH_get_ex_new_index.3 DH_set_ex_data.3 \
	DH_new.3 DH_free.3 \
	DH_set_method.3 DH_OpenSSL.3 \
	DH_set_method.3 DH_get_default_method.3 \
	DH_set_method.3 DH_get_default_openssl_method.3 \
	DH_set_method.3 DH_new_method.3 \
	DH_set_method.3 DH_set_default_method.3 \
	DH_set_method.3 DH_set_default_openssl_method.3 \
	DSA_SIG_new.3 DSA_SIG_free.3 \
	DSA_do_sign.3 DSA_do_verify.3 \
	DSA_generate_parameters.3 DSA_generate_parameters_ex.3 \
	DSA_get_ex_new_index.3 DSA_get_ex_data.3 \
	DSA_get_ex_new_index.3 DSA_set_ex_data.3 \
	DSA_new.3 DSA_free.3 \
	DSA_set_method.3 DSA_OpenSSL.3 \
	DSA_set_method.3 DSA_get_default_method.3 \
	DSA_set_method.3 DSA_get_default_openssl_method.3 \
	DSA_set_method.3 DSA_new_method.3 \
	DSA_set_method.3 DSA_set_default_method.3 \
	DSA_set_method.3 DSA_set_default_openssl_method.3 \
	DSA_sign.3 DSA_sign_setup.3 \
	DSA_sign.3 DSA_verify.3 \
	EC_GFp_simple_method.3 EC_GF2m_simple_method.3 \
	EC_GFp_simple_method.3 EC_GFp_mont_method.3 \
	EC_GFp_simple_method.3 EC_GFp_nist_method.3 \
	EC_GFp_simple_method.3 EC_GFp_nistp224_method.3 \
	EC_GFp_simple_method.3 EC_GFp_nistp256_method.3 \
	EC_GFp_simple_method.3 EC_GFp_nistp521_method.3 \
	EC_GFp_simple_method.3 EC_METHOD_get_field_type.3 \
	EC_GROUP_copy.3 EC_GROUP_check.3 \
	EC_GROUP_copy.3 EC_GROUP_check_discriminant.3 \
	EC_GROUP_copy.3 EC_GROUP_cmp.3 \
	EC_GROUP_copy.3 EC_GROUP_dup.3 \
	EC_GROUP_copy.3 EC_GROUP_get0_generator.3 \
	EC_GROUP_copy.3 EC_GROUP_get0_seed.3 \
	EC_GROUP_copy.3 EC_GROUP_get_asn1_flag.3 \
	EC_GROUP_copy.3 EC_GROUP_get_basis_type.3 \
	EC_GROUP_copy.3 EC_GROUP_get_cofactor.3 \
	EC_GROUP_copy.3 EC_GROUP_get_curve_name.3 \
	EC_GROUP_copy.3 EC_GROUP_get_degree.3 \
	EC_GROUP_copy.3 EC_GROUP_get_order.3 \
	EC_GROUP_copy.3 EC_GROUP_get_pentanomial_basis.3 \
	EC_GROUP_copy.3 EC_GROUP_get_point_conversion_form.3 \
	EC_GROUP_copy.3 EC_GROUP_get_seed_len.3 \
	EC_GROUP_copy.3 EC_GROUP_get_trinomial_basis.3 \
	EC_GROUP_copy.3 EC_GROUP_method_of.3 \
	EC_GROUP_copy.3 EC_GROUP_set_asn1_flag.3 \
	EC_GROUP_copy.3 EC_GROUP_set_curve_name.3 \
	EC_GROUP_copy.3 EC_GROUP_set_point_conversion_form.3 \
	EC_GROUP_copy.3 EC_GROUP_set_seed.3 \
	EC_GROUP_copy.3 EC_GROUP_set_generator.3 \
	EC_GROUP_new.3 EC_GROUP_clear_free.3 \
	EC_GROUP_new.3 EC_GROUP_free.3 \
	EC_GROUP_new.3 EC_GROUP_get_curve_GF2m.3 \
	EC_GROUP_new.3 EC_GROUP_get_curve_GFp.3 \
	EC_GROUP_new.3 EC_GROUP_new_by_curve_name.3 \
	EC_GROUP_new.3 EC_GROUP_new_curve_GF2m.3 \
	EC_GROUP_new.3 EC_GROUP_new_curve_GFp.3 \
	EC_GROUP_new.3 EC_GROUP_set_curve_GF2m.3 \
	EC_GROUP_new.3 EC_GROUP_set_curve_GFp.3 \
	EC_GROUP_new.3 EC_get_builtin_curves.3 \
	EC_KEY_new.3 EC_KEY_check_key.3 \
	EC_KEY_new.3 EC_KEY_clear_flags.3 \
	EC_KEY_new.3 EC_KEY_copy.3 \
	EC_KEY_new.3 EC_KEY_dup.3 \
	EC_KEY_new.3 EC_KEY_free.3 \
	EC_KEY_new.3 EC_KEY_generate_key.3 \
	EC_KEY_new.3 EC_KEY_get0_group.3 \
	EC_KEY_new.3 EC_KEY_get0_private_key.3 \
	EC_KEY_new.3 EC_KEY_get0_public_key.3 \
	EC_KEY_new.3 EC_KEY_get_conv_form.3 \
	EC_KEY_new.3 EC_KEY_get_enc_flags.3 \
	EC_KEY_new.3 EC_KEY_get_flags.3 \
	EC_KEY_new.3 EC_KEY_get_key_method_data.3 \
	EC_KEY_new.3 EC_KEY_insert_key_method_data.3 \
	EC_KEY_new.3 EC_KEY_new_by_curve_name.3 \
	EC_KEY_new.3 EC_KEY_precompute_mult.3 \
	EC_KEY_new.3 EC_KEY_set_asn1_flag.3 \
	EC_KEY_new.3 EC_KEY_set_conv_form.3 \
	EC_KEY_new.3 EC_KEY_set_enc_flags.3 \
	EC_KEY_new.3 EC_KEY_set_flags.3 \
	EC_KEY_new.3 EC_KEY_set_group.3 \
	EC_KEY_new.3 EC_KEY_set_private_key.3 \
	EC_KEY_new.3 EC_KEY_set_public_key.3 \
	EC_KEY_new.3 EC_KEY_set_public_key_affine_coordinates.3 \
	EC_KEY_new.3 EC_KEY_up_ref.3 \
	EC_POINT_add.3 EC_GROUP_have_precompute_mult.3 \
	EC_POINT_add.3 EC_GROUP_precompute_mult.3 \
	EC_POINT_add.3 EC_POINT_cmp.3 \
	EC_POINT_add.3 EC_POINT_dbl.3 \
	EC_POINT_add.3 EC_POINT_invert.3 \
	EC_POINT_add.3 EC_POINT_is_at_infinity.3 \
	EC_POINT_add.3 EC_POINT_is_on_curve.3 \
	EC_POINT_add.3 EC_POINT_make_affine.3 \
	EC_POINT_add.3 EC_POINT_mul.3 \
	EC_POINT_add.3 EC_POINTs_make_affine.3 \
	EC_POINT_add.3 EC_POINTs_mul.3 \
	EC_POINT_new.3 EC_POINT_bn2point.3 \
	EC_POINT_new.3 EC_POINT_clear_free.3 \
	EC_POINT_new.3 EC_POINT_copy.3 \
	EC_POINT_new.3 EC_POINT_dup.3 \
	EC_POINT_new.3 EC_POINT_free.3 \
	EC_POINT_new.3 EC_POINT_get_Jprojective_coordinates_GFp.3 \
	EC_POINT_new.3 EC_POINT_get_affine_coordinates_GF2m.3 \
	EC_POINT_new.3 EC_POINT_get_affine_coordinates_GFp.3 \
	EC_POINT_new.3 EC_POINT_hex2point.3 \
	EC_POINT_new.3 EC_POINT_method_of.3 \
	EC_POINT_new.3 EC_POINT_oct2point.3 \
	EC_POINT_new.3 EC_POINT_point2bn.3 \
	EC_POINT_new.3 EC_POINT_point2hex.3 \
	EC_POINT_new.3 EC_POINT_point2oct.3 \
	EC_POINT_new.3 EC_POINT_set_Jprojective_coordinates.3 \
	EC_POINT_new.3 EC_POINT_set_affine_coordinates_GF2m.3 \
	EC_POINT_new.3 EC_POINT_set_affine_coordinates_GFp.3 \
	EC_POINT_new.3 EC_POINT_set_compressed_coordinates_GF2m.3 \
	EC_POINT_new.3 EC_POINT_set_compressed_coordinates_GFp.3 \
	EC_POINT_new.3 EC_POINT_set_to_infinity.3 \
	ERR_GET_LIB.3 ERR_GET_FUNC.3 \
	ERR_GET_LIB.3 ERR_GET_REASON.3 \
	ERR_error_string.3 ERR_error_string_n.3 \
	ERR_error_string.3 ERR_func_error_string.3 \
	ERR_error_string.3 ERR_lib_error_string.3 \
	ERR_error_string.3 ERR_reason_error_string.3 \
	ERR_get_error.3 ERR_get_error_line.3 \
	ERR_get_error.3 ERR_get_error_line_data.3 \
	ERR_get_error.3 ERR_peek_error.3 \
	ERR_get_error.3 ERR_peek_error_line.3 \
	ERR_get_error.3 ERR_peek_error_line_data.3 \
	ERR_get_error.3 ERR_peek_last_error.3 \
	ERR_get_error.3 ERR_peek_last_error_line.3 \
	ERR_get_error.3 ERR_peek_last_error_line_data.3 \
	ERR_load_crypto_strings.3 ERR_free_strings.3 \
	ERR_load_crypto_strings.3 SSL_load_error_strings.3 \
	ERR_load_strings.3 ERR_PACK.3 \
	ERR_load_strings.3 ERR_get_next_error_library.3 \
	ERR_print_errors.3 ERR_print_errors_fp.3 \
	ERR_put_error.3 ERR_add_error_data.3 \
	ERR_remove_state.3 ERR_remove_thread_state.3 \
	ERR_set_mark.3 ERR_pop_to_mark.3 \
	EVP_DigestInit.3 EVP_DigestFinal.3 \
	EVP_DigestInit.3 EVP_DigestUpdate.3 \
	EVP_DigestInit.3 EVP_MD_CTX_block_size.3 \
	EVP_DigestInit.3 EVP_MD_CTX_copy.3 \
	EVP_DigestInit.3 EVP_MD_CTX_md.3 \
	EVP_DigestInit.3 EVP_MD_CTX_size.3 \
	EVP_DigestInit.3 EVP_MD_CTX_type.3 \
	EVP_DigestInit.3 EVP_MD_block_size.3 \
	EVP_DigestInit.3 EVP_MD_pkey_type.3 \
	EVP_DigestInit.3 EVP_MD_size.3 \
	EVP_DigestInit.3 EVP_MD_type.3 \
	EVP_DigestInit.3 EVP_dss.3 \
	EVP_DigestInit.3 EVP_dss1.3 \
	EVP_DigestInit.3 EVP_get_digestbyname.3 \
	EVP_DigestInit.3 EVP_get_digestbynid.3 \
	EVP_DigestInit.3 EVP_get_digestbyobj.3 \
	EVP_DigestInit.3 EVP_md2.3 \
	EVP_DigestInit.3 EVP_md5.3 \
	EVP_DigestInit.3 EVP_md_null.3 \
	EVP_DigestInit.3 EVP_mdc2.3 \
	EVP_DigestInit.3 EVP_ripemd160.3 \
	EVP_DigestInit.3 EVP_sha.3 \
	EVP_DigestInit.3 EVP_sha1.3 \
	EVP_DigestSignInit.3 EVP_DigestSignUpdate.3 \
	EVP_DigestSignInit.3 EVP_DigestSignFinal.3 \
	EVP_DigestVerifyInit.3 EVP_DigestVerifyUpdate.3 \
	EVP_DigestVerifyInit.3 EVP_DigestVerifyFinal.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_block_size.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_cipher.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_cleanup.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_ctrl.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_iv_length.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_key_length.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_nid.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_set_key_length.3 \
	EVP_EncryptInit.3 EVP_CIPHER_CTX_type.3 \
	EVP_EncryptInit.3 EVP_CIPHER_asn1_to_param.3 \
	EVP_EncryptInit.3 EVP_CIPHER_block_size.3 \
	EVP_EncryptInit.3 EVP_CIPHER_iv_length.3 \
	EVP_EncryptInit.3 EVP_CIPHER_key_length.3 \
	EVP_EncryptInit.3 EVP_CIPHER_nid.3 \
	EVP_EncryptInit.3 EVP_CIPHER_param_to_asn1.3 \
	EVP_EncryptInit.3 EVP_CIPHER_type.3 \
	EVP_EncryptInit.3 EVP_CipherFinal.3 \
	EVP_EncryptInit.3 EVP_CipherFinal_ex.3 \
	EVP_EncryptInit.3 EVP_CipherInit.3 \
	EVP_EncryptInit.3 EVP_CipherInit_ex.3 \
	EVP_EncryptInit.3 EVP_CipherUpdate.3 \
	EVP_EncryptInit.3 EVP_DecryptFinal.3 \
	EVP_EncryptInit.3 EVP_DecryptFinal_ex.3 \
	EVP_EncryptInit.3 EVP_DecryptInit.3 \
	EVP_EncryptInit.3 EVP_DecryptInit_ex.3 \
	EVP_EncryptInit.3 EVP_DecryptUpdate.3 \
	EVP_EncryptInit.3 EVP_EncryptFinal.3 \
	EVP_EncryptInit.3 EVP_EncryptFinal_ex.3 \
	EVP_EncryptInit.3 EVP_EncryptInit_ex.3 \
	EVP_EncryptInit.3 EVP_EncryptUpdate.3 \
	EVP_EncryptInit.3 EVP_aes_128_ccm.3 \
	EVP_EncryptInit.3 EVP_aes_128_gcm.3 \
	EVP_EncryptInit.3 EVP_aes_192_ccm.3 \
	EVP_EncryptInit.3 EVP_aes_192_gcm.3 \
	EVP_EncryptInit.3 EVP_aes_256_ccm.3 \
	EVP_EncryptInit.3 EVP_aes_256_gcm.3 \
	EVP_EncryptInit.3 EVP_bf_cbc.3 \
	EVP_EncryptInit.3 EVP_bf_cfb.3 \
	EVP_EncryptInit.3 EVP_bf_ecb.3 \
	EVP_EncryptInit.3 EVP_bf_ofb.3 \
	EVP_EncryptInit.3 EVP_cast5_cbc.3 \
	EVP_EncryptInit.3 EVP_cast5_cfb.3 \
	EVP_EncryptInit.3 EVP_cast5_ecb.3 \
	EVP_EncryptInit.3 EVP_cast5_ofb.3 \
	EVP_EncryptInit.3 EVP_des_cbc.3 \
	EVP_EncryptInit.3 EVP_des_cfb.3 \
	EVP_EncryptInit.3 EVP_des_ecb.3 \
	EVP_EncryptInit.3 EVP_des_ede.3 \
	EVP_EncryptInit.3 EVP_des_ede3.3 \
	EVP_EncryptInit.3 EVP_des_ede3_cbc.3 \
	EVP_EncryptInit.3 EVP_des_ede3_cfb.3 \
	EVP_EncryptInit.3 EVP_des_ede3_ofb.3 \
	EVP_EncryptInit.3 EVP_des_ede_cbc.3 \
	EVP_EncryptInit.3 EVP_des_ede_cfb.3 \
	EVP_EncryptInit.3 EVP_des_ede_ofb.3 \
	EVP_EncryptInit.3 EVP_des_ofb.3 \
	EVP_EncryptInit.3 EVP_desx_cbc.3 \
	EVP_EncryptInit.3 EVP_enc_null.3 \
	EVP_EncryptInit.3 EVP_get_cipherbyname.3 \
	EVP_EncryptInit.3 EVP_get_cipherbynid.3 \
	EVP_EncryptInit.3 EVP_get_cipherbyobj.3 \
	EVP_EncryptInit.3 EVP_idea_cbc.3 \
	EVP_EncryptInit.3 EVP_idea_cbc.3 \
	EVP_EncryptInit.3 EVP_idea_cfb.3 \
	EVP_EncryptInit.3 EVP_idea_ecb.3 \
	EVP_EncryptInit.3 EVP_idea_ofb.3 \
	EVP_EncryptInit.3 EVP_rc2_40_cbc.3 \
	EVP_EncryptInit.3 EVP_rc2_64_cbc.3 \
	EVP_EncryptInit.3 EVP_rc2_cbc.3 \
	EVP_EncryptInit.3 EVP_rc2_cfb.3 \
	EVP_EncryptInit.3 EVP_rc2_ecb.3 \
	EVP_EncryptInit.3 EVP_rc2_ofb.3 \
	EVP_EncryptInit.3 EVP_rc4.3 \
	EVP_EncryptInit.3 EVP_rc4_40.3 \
	EVP_EncryptInit.3 EVP_rc5_32_12_16_cbc.3 \
	EVP_EncryptInit.3 EVP_rc5_32_12_16_cfb.3 \
	EVP_EncryptInit.3 EVP_rc5_32_12_16_ecb.3 \
	EVP_EncryptInit.3 EVP_rc5_32_12_16_ofb.3 \
	EVP_OpenInit.3 EVP_OpenFinal.3 \
	EVP_OpenInit.3 EVP_OpenUpdate.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_dh_paramgen_generator.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_dh_paramgen_prime_len.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_dsa_paramgen_bits.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_ec_paramgen_curve_nid.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_rsa_keygen_pubexp.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_rsa_padding.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_rsa_pss_saltlen.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_rsa_rsa_keygen_bits.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_set_signature_md.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_ctrl_str.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_get_default_digest_nid.3 \
	EVP_PKEY_CTX_new.3 EVP_PKEY_CTX_new_id.3 \
	EVP_PKEY_CTX_new.3 EVP_PKEY_CTX_dup.3 \
	EVP_PKEY_CTX_new.3 EVP_PKEY_CTX_free.3 \
	EVP_PKEY_cmp.3 EVP_PKEY_copy_parameters.3 \
	EVP_PKEY_cmp.3 EVP_PKEY_missing_parameters.3 \
	EVP_PKEY_cmp.3 EVP_PKEY_cmp_parameters.3 \
	EVP_PKEY_new.3 EVP_PKEY_free.3 \
	EVP_PKEY_decrypt.3 EVP_PKEY_decrypt_init.3 \
	EVP_PKEY_derive.3 EVP_PKEY_derive_init.3 \
	EVP_PKEY_derive.3 EVP_PKEY_derive_set_peer.3 \
	EVP_PKEY_get_default_digest.3 EVP_PKEY_get_default_digest_nid.3 \
	EVP_PKEY_encrypt.3 EVP_PKEY_encrypt_init.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_keygen_init.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_paramgen_init.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_paramgen.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_CTX_set_cb.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_CTX_get_cb.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_CTX_get_keygen_info.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_CTX_set_app_data.3 \
	EVP_PKEY_keygen.3 EVP_PKEY_CTX_get_app_data.3 \
	EVP_PKEY_new.3 EVP_PKEY_free.3 \
	EVP_PKEY_print_private.3 EVP_PKEY_print_public.3 \
	EVP_PKEY_print_private.3 EVP_PKEY_print_params.3 \
	EVP_PKEY_sign.3 EVP_PKEY_sign_init.3 \
	EVP_PKEY_verify.3 EVP_PKEY_verify_init.3 \
	EVP_PKEY_verify_recover.3 EVP_PKEY_verify_recover_init.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_assign_DH.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_assign_DSA.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_assign_EC_KEY.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_assign_RSA.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_get1_DH.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_get1_DSA.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_get1_EC_KEY.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_get1_RSA.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_set1_DH.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_set1_DSA.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_set1_EC_KEY.3 \
	EVP_PKEY_set1_RSA.3 EVP_PKEY_type.3 \
	EVP_PKEY_sign.3 EVP_PKEY_sign_init.3 \
	EVP_PKEY_verify.3 EVP_PKEY_verify_init.3 \
	EVP_PKEY_verify_recover.3 EVP_PKEY_verify_recover_init.3 \
	EVP_SealInit.3 EVP_SealFinal.3 \
	EVP_SealInit.3 EVP_SealUpdate.3 \
	EVP_SignInit.3 EVP_PKEY_size.3 \
	EVP_SignInit.3 EVP_SignFinal.3 \
	EVP_SignInit.3 EVP_SignUpdate.3 \
	EVP_VerifyInit.3 EVP_VerifyFinal.3 \
	EVP_VerifyInit.3 EVP_VerifyUpdate.3 \
	HMAC.3 HMAC_Final.3 \
	HMAC.3 HMAC_Init.3 \
	HMAC.3 HMAC_Update.3 \
	HMAC.3 HMAC_cleanup.3 \
	MD5.3 MD2.3 \
	MD5.3 MD2_Final.3 \
	MD5.3 MD2_Init.3 \
	MD5.3 MD2_Update.3 \
	MD5.3 MD4.3 \
	MD5.3 MD4_Final.3 \
	MD5.3 MD4_Init.3 \
	MD5.3 MD4_Update.3 \
	MD5.3 MD5_Final.3 \
	MD5.3 MD5_Init.3 \
	MD5.3 MD5_Update.3 \
	OBJ_nid2obj.3 OBJ_cleanup.3 \
	OBJ_nid2obj.3 OBJ_cmp.3 \
	OBJ_nid2obj.3 OBJ_create.3 \
	OBJ_nid2obj.3 OBJ_dup.3 \
	OBJ_nid2obj.3 OBJ_ln2nid.3 \
	OBJ_nid2obj.3 OBJ_nid2ln.3 \
	OBJ_nid2obj.3 OBJ_nid2sn.3 \
	OBJ_nid2obj.3 OBJ_obj2nid.3 \
	OBJ_nid2obj.3 OBJ_obj2txt.3 \
	OBJ_nid2obj.3 OBJ_sn2nid.3 \
	OBJ_nid2obj.3 OBJ_txt2nid.3 \
	OBJ_nid2obj.3 OBJ_txt2obj.3 \
	OPENSSL_VERSION_NUMBER.3 SSLeay.3 \
	OPENSSL_VERSION_NUMBER.3 SSLeay_version.3 \
	OPENSSL_config.3 OPENSSL_no_config.3 \
	OPENSSL_load_builtin_modules.3 ASN1_add_oid_module.3 \
	OPENSSL_load_builtin_modules.3 ENGINE_add_conf_module.3 \
	OpenSSL_add_all_algorithms.3 EVP_cleanup.3 \
	OpenSSL_add_all_algorithms.3 OpenSSL_add_all_ciphers.3 \
	OpenSSL_add_all_algorithms.3 OpenSSL_add_all_digests.3 \
	PKCS5_PBKDF2_HMAC.3 PKCS5_PBKDF2_HMAC_SHA1.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_DHparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_DSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_DSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_DSAparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_NETSCAPE_CERT_SEQUENCE.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_PKCS7.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_PrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_RSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_RSAPublicKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_RSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_X509.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_X509_AUX.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_X509_CRL.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_X509_REQ.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_DHparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_DSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_DSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_DSAparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_NETSCAPE_CERT_SEQUENCE.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_PKCS7.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_RSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_RSAPublicKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_RSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_X509.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_X509_AUX.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_X509_CRL.3 \
	PEM_read_bio_PrivateKey.3 PEM_read_bio_X509_REQ.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_DHparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_DSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_DSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_DSAparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_NETSCAPE_CERT_SEQUENCE.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_PKCS7.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_PKCS8PrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_PKCS8PrivateKey_nid.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_PrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_RSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_RSAPublicKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_RSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_X509.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_X509_AUX.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_X509_CRL.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_X509_REQ.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_X509_REQ_NEW.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_DHparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_DSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_DSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_DSAparams.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_NETSCAPE_CERT_SEQUENCE.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_PKCS7.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_PKCS8PrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_PKCS8PrivateKey_nid.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_PrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_RSAPrivateKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_RSAPublicKey.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_RSA_PUBKEY.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_X509.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_X509_AUX.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_X509_CRL.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_X509_REQ.3 \
	PEM_read_bio_PrivateKey.3 PEM_write_bio_X509_REQ_NEW.3 \
	PKCS7_verify.3 PKCS7_get0_signers.3 \
	RAND_add.3 RAND_seed.3 \
	RAND_add.3 RAND_status.3 \
	RAND_bytes.3 RAND_pseudo_bytes.3 \
	RAND_load_file.3 RAND_file_name.3 \
	RAND_load_file.3 RAND_write_file.3 \
	RAND_set_rand_method.3 RAND_SSLeay.3 \
	RAND_set_rand_method.3 RAND_get_rand_method.3 \
	RC4.3 RC4_set_key.3 \
	RIPEMD160.3 RIPEMD160_Final.3 \
	RIPEMD160.3 RIPEMD160_Init.3 \
	RIPEMD160.3 RIPEMD160_Update.3 \
	RSA_blinding_on.3 RSA_blinding_off.3 \
	RSA_generate_key.3 RSA_generate_key_ex.3 \
	RSA_get_ex_new_index.3 RSA_get_ex_data.3 \
	RSA_get_ex_new_index.3 RSA_set_ex_data.3 \
	RSA_new.3 RSA_free.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_add_PKCS1_OAEP.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_add_PKCS1_type_2.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_add_SSLv23.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_add_none.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_check_PKCS1_OAEP.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_check_PKCS1_type_1.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_check_PKCS1_type_2.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_check_SSLv23.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_padding_check_none.3 \
	RSA_print.3 DHparams_print.3 \
	RSA_print.3 DHparams_print_fp.3 \
	RSA_print.3 DSA_print.3 \
	RSA_print.3 DSA_print_fp.3 \
	RSA_print.3 DSAparams_print.3 \
	RSA_print.3 DSAparams_print_fp.3 \
	RSA_print.3 RSA_print_fp.3 \
	RSA_private_encrypt.3 RSA_public_decrypt.3 \
	RSA_public_encrypt.3 RSA_private_decrypt.3 \
	RSA_set_method.3 RSA_PKCS1_RSAref.3 \
	RSA_set_method.3 RSA_PKCS1_SSLeay.3 \
	RSA_set_method.3 RSA_flags.3 \
	RSA_set_method.3 RSA_get_default_method.3 \
	RSA_set_method.3 RSA_get_default_openssl_method.3 \
	RSA_set_method.3 RSA_get_method.3 \
	RSA_set_method.3 RSA_new_method.3 \
	RSA_set_method.3 RSA_null_method.3 \
	RSA_set_method.3 RSA_set_default_method.3 \
	RSA_set_method.3 RSA_set_default_openssl_method.3 \
	RSA_sign.3 RSA_verify.3 \
	RSA_sign_ASN1_OCTET_STRING.3 RSA_verify_ASN1_OCTET_STRING.3 \
	SHA1.3 SHA1_Final.3 \
	SHA1.3 SHA1_Init.3 \
	SHA1.3 SHA1_Update.3 \
	X509_NAME_ENTRY_get_object.3 X509_NAME_ENTRY_create_by_NID.3 \
	X509_NAME_ENTRY_get_object.3 X509_NAME_ENTRY_create_by_OBJ.3 \
	X509_NAME_ENTRY_get_object.3 X509_NAME_ENTRY_create_by_txt.3 \
	X509_NAME_ENTRY_get_object.3 X509_NAME_ENTRY_get_data.3 \
	X509_NAME_ENTRY_get_object.3 X509_NAME_ENTRY_set_data.3 \
	X509_NAME_ENTRY_get_object.3 X509_NAME_ENTRY_set_object.3 \
	X509_NAME_add_entry_by_txt.3 X509_NAME_add_entry.3 \
	X509_NAME_add_entry_by_txt.3 X509_NAME_add_entry_by_NID.3 \
	X509_NAME_add_entry_by_txt.3 X509_NAME_add_entry_by_OBJ.3 \
	X509_NAME_add_entry_by_txt.3 X509_NAME_delete_entry.3 \
	X509_NAME_print_ex.3 X509_NAME_oneline.3 \
	X509_NAME_print_ex.3 X509_NAME_print.3 \
	X509_NAME_print_ex.3 X509_NAME_print_ex_fp.3 \
	X509_new.3 X509_free.3 \
	X509_STORE_CTX_get_error.3 X509_STORE_CTX_get1_chain.3 \
	X509_STORE_CTX_get_error.3 X509_STORE_CTX_get_current_cert.3 \
	X509_STORE_CTX_get_error.3 X509_STORE_CTX_get_error_depth.3 \
	X509_STORE_CTX_get_error.3 X509_STORE_CTX_set_error.3 \
	X509_STORE_CTX_get_error.3 X509_verify_cert_error_string.3 \
	X509_STORE_CTX_get_ex_new_index.3 X509_STORE_CTX_get_ex_data.3 \
	X509_STORE_CTX_get_ex_new_index.3 X509_STORE_CTX_set_ex_data.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_cleanup.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_free.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_get0_param.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_init.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_set0_crls.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_set0_param.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_set_cert.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_set_chain.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_set_default.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_trusted_stack.3 \
	X509_STORE_set_verify_cb_func.3 X509_STORE_set_verify_cb.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_add0_policy.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_clear_flags.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_get_depth.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_get_flags.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_set1_policies.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_set_depth.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_set_purpose.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_set_time.3 \
	X509_VERIFY_PARAM_set_flags.3 X509_VERIFY_PARAM_set_trust.3 \
	bn_internal.3 bn_add_words.3 \
	bn_internal.3 bn_check_top.3 \
	bn_internal.3 bn_cmp_words.3 \
	bn_internal.3 bn_div_words.3 \
	bn_internal.3 bn_dump.3 \
	bn_internal.3 bn_expand.3 \
	bn_internal.3 bn_expand2.3 \
	bn_internal.3 bn_fix_top.3 \
	bn_internal.3 bn_mul_add_words.3 \
	bn_internal.3 bn_mul_comba4.3 \
	bn_internal.3 bn_mul_comba8.3 \
	bn_internal.3 bn_mul_high.3 \
	bn_internal.3 bn_mul_low_normal.3 \
	bn_internal.3 bn_mul_low_recursive.3 \
	bn_internal.3 bn_mul_normal.3 \
	bn_internal.3 bn_mul_part_recursive.3 \
	bn_internal.3 bn_mul_recursive.3 \
	bn_internal.3 bn_mul_words.3 \
	bn_internal.3 bn_print.3 \
	bn_internal.3 bn_set_high.3 \
	bn_internal.3 bn_set_low.3 \
	bn_internal.3 bn_set_max.3 \
	bn_internal.3 bn_sqr_comba4.3 \
	bn_internal.3 bn_sqr_comba8.3 \
	bn_internal.3 bn_sqr_normal.3 \
	bn_internal.3 bn_sqr_recursive.3 \
	bn_internal.3 bn_sqr_words.3 \
	bn_internal.3 bn_sub_words.3 \
	bn_internal.3 bn_wexpand.3 \
	bn_internal.3 mul.3 \
	bn_internal.3 mul_add.3 \
	bn_internal.3 sqr.3 \
	d2i_ASN1_OBJECT.3 i2d_ASN1_OBJECT.3 \
	d2i_DHparams.3 i2d_DHparams.3 \
	d2i_DSAPublicKey.3 d2i_DSAPrivateKey.3 \
	d2i_DSAPublicKey.3 d2i_DSA_PUBKEY.3 \
	d2i_DSAPublicKey.3 d2i_DSA_SIG.3 \
	d2i_DSAPublicKey.3 d2i_DSAparams.3 \
	d2i_DSAPublicKey.3 i2d_DSAPrivateKey.3 \
	d2i_DSAPublicKey.3 i2d_DSAPublicKey.3 \
	d2i_DSAPublicKey.3 i2d_DSA_PUBKEY.3 \
	d2i_DSAPublicKey.3 i2d_DSA_SIG.3 \
	d2i_DSAPublicKey.3 i2d_DSAparams.3 \
	d2i_ECPKParameters.3 ECPKParameters_print.3 \
	d2i_ECPKParameters.3 ECPKParameters_print_fp.3 \
	d2i_ECPKParameters.3 d2i_ECPKParameters_bio.3 \
	d2i_ECPKParameters.3 d2i_ECPKParameters_fp.3 \
	d2i_ECPKParameters.3 i2d_ECPKParameters.3 \
	d2i_ECPKParameters.3 i2d_ECPKParameters_bio.3 \
	d2i_ECPKParameters.3 i2d_ECPKParameters_fp.3 \
	d2i_PKCS8PrivateKey.3 d2i_PKCS8PrivateKey_bio.3 \
	d2i_PKCS8PrivateKey.3 d2i_PKCS8PrivateKey_fp.3 \
	d2i_PKCS8PrivateKey.3 i2d_PKCS8PrivateKey_bio.3 \
	d2i_PKCS8PrivateKey.3 i2d_PKCS8PrivateKey_fp.3 \
	d2i_PKCS8PrivateKey.3 i2d_PKCS8PrivateKey_nid_bio.3 \
	d2i_PKCS8PrivateKey.3 i2d_PKCS8PrivateKey_nid_fp.3 \
	d2i_RSAPublicKey.3 d2i_Netscape_RSA.3 \
	d2i_RSAPublicKey.3 d2i_RSAPrivateKey.3 \
	d2i_RSAPublicKey.3 i2d_Netscape_RSA.3 \
	d2i_RSAPublicKey.3 i2d_RSAPrivateKey.3 \
	d2i_RSAPublicKey.3 i2d_RSAPublicKey.3 \
	d2i_X509.3 d2i_X509_bio.3 \
	d2i_X509.3 d2i_X509_fp.3 \
	d2i_X509.3 i2d_X509.3 \
	d2i_X509.3 i2d_X509_bio.3 \
	d2i_X509.3 i2d_X509_fp.3 \
	d2i_X509_ALGOR.3 i2d_X509_ALGOR.3 \
	d2i_X509_CRL.3 d2i_X509_CRL_bio.3 \
	d2i_X509_CRL.3 d2i_X509_CRL_fp.3 \
	d2i_X509_CRL.3 i2d_X509_CRL.3 \
	d2i_X509_CRL.3 i2d_X509_CRL_bio.3 \
	d2i_X509_CRL.3 i2d_X509_CRL_fp.3 \
	d2i_X509_NAME.3 i2d_X509_NAME.3 \
	d2i_X509_REQ.3 d2i_X509_REQ_bio.3 \
	d2i_X509_REQ.3 d2i_X509_REQ_fp.3 \
	d2i_X509_REQ.3 i2d_X509_REQ.3 \
	d2i_X509_REQ.3 i2d_X509_REQ_bio.3 \
	d2i_X509_REQ.3 i2d_X509_REQ_fp.3 \
	ecdsa.3 ECDSA_OpenSSL.3 \
	ecdsa.3 ECDSA_SIG_free.3 \
	ecdsa.3 ECDSA_SIG_new.3 \
	ecdsa.3 ECDSA_do_sign.3 \
	ecdsa.3 ECDSA_do_sign_ex.3 \
	ecdsa.3 ECDSA_do_verify.3 \
	ecdsa.3 ECDSA_get_default_method.3 \
	ecdsa.3 ECDSA_get_ex_data.3 \
	ecdsa.3 ECDSA_get_ex_new_index.3 \
	ecdsa.3 ECDSA_set_default_method.3 \
	ecdsa.3 ECDSA_set_ex_data.3 \
	ecdsa.3 ECDSA_set_method.3 \
	ecdsa.3 ECDSA_sign.3 \
	ecdsa.3 ECDSA_sign_ex.3 \
	ecdsa.3 ECDSA_sign_setup.3 \
	ecdsa.3 ECDSA_verify.3 \
	ecdsa.3 d2i_ECDSA_SIG.3 \
	ecdsa.3 i2d_ECDSA_SIG.3 \
	engine.3 ENGINE_add.3 \
	engine.3 ENGINE_by_id.3 \
	engine.3 ENGINE_finish.3 \
	engine.3 ENGINE_get_first.3 \
	engine.3 ENGINE_get_last.3 \
	engine.3 ENGINE_get_next.3 \
	engine.3 ENGINE_get_prev.3 \
	engine.3 ENGINE_init.3 \
	engine.3 ENGINE_load_builtin_engines.3 \
	engine.3 ENGINE_remove.3 \
	lh_stats.3 lh_node_stats.3 \
	lh_stats.3 lh_node_stats_bio.3 \
	lh_stats.3 lh_node_usage_stats.3 \
	lh_stats.3 lh_node_usage_stats_bio.3 \
	lh_stats.3 lh_stats_bio.3 \
	lhash.3 lh_delete.3 \
	lhash.3 lh_doall.3 \
	lhash.3 lh_doall_arg.3 \
	lhash.3 lh_error.3 \
	lhash.3 lh_free.3 \
	lhash.3 lh_insert.3 \
	lhash.3 lh_new.3 \
	lhash.3 lh_retrieve.3 \
	ui.3 UI_OpenSSL.3 \
	ui.3 UI_add_error_string.3 \
	ui.3 UI_add_info_string.3 \
	ui.3 UI_add_input_boolean.3 \
	ui.3 UI_add_input_string.3 \
	ui.3 UI_add_user_data.3 \
	ui.3 UI_add_verify_string.3 \
	ui.3 UI_construct_prompt.3 \
	ui.3 UI_ctrl.3 \
	ui.3 UI_dup_error_string.3 \
	ui.3 UI_dup_info_string.3 \
	ui.3 UI_dup_input_boolean.3 \
	ui.3 UI_dup_input_string.3 \
	ui.3 UI_dup_verify_string.3 \
	ui.3 UI_free.3 \
	ui.3 UI_get0_result.3 \
	ui.3 UI_get0_user_data.3 \
	ui.3 UI_get_default_method.3 \
	ui.3 UI_get_method.3 \
	ui.3 UI_new.3 \
	ui.3 UI_new_method.3 \
	ui.3 UI_process.3 \
	ui.3 UI_set_default_method.3 \
	ui.3 UI_set_method.3 \
	ui_compat.3 des_read_2passwords.3 \
	ui_compat.3 des_read_password.3 \
	ui_compat.3 des_read_pw.3 \
	ui_compat.3 des_read_pw_string.3

.include <bsd.man.mk>
.else
maninstall:

.endif

.PATH: ${.CURDIR}/../../libssl/src/doc/crypto
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
