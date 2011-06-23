# $OpenBSD: Makefile,v 1.18 2011/06/23 22:46:12 schwarze Exp $

.include <bsd.own.mk>		# for NOMAN

POD2MAN=pod2man --official --release="OpenBSD ${OSREV}" --center=OpenSSL

.ifndef NOMAN
MAN=	\
	ASN1_OBJECT_new.3 \
	ASN1_STRING_length.3 \
	ASN1_STRING_new.3 \
	ASN1_STRING_print_ex.3 \
	ASN1_generate_nconf.3 \
	BIO_ctrl.3 \
	BIO_f_base64.3 \
	BIO_f_buffer.3 \
	BIO_f_cipher.3 \
	BIO_f_md.3 \
	BIO_f_null.3 \
	BIO_f_ssl.3 \
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
	CONF_modules_free.3 \
	CONF_modules_load_file.3 \
	CRYPTO_set_ex_data.3 \
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
	EVP_EncryptInit.3 \
	EVP_OpenInit.3 \
	EVP_PKEY_new.3 \
	EVP_PKEY_set1_RSA.3 \
	EVP_SealInit.3 \
	EVP_SignInit.3 \
	EVP_VerifyInit.3 \
	OBJ_nid2obj.3 \
	OPENSSL_VERSION_NUMBER.3 \
	OPENSSL_config.3 \
	OPENSSL_load_builtin_modules.3 \
	OpenSSL_add_all_algorithms.3 \
	PKCS12_create.3 \
	PKCS12_parse.3 \
	PKCS7_decrypt.3 \
	PKCS7_encrypt.3 \
	PKCS7_sign.3 \
	PKCS7_verify.3 \
	RAND_add.3 \
	RAND_bytes.3 \
	RAND_cleanup.3 \
	RAND_egd.3 \
	RAND_load_file.3 \
	RAND_set_rand_method.3 \
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
	SMIME_read_PKCS7.3 \
	SMIME_write_PKCS7.3 \
	SSL_CIPHER_get_name.3 \
	SSL_COMP_add_compression_method.3 \
	SSL_CTX_add_extra_chain_cert.3 \
	SSL_CTX_add_session.3 \
	SSL_CTX_ctrl.3 \
	SSL_CTX_flush_sessions.3 \
	SSL_CTX_free.3 \
	SSL_CTX_get_ex_new_index.3 \
	SSL_CTX_get_verify_mode.3 \
	SSL_CTX_load_verify_locations.3 \
	SSL_CTX_new.3 \
	SSL_CTX_sess_number.3 \
	SSL_CTX_sess_set_cache_size.3 \
	SSL_CTX_sess_set_get_cb.3 \
	SSL_CTX_sessions.3 \
	SSL_CTX_set_cert_store.3 \
	SSL_CTX_set_cert_verify_callback.3 \
	SSL_CTX_set_cipher_list.3 \
	SSL_CTX_set_client_CA_list.3 \
	SSL_CTX_set_client_cert_cb.3 \
	SSL_CTX_set_default_passwd_cb.3 \
	SSL_CTX_set_generate_session_id.3 \
	SSL_CTX_set_info_callback.3 \
	SSL_CTX_set_max_cert_list.3 \
	SSL_CTX_set_mode.3 \
	SSL_CTX_set_msg_callback.3 \
	SSL_CTX_set_options.3 \
	SSL_CTX_set_quiet_shutdown.3 \
	SSL_CTX_set_session_cache_mode.3 \
	SSL_CTX_set_session_id_context.3 \
	SSL_CTX_set_ssl_version.3 \
	SSL_CTX_set_timeout.3 \
	SSL_CTX_set_tmp_dh_callback.3 \
	SSL_CTX_set_tmp_rsa_callback.3 \
	SSL_CTX_set_verify.3 \
	SSL_CTX_use_certificate.3 \
	SSL_SESSION_free.3 \
	SSL_SESSION_get_ex_new_index.3 \
	SSL_SESSION_get_time.3 \
	SSL_accept.3 \
	SSL_alert_type_string.3 \
	SSL_clear.3 \
	SSL_connect.3 \
	SSL_do_handshake.3 \
	SSL_free.3 \
	SSL_get_SSL_CTX.3 \
	SSL_get_ciphers.3 \
	SSL_get_client_CA_list.3 \
	SSL_get_current_cipher.3 \
	SSL_get_default_timeout.3 \
	SSL_get_error.3 \
	SSL_get_ex_data_X509_STORE_CTX_idx.3 \
	SSL_get_ex_new_index.3 \
	SSL_get_fd.3 \
	SSL_get_peer_cert_chain.3 \
	SSL_get_peer_certificate.3 \
	SSL_get_rbio.3 \
	SSL_get_session.3 \
	SSL_get_verify_result.3 \
	SSL_get_version.3 \
	SSL_library_init.3 \
	SSL_load_client_CA_file.3 \
	SSL_new.3 \
	SSL_pending.3 \
	SSL_read.3 \
	SSL_rstate_string.3 \
	SSL_session_reused.3 \
	SSL_set_bio.3 \
	SSL_set_connect_state.3 \
	SSL_set_fd.3 \
	SSL_set_session.3 \
	SSL_set_shutdown.3 \
	SSL_set_verify_result.3 \
	SSL_shutdown.3 \
	SSL_state_string.3 \
	SSL_want.3 \
	SSL_write.3 \
	X509_NAME_ENTRY_get_object.3 \
	X509_NAME_add_entry_by_txt.3 \
	X509_NAME_get_index_by_NID.3 \
	X509_NAME_print_ex.3 \
	X509_new.3 \
	bn.3 \
	bn_internal.3 \
	crypto.3 \
	d2i_ASN1_OBJECT.3 \
	d2i_DHparams.3 \
	d2i_DSAPublicKey.3 \
	d2i_PKCS8PrivateKey.3 \
	d2i_RSAPublicKey.3 \
	d2i_SSL_SESSION.3 \
	d2i_X509.3 \
	d2i_X509_ALGOR.3 \
	d2i_X509_CRL.3 \
	d2i_X509_NAME.3 \
	d2i_X509_REQ.3 \
	d2i_X509_SIG.3 \
	dh.3 \
	dsa.3 \
	ecdsa.3 \
	engine.3 \
	evp.3 \
	lh_stats.3 \
	lhash.3 \
	pem.3 \
	rsa.3 \
	ssl.3 \
	ui.3 \
	ui_compat.3 \
	x509.3 \
	BF_set_key.3 \
	BIO.3 \
	BUF_MEM_new.3 \
	CRYPTO_set_locking_callback.3 \
	DES_set_key.3 \
	ERR.3 \
	HMAC.3 \
	MD5.3 \
	PEM_read_bio_PrivateKey.3 \
	RAND.3 \
	RC4.3 \
	RIPEMD160.3 \
	SHA1.3 \
	acss.3

