# $OpenBSD: Makefile,v 1.24 2014/04/14 08:18:40 mpi Exp $

.include <bsd.own.mk>		# for NOMAN

POD2MAN=pod2man --official --release="OpenBSD ${OSREV}" --center=OpenSSL

.ifndef NOMAN

# libcrypto
MAN=	ASN1_OBJECT_new.3 ASN1_STRING_length.3 ASN1_STRING_new.3 \
	ASN1_STRING_print_ex.3 ASN1_generate_nconf.3

MAN+=	BIO_ctrl.3 BIO_f_base64.3 BIO_f_buffer.3 BIO_f_cipher.3 BIO_f_md.3 \
	BIO_f_null.3 BIO_f_ssl.3 BIO_find_type.3 BIO_new.3 BIO_new_CMS.3 \
	BIO_push.3 BIO_read.3 BIO_s_accept.3 BIO_s_bio.3 BIO_s_connect.3 \
	BIO_s_fd.3 BIO_s_file.3 BIO_s_mem.3 BIO_s_null.3 BIO_s_socket.3 \
	BIO_set_callback.3 BIO_should_retry.3

MAN+=	BN_BLINDING_new.3 BN_CTX_new.3 BN_CTX_start.3 BN_add.3 BN_add_word.3 \
	BN_bn2bin.3 BN_cmp.3 BN_copy.3 BN_generate_prime.3 BN_mod_inverse.3 \
	BN_mod_mul_montgomery.3 BN_mod_mul_reciprocal.3 BN_new.3 \
	BN_num_bytes.3 BN_rand.3 BN_set_bit.3 BN_swap.3 BN_zero.3

MAN+=	CMS_add0_cert.3 CMS_add1_recipient_cert.3 CMS_compress.3 CMS_decrypt.3 \
	CMS_encrypt.3 CMS_final.3 CMS_get0_RecipientInfos.3 \
	CMS_get0_SignerInfos.3 CMS_get0_type.3 CMS_get1_ReceiptRequest.3 \
	CMS_sign.3 CMS_sign_add1_signer.3 CMS_sign_receipt.3 CMS_uncompress.3 \
	CMS_verify.3 CMS_verify_receipt.3

MAN+=	CONF_modules_free.3 CONF_modules_load_file.3

MAN+=	CRYPTO_set_ex_data.3

MAN+=	DH_generate_key.3 DH_generate_parameters.3 DH_get_ex_new_index.3 \
	DH_new.3 DH_set_method.3 DH_size.3

MAN+=	DSA_SIG_new.3 DSA_do_sign.3 DSA_dup_DH.3 DSA_generate_key.3 \
	DSA_generate_parameters.3 DSA_get_ex_new_index.3 DSA_new.3 \
	DSA_set_method.3 DSA_sign.3 DSA_size.3

MAN+=	ERR_GET_LIB.3 ERR_clear_error.3 ERR_error_string.3 ERR_get_error.3 \
	ERR_load_crypto_strings.3 ERR_load_strings.3 ERR_print_errors.3 \
	ERR_put_error.3 ERR_remove_state.3 ERR_set_mark.3

MAN+=	EVP_BytesToKey.3 EVP_DigestInit.3 EVP_DigestSignInit.3 \
	EVP_DigestVerifyInit.3 EVP_EncryptInit.3 EVP_OpenInit.3 \
	EVP_PKEY_CTX_ctrl.3 EVP_PKEY_CTX_new.3 EVP_PKEY_cmp.3 \
	EVP_PKEY_decrypt.3 EVP_PKEY_derive.3 EVP_PKEY_encrypt.3 \
	EVP_PKEY_get_default_digest.3 EVP_PKEY_keygen.3 EVP_PKEY_new.3 \
	EVP_PKEY_print_private.3 EVP_PKEY_set1_RSA.3 EVP_PKEY_sign.3 \
	EVP_PKEY_verify.3 EVP_PKEY_verify_recover.3 EVP_SealInit.3 \
	EVP_SignInit.3 EVP_VerifyInit.3

