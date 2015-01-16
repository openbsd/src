# $OpenBSD: Makefile,v 1.30 2015/01/16 01:58:17 schwarze Exp $

.include <bsd.own.mk>		# for NOMAN

POD2MAN=pod2man --official --release="OpenBSD ${OSREV}" --center=OpenSSL

.ifndef NOMAN
MAN=	\
	BIO_f_ssl.3 \
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
	SSL_CTX_set_psk_client_callback.3 \
	SSL_CTX_set_quiet_shutdown.3 \
	SSL_CTX_set_session_cache_mode.3 \
	SSL_CTX_set_session_id_context.3 \
	SSL_CTX_set_ssl_version.3 \
	SSL_CTX_set_timeout.3 \
	SSL_CTX_set_tmp_dh_callback.3 \
	SSL_CTX_set_tmp_rsa_callback.3 \
	SSL_CTX_set_verify.3 \
	SSL_CTX_use_certificate.3 \
	SSL_CTX_use_psk_identity_hint.3 \
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
	SSL_get_psk_identity.3 \
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
	d2i_SSL_SESSION.3 \
	ssl.3 \

# XXX ERR_load_crypto_strings.3 built by libcrypto
# XXX PLEASE LEAVE THESE AS TWO PER LINE. DO NOT WRAP.
# XXX SCRIPTS IN PORTABLE DEPEND ON THIS.
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
	SSL_CTX_new.3 SSLv23_client_method.3 \
	SSL_CTX_new.3 SSLv23_method.3 \
	SSL_CTX_new.3 SSLv23_server_method.3 \
	SSL_CTX_new.3 SSLv3_client_method.3 \
	SSL_CTX_new.3 SSLv3_method.3 \
	SSL_CTX_new.3 SSLv3_server_method.3 \
	SSL_CTX_new.3 TLSv1_1_client_method.3 \
	SSL_CTX_new.3 TLSv1_1_method.3 \
	SSL_CTX_new.3 TLSv1_1_server_method.3 \
	SSL_CTX_new.3 TLSv1_client_method.3 \
	SSL_CTX_new.3 TLSv1_method.3 \
	SSL_CTX_new.3 TLSv1_server_method.3 \
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
	SSL_CTX_set_default_passwd_cb.3 SSL_CTX_set_default_passwd_cb_userdata.3 \
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

.PATH: ${.CURDIR}/../src/doc/ssl
.SUFFIXES: .pod .1 .3 .7
.for sect in 1 3 7
.pod.${sect}:
	@echo '${POD2MAN} --section=${sect} --name=${*:U} $< > $@'
	@${POD2MAN} --section=${sect} --name=${*:U} $< > $@.tmp && mv $@.tmp $@
.endfor

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

clean cleandir:
	rm -f ${MAN}