# These pages removed/renamed
#	OPENSSL_ia32cap.cat3 \	# i386-only, internal API
#	OPENSSL_Applink.cat3 \	# Win32 only
#	bio.cat3 \		# conflict; see below
#	blowfish.cat3 \		# conflict; see below
#	buffer.cat3 \		# conflict; see below
#	des.cat3 \		# conflict; see below
#	des_modes.cat3 \	# fairly pointless
#	hmac.cat3 \		# conflict; see below
#	md5.cat3 \		# conflict; see below
#	mdc2.cat3 \		# patents; removed
#	rand.cat3 \		# conflict; see below
#	rc4.cat3 \		# conflict; see below
#	ripemd.cat3 \		# conflict; see below
#	sha.cat3 \		# conflict; see below


# these are a real problem, since they re-document functions described in
# other pages.

.for page src in \
	BF_set_key blowfish \
	BIO bio \
	BUF_MEM_new buffer \
	CRYPTO_set_locking_callback threads \
	DES_set_key des \
	ERR err \
	HMAC hmac \
	MD5 md5 \
	PEM_read_bio_PrivateKey pem \
	RAND rand \
	RC4 rc4 \
	RIPEMD160 ripemd \
	SHA1 sha

${page}.3: ${src}.pod
	${POD2MAN} --section=3 --name=${page:U} ${.ALLSRC} > ${.TARGET}
.endfor