MAN+=	OBJ_nid2obj.3

MAN+=	OPENSSL_Applink.3 OPENSSL_VERSION_NUMBER.3 OPENSSL_config.3 \
	OPENSSL_ia32cap.3 OPENSSL_load_builtin_modules.3 \
	OpenSSL_add_all_algorithms.3

MAN+=	PEM_write_bio_CMS_stream.3 PEM_write_bio_PKCS7_stream.3

MAN+=	PKCS12_create.3 PKCS12_parse.3 PKCS7_decrypt.3 PKCS7_encrypt.3 \
	PKCS7_sign.3 PKCS7_sign_add_signer.3 PKCS7_verify.3

MAN+=	RAND_add.3 RAND_bytes.3 RAND_cleanup.3 RAND_egd.3 RAND_load_file.3 \
	RAND_set_rand_method.3

MAN+=	RSA_blinding_on.3 RSA_check_key.3 RSA_generate_key.3 \
	RSA_get_ex_new_index.3 RSA_new.3 RSA_padding_add_PKCS1_type_1.3 \
	RSA_print.3 RSA_private_encrypt.3 RSA_public_encrypt.3 \
	RSA_set_method.3 RSA_sign.3 RSA_sign_ASN1_OCTET_STRING.3 RSA_size.3

MAN+=	SMIME_read_CMS.3 SMIME_read_PKCS7.3 SMIME_write_CMS.3 \
	SMIME_write_PKCS7.3

MAN+=	X509_NAME_ENTRY_get_object.3 X509_NAME_add_entry_by_txt.3 \
	X509_NAME_get_index_by_NID.3 X509_NAME_print_ex.3 \
	X509_STORE_CTX_get_error.3 X509_STORE_CTX_get_ex_new_index.3 \
	X509_STORE_CTX_new.3 X509_STORE_CTX_set_verify_cb.3 \
	X509_STORE_set_verify_cb_func.3 X509_VERIFY_PARAM_set_flags.3 \
	X509_new.3 X509_verify_cert.3

MAN+=	d2i_ASN1_OBJECT.3 d2i_DHparams.3 d2i_DSAPublicKey.3 \
	d2i_PKCS8PrivateKey.3 d2i_RSAPublicKey.3 d2i_X509.3 d2i_X509_ALGOR.3 \
	d2i_X509_CRL.3 d2i_X509_NAME.3 d2i_X509_REQ.3 d2i_X509_SIG.3

MAN+=	i2d_CMS_bio_stream.3 i2d_PKCS7_bio_stream.3

MAN+=	bio.3 blowfish.3 bn.3 bn_internal.3 buffer.3 crypto.3 des.3 \
	des_modes.3 dh.3 dsa.3 ecdsa.3 engine.3 err.3 evp.3 hmac.3 lh_stats.3 \
	lhash.3 md5.3 mdc2.3 pem.3 rand.3 rc4.3 ripemd.3 rsa.3 sha.3 threads.3 \
	ui.3 ui_compat.3 x509.3