MLINKS+=\
	ASN1_OBJECT_new.3 ASN1_OBJECT_free.3 \
	ASN1_STRING_length.3 ASN1_STRING_cmp.3 \
	ASN1_STRING_length.3 ASN1_STRING_data.3 \
	ASN1_STRING_length.3 ASN1_STRING_dup.3 \
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
	BIO_f_ssl.3 BIO_do_handshake.3 \
	BIO_f_ssl.3 BIO_get_num_renegotiates.3 \
	BIO_f_ssl.3 BIO_get_ssl.3 \
	BIO_f_ssl.3 BIO_new_buffer_ssl_connect.3 \
	BIO_f_ssl.3 BIO_new_ssl.3 \
	BIO_f_ssl.3 BIO_new_ssl_connect.3 \
	BIO_f_ssl.3 BIO_set_ssl.3 \
	BIO_f_ssl.3 BIO_set_ssl_mode.3 \
	BIO_f_ssl.3 BIO_set_ssl_renegotiate_bytes.3 \
	BIO_f_ssl.3 BIO_set_ssl_renegotiate_timeout.3 \
	BIO_f_ssl.3 BIO_ssl_copy_session_id.3 \
	BIO_f_ssl.3 BIO_ssl_shutdown.3 \
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
	BIO_s_socket.3 BIO_get_fd.3 \
	BIO_s_socket.3 BIO_new_socket.3 \
	BIO_s_socket.3 BIO_set_fd.3 \
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
	BN_BLINDING_new.3 BN_BLINDING_update.3 \
	BN_CTX_new.3 BN_CTX_free.3 \
	BN_CTX_new.3 BN_CTX_init.3 \
	BN_CTX_start.3 BN_CTX_end.3 \
	BN_CTX_start.3 BN_CTX_get.3 \
	BN_add.3 BN_div.3 \
	BN_add.3 BN_exp.3 \
	BN_add.3 BN_gcd.3 \
	BN_add.3 BN_mod.3 \
	BN_add.3 BN_mod_exp.3 \
	BN_add.3 BN_mod_mul.3 \
	BN_add.3 BN_mul.3 \
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
	BN_generate_prime.3 BN_is_prime.3 \
	BN_generate_prime.3 BN_is_prime_fasttest.3 \
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
	DH_generate_key.3 DH_compute_key.3 \
	DH_generate_parameters.3 DH_check.3 \
	DH_get_ex_new_index.3 DH_get_ex_data.3 \
	DH_get_ex_new_index.3 DH_set_ex_data.3 \
	DH_new.3 DH_free.3 \
	DH_set_method.3 DH_OpenSSL.3 \
	DH_set_method.3 DH_get_default_method.3 \
	DH_set_method.3 DH_get_default_openssl_method.3 \
	DH_set_method.3 DH_new_method.3 \
	DH_set_method.3 DH_set_default_method.3 \
	DH_set_method.3 DH_set_default_openssl_method.3 \
	DSA_new.3 DSA_free.3 \
	DSA_set_method.3 DSA_OpenSSL.3 \
	DSA_set_method.3 DSA_get_default_method.3 \
	DSA_set_method.3 DSA_get_default_openssl_method.3 \
	DSA_set_method.3 DSA_new_method.3 \
	DSA_set_method.3 DSA_set_default_method.3 \
	DSA_set_method.3 DSA_set_default_openssl_method.3 \
	DSA_sign.3 DSA_sign_setup.3 \
	DSA_sign.3 DSA_verify.3 \
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
	ERR_load_crypto_strings.3 ERR_free_strings.3 \
	ERR_load_crypto_strings.3 SSL_load_error_strings.3 \
	ERR_load_strings.3 ERR_PACK.3 \
	ERR_load_strings.3 ERR_get_next_error_library.3 \
	ERR_print_errors.3 ERR_print_errors_fp.3 \
	ERR_put_error.3 ERR_add_error_data.3 \
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
	EVP_DigestInit.3 OBJ_nid2sn.3 \
	EVP_DigestInit.3 OBJ_obj2nid.3 \
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
	EVP_EncryptInit.3 EVP_CipherInit.3 \
	EVP_EncryptInit.3 EVP_CipherUpdate.3 \
	EVP_EncryptInit.3 EVP_DecryptFinal.3 \
	EVP_EncryptInit.3 EVP_DecryptInit.3 \
	EVP_EncryptInit.3 EVP_DecryptUpdate.3 \
	EVP_EncryptInit.3 EVP_EncryptFinal.3 \
	EVP_EncryptInit.3 EVP_EncryptUpdate.3 \
	EVP_EncryptInit.3 EVP_get_cipherbyname.3 \
	EVP_EncryptInit.3 EVP_get_cipherbynid.3 \
	EVP_EncryptInit.3 EVP_get_cipherbyobj.3 \
	EVP_EncryptInit.3 OBJ_nid2sn.3 \
	EVP_EncryptInit.3 OBJ_obj2nid.3 \
	EVP_OpenInit.3 EVP_OpenFinal.3 \
	EVP_OpenInit.3 EVP_OpenUpdate.3 \
	EVP_PKEY_new.3 EVP_PKEY_free.3 \
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
	RAND_add.3 RAND_event.3 \
	RAND_add.3 RAND_screen.3 \
	RAND_add.3 RAND_seed.3 \
	RAND_add.3 RAND_status.3 \
	RAND_bytes.3 RAND_pseudo_bytes.3 \
	RAND_egd.3 RAND_egd_bytes.3 \
	RAND_load_file.3 RAND_file_name.3 \
	RAND_load_file.3 RAND_write_file.3 \
	RAND_set_rand_method.3 RAND_SSLeay.3 \
	RAND_set_rand_method.3 RAND_get_rand_method.3 \
	RC4.3 RC4_set_key.3 \
	RIPEMD160.3 RIPEMD160_Final.3 \
	RIPEMD160.3 RIPEMD160_Init.3 \
	RIPEMD160.3 RIPEMD160_Update.3 \
	RSA_blinding_on.3 RSA_blinding_off.3 \
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
	SSL_CIPHER_get_name.3 SSL_CIPHER_description.3 \
	SSL_CIPHER_get_name.3 SSL_CIPHER_get_bits.3 \
	SSL_CIPHER_get_name.3 SSL_CIPHER_get_version.3 \
	SSL_CTX_add_session.3 SSL_CTX_remove_session.3 \
	SSL_CTX_add_session.3 SSL_add_session.3 \
	SSL_CTX_add_session.3 SSL_remove_session.3 \
	SSL_CTX_ctrl.3 SSL_CTX_callback_ctrl.3 \
	SSL_CTX_ctrl.3 SSL_callback_ctrl.3 \
	SSL_CTX_ctrl.3 SSL_ctrl.3 \
	SSL_CTX_flush_sessions.3 SSL_flush_sessions.3 \
	SSL_CTX_get_ex_new_index.3 SSL_CTX_get_ex_data.3 \
	SSL_CTX_get_ex_new_index.3 SSL_CTX_set_ex_data.3 \
	SSL_CTX_get_verify_mode.3 SSL_CTX_get_verify_callback.3 \
	SSL_CTX_get_verify_mode.3 SSL_CTX_get_verify_depth.3 \
	SSL_CTX_get_verify_mode.3 SSL_get_verify_callback.3 \
	SSL_CTX_get_verify_mode.3 SSL_get_verify_depth.3 \
	SSL_CTX_get_verify_mode.3 SSL_get_verify_mode.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_accept.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_accept_good.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_accept_renegotiate.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_cache_full.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_cb_hits.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_connect.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_connect_good.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_connect_renegotiate.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_hits.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_misses.3 \
	SSL_CTX_sess_number.3 SSL_CTX_sess_timeouts.3 \
	SSL_CTX_sess_set_cache_size.3 SSL_CTX_sess_get_cache_size.3 \
	SSL_CTX_sess_set_get_cb.3 SSL_CTX_sess_get_get_cb.3 \
	SSL_CTX_sess_set_get_cb.3 SSL_CTX_sess_get_new_cb.3 \
	SSL_CTX_sess_set_get_cb.3 SSL_CTX_sess_get_remove_cb.3 \
	SSL_CTX_sess_set_get_cb.3 SSL_CTX_sess_set_new_cb.3 \
	SSL_CTX_sess_set_get_cb.3 SSL_CTX_sess_set_remove.3 \
	SSL_CTX_set_cert_store.3 SSL_CTX_get_cert_store.3 \
	SSL_CTX_set_cipher_list.3 SSL_set_cipher_list.3 \
	SSL_CTX_set_client_CA_list.3 SSL_CTX_add_client_CA.3 \
	SSL_CTX_set_client_CA_list.3 SSL_add_client_CA.3 \
	SSL_CTX_set_client_CA_list.3 SSL_set_client_CA_list.3 \
	SSL_CTX_set_default_passwd_cb.3 \
	SSL_CTX_set_default_passwd_cb_userdata.3 \
	SSL_CTX_set_max_cert_list.3 SSL_CTX_get_max_cert_list.3 \
	SSL_CTX_set_max_cert_list.3 SSL_get_max_cert_list.3 \
	SSL_CTX_set_max_cert_list.3 SSL_set_max_cert_list.3 \
	SSL_CTX_set_mode.3 SSL_CTX_get_mode.3 \
	SSL_CTX_set_mode.3 SSL_get_mode.3 \
	SSL_CTX_set_mode.3 SSL_set_mode.3 \
	SSL_CTX_set_msg_callback.3 SSL_CTX_set_msg_callback_arg.3 \
	SSL_CTX_set_msg_callback.3 SSL_set_msg_callback.3 \
	SSL_CTX_set_msg_callback.3 SSL_set_msg_callback_arg.3 \
	SSL_CTX_set_options.3 SSL_CTX_get_options.3 \
	SSL_CTX_set_options.3 SSL_get_options.3 \
	SSL_CTX_set_options.3 SSL_set_options.3 \
	SSL_CTX_set_quiet_shutdown.3 SSL_CTX_get_quiet_shutdown.3 \
	SSL_CTX_set_quiet_shutdown.3 SSL_get_quiet_shutdown.3 \
	SSL_CTX_set_quiet_shutdown.3 SSL_set_quiet_shutdown.3 \
	SSL_CTX_set_session_cache_mode.3 SSL_CTX_get_session_cache_mode.3 \
	SSL_CTX_set_session_id_context.3 SSL_set_session_id_context.3 \
	SSL_CTX_set_ssl_version.3 SSL_get_ssl_method.3 \
	SSL_CTX_set_ssl_version.3 SSL_set_ssl_method.3 \
	SSL_CTX_set_timeout.3 SSL_CTX_get_timeout.3 \
	SSL_CTX_set_tmp_dh_callback.3 SSL_CTX_set_tmp_dh.3 \
	SSL_CTX_set_tmp_dh_callback.3 SSL_set_tmp_dh.3 \
	SSL_CTX_set_tmp_dh_callback.3 SSL_set_tmp_dh_callback.3 \
	SSL_CTX_set_tmp_rsa_callback.3 SSL_CTX_need_tmp_rsa.3 \
	SSL_CTX_set_tmp_rsa_callback.3 SSL_CTX_set_tmp_rsa.3 \
	SSL_CTX_set_tmp_rsa_callback.3 SSL_need_tmp_rsa.3 \
	SSL_CTX_set_tmp_rsa_callback.3 SSL_set_tmp_rsa.3 \
	SSL_CTX_set_tmp_rsa_callback.3 SSL_set_tmp_rsa_callback.3 \
	SSL_CTX_set_verify.3 SSL_CTX_set_verify_depth.3 \
	SSL_CTX_set_verify.3 SSL_set_verify.3 \
	SSL_CTX_set_verify.3 SSL_set_verify_depth.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_check_private_key.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_PrivateKey.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_PrivateKey_ASN1.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_PrivateKey_file.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_RSAPrivateKey.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_RSAPrivateKey_ASN1.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_RSAPrivateKey_file.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_certificate_ASN1.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_certificate_chain_file.3 \
	SSL_CTX_use_certificate.3 SSL_CTX_use_certificate_file.3 \
	SSL_CTX_use_certificate.3 SSL_check_private_key.3 \
	SSL_CTX_use_certificate.3 SSL_use_PrivateKey.3 \
	SSL_CTX_use_certificate.3 SSL_use_PrivateKey_ASN1.3 \
	SSL_CTX_use_certificate.3 SSL_use_PrivateKey_file.3 \
	SSL_CTX_use_certificate.3 SSL_use_RSAPrivateKey.3 \
	SSL_CTX_use_certificate.3 SSL_use_RSAPrivateKey_ASN1.3 \
	SSL_CTX_use_certificate.3 SSL_use_RSAPrivateKey_file.3 \
	SSL_CTX_use_certificate.3 SSL_use_certificate.3 \
	SSL_CTX_use_certificate.3 SSL_use_certificate_ASN1.3 \
	SSL_CTX_use_certificate.3 SSL_use_certificate_file.3 \
	SSL_SESSION_get_ex_new_index.3 SSL_SESSION_get_ex_data.3 \
	SSL_SESSION_get_ex_new_index.3 SSL_SESSION_set_ex_data.3 \
	SSL_SESSION_get_time.3 SSL_SESSION_get_timeout.3 \
	SSL_SESSION_get_time.3 SSL_SESSION_set_time.3 \
	SSL_SESSION_get_time.3 SSL_SESSION_set_timeout.3 \
	SSL_SESSION_get_time.3 SSL_get_time.3 \
	SSL_SESSION_get_time.3 SSL_get_timeout.3 \
	SSL_SESSION_get_time.3 SSL_set_time.3 \
	SSL_SESSION_get_time.3 SSL_set_timeout.3 \
	SSL_alert_type_string.3 SSL_alert_desc_string.3 \
	SSL_alert_type_string.3 SSL_alert_desc_string_long.3 \
	SSL_alert_type_string.3 SSL_alert_type_string_long.3 \
	SSL_get_ciphers.3 SSL_get_cipher_list.3 \
	SSL_get_client_CA_list.3 SSL_CTX_get_client_CA_list.3 \
	SSL_get_current_cipher.3 SSL_get_cipher.3 \
	SSL_get_current_cipher.3 SSL_get_cipher_bits.3 \
	SSL_get_current_cipher.3 SSL_get_cipher_name.3 \
	SSL_get_current_cipher.3 SSL_get_cipher_version.3 \
	SSL_get_ex_new_index.3 SSL_get_ex_data.3 \
	SSL_get_ex_new_index.3 SSL_set_ex_data.3 \
	SSL_get_fd.3 SSL_get_rfd.3 \
	SSL_get_fd.3 SSL_get_wfd.3 \
	SSL_get_rbio.3 SSL_get_wbio.3 \
	SSL_get_session.3 SSL_get0_session.3 \
	SSL_get_session.3 SSL_get1_session.3 \
	SSL_library_init.3 OpenSSL_add_ssl_algorithms.3 \
	SSL_library_init.3 SSLeay_add_ssl_algorithms.3 \
	SSL_rstate_string.3 SSL_rstate_string_long.3 \
	SSL_set_connect_state.3 SSL_set_accept_state.3 \
	SSL_set_fd.3 SSL_set_rfd.3 \
	SSL_set_fd.3 SSL_set_wfd.3 \
	SSL_set_shutdown.3 SSL_get_shutdown.3 \
	SSL_state_string.3 SSL_state_string_long.3 \
	SSL_want.3 SSL_want_nothing.3 \
	SSL_want.3 SSL_want_read.3 \
	SSL_want.3 SSL_want_write.3 \
	SSL_want.3 SSL_want_x509_lookup.3 \
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
	X509_new.3 X509_free.3 \
	acss.3 acss_setkey.3 \
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
	d2i_SSL_SESSION.3 i2d_SSL_SESSION.3 \
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
	dsa.3 DSA_OpenSSL.3 \
	dsa.3 DSA_SIG_free.3 \
	dsa.3 DSA_do_verify.3 \
	dsa.3 DSA_free.3 \
	dsa.3 DSA_get_default_method.3 \
	dsa.3 DSA_get_ex_data.3 \
	dsa.3 DSA_new_method.3 \
	dsa.3 DSA_set_default_method.3 \
	dsa.3 DSA_set_ex_data.3 \
	dsa.3 DSA_sign_setup.3 \
	dsa.3 DSA_verify.3 \
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

# XXX .PATH order is critical because of non-unique filenames
.PATH: ${.CURDIR}/../src/doc/crypto ${.CURDIR}/../src/doc/ssl ${.CURDIR}/../src/doc/apps
.SUFFIXES: .pod .1 .3 .7
.for sect in 1 3 7
.pod.${sect}:
	${POD2MAN} --section=${sect} --name=${*:U} ${.IMPSRC} > ${.TARGET}
.endfor

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

clean cleandir:
	rm -f ${MAN}