# libssl
MAN+=	SSL_CIPHER_get_name.3 SSL_COMP_add_compression_method.3 \
	SSL_CTX_add_extra_chain_cert.3 SSL_CTX_add_session.3 SSL_CTX_ctrl.3 \
	SSL_CTX_flush_sessions.3 SSL_CTX_free.3 SSL_CTX_get_ex_new_index.3 \
	SSL_CTX_get_verify_mode.3 SSL_CTX_load_verify_locations.3 \
	SSL_CTX_new.3 SSL_CTX_sess_number.3 SSL_CTX_sess_set_cache_size.3 \
	SSL_CTX_sess_set_get_cb.3 SSL_CTX_sessions.3 SSL_CTX_set_cert_store.3 \
	SSL_CTX_set_cert_verify_callback.3 SSL_CTX_set_cipher_list.3 \
	SSL_CTX_set_client_CA_list.3 SSL_CTX_set_client_cert_cb.3 \
	SSL_CTX_set_default_passwd_cb.3 SSL_CTX_set_generate_session_id.3 \
	SSL_CTX_set_info_callback.3 SSL_CTX_set_max_cert_list.3 \
	SSL_CTX_set_mode.3 SSL_CTX_set_msg_callback.3 SSL_CTX_set_options.3 \
	SSL_CTX_set_psk_client_callback.3 SSL_CTX_set_quiet_shutdown.3 \
	SSL_CTX_set_session_cache_mode.3 SSL_CTX_set_session_id_context.3 \
	SSL_CTX_set_ssl_version.3 SSL_CTX_set_timeout.3 \
	SSL_CTX_set_tmp_dh_callback.3 SSL_CTX_set_tmp_rsa_callback.3 \
	SSL_CTX_set_verify.3 SSL_CTX_use_certificate.3 \
	SSL_CTX_use_psk_identity_hint.3 SSL_SESSION_free.3 \
	SSL_SESSION_get_ex_new_index.3 SSL_SESSION_get_time.3 SSL_accept.3 \
	SSL_alert_type_string.3	SSL_clear.3 SSL_connect.3 SSL_do_handshake.3 \
	SSL_free.3 SSL_get_SSL_CTX.3 SSL_get_ciphers.3 \
	SSL_get_client_CA_list.3 SSL_get_current_cipher.3 \
	SSL_get_default_timeout.3 SSL_get_error.3 \
	SSL_get_ex_data_X509_STORE_CTX_idx.3 SSL_get_ex_new_index.3 \
	SSL_get_fd.3 SSL_get_peer_cert_chain.3 SSL_get_peer_certificate.3 \
	SSL_get_psk_identity.3 SSL_get_rbio.3 SSL_get_session.3 \
	SSL_get_verify_result.3	SSL_get_version.3 SSL_library_init.3 \
	SSL_load_client_CA_file.3 SSL_new.3 SSL_pending.3 SSL_read.3 \
	SSL_rstate_string.3 SSL_session_reused.3 SSL_set_bio.3 \
	SSL_set_connect_state.3	SSL_set_fd.3 SSL_set_session.3 \
	SSL_set_shutdown.3 SSL_set_verify_result.3 SSL_shutdown.3 \
	SSL_state_string.3 SSL_want.3 SSL_write.3 \
	d2i_SSL_SESSION.3 ssl.3

# XXX ERR_load_crypto_strings.3 built by libcrypto
MLINKS+=\
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
	ERR_load_crypto_strings.3 SSL_load_error_strings.3 \
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
	SSL_CTX_set_psk_client_callback.3 SSL_set_psk_client_callback.3 \
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
	SSL_CTX_use_psk_identity_hint.3 SSL_use_psk_identity_hint.3 \
	SSL_CTX_use_psk_identity_hint.3 SSL_CTX_set_psk_server_callback.3 \
	SSL_CTX_use_psk_identity_hint.3 SSL_set_psk_server_callback.3 \
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
	SSL_get_psk_identity.3 SSL_get_psk_identity_hint.3 \
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
	d2i_SSL_SESSION.3 i2d_SSL_SESSION.3 \

.include <bsd.man.mk>
.else
maninstall:

.endif

# XXX .PATH order is critical because of non-unique filenames
# XXX doc/crypto because of BIO_f_ssl()
.PATH: ${.CURDIR}/../src/doc/crypto ${.CURDIR}/../src/doc/ssl
.SUFFIXES: .pod .1 .3 .7
.for sect in 1 3 7
.pod.${sect}:
	@echo '${POD2MAN} --section=${sect} --name=${*:U} $< > $@'
	@${POD2MAN} --section=${sect} --name=${*:U} $< > $@.tmp && mv $@.tmp $@
.endfor

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

clean cleandir:
	rm -f ${MAN} ${MANLINT}
