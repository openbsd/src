# $OpenBSD: Makefile,v 1.1 2002/09/03 18:59:56 markus Exp $

.include <bsd.own.mk>		# for NOMAN

.ifndef NOMAN
MANALL=	\
	BN_CTX_new.cat3 \
	BN_CTX_start.cat3 \
	BN_add.cat3 \
	BN_add_word.cat3 \
	BN_bn2bin.cat3 \
	BN_cmp.cat3 \
	BN_copy.cat3 \
	BN_generate_prime.cat3 \
	BN_mod_inverse.cat3 \
	BN_mod_mul_montgomery.cat3 \
	BN_mod_mul_reciprocal.cat3 \
	BN_new.cat3 \
	BN_num_bytes.cat3 \
	BN_rand.cat3 \
	BN_set_bit.cat3 \
	BN_swap.cat3 \
	BN_zero.cat3 \
	BUF_MEM_new.cat3 \
	BUF_MEM_new.cat3 \
	CRYPTO_set_ex_data.cat3 \
	CRYPTO_set_locking_callback.cat3 \
	DH_generate_key.cat3 \
	DH_generate_parameters.cat3 \
	DH_get_ex_new_index.cat3 \
	DH_new.cat3 \
	DH_set_method.cat3 \
	DH_size.cat3 \
	DSA_SIG_new.cat3 \
	DSA_do_sign.cat3 \
	DSA_dup_DH.cat3 \
	DSA_generate_key.cat3 \
	DSA_generate_parameters.cat3 \
	DSA_get_ex_new_index.cat3 \
	DSA_new.cat3 \
	DSA_set_method.cat3 \
	DSA_sign.cat3 \
	DSA_size.cat3 \
	ERR_GET_LIB.cat3 \
	ERR_clear_error.cat3 \
	ERR_error_string.cat3 \
	ERR_get_error.cat3 \
	ERR_load_crypto_strings.cat3 \
	ERR_load_strings.cat3 \
	ERR_print_errors.cat3 \
	ERR_put_error.cat3 \
	ERR_remove_state.cat3 \
	EVP_BytesToKey.cat3 \
	EVP_DigestInit.cat3 \
	EVP_EncryptInit.cat3 \
	EVP_OpenInit.cat3 \
	EVP_SealInit.cat3 \
	EVP_SignInit.cat3 \
	EVP_VerifyInit.cat3 \
	HMAC.cat3 \
	MD5.cat3 \
	MDC2.cat3 \
	OPENSSL_VERSION_NUMBER.cat3 \
	OpenSSL_add_all_algorithms.cat3 \
	RAND_add.cat3 \
	RAND_bytes.cat3 \
	RAND_cleanup.cat3 \
	RAND_egd.cat3 \
	RAND_load_file.cat3 \
	RAND_set_rand_method.cat3 \
	RC4.cat3 \
	RIPEMD160.cat3 \
	RSA_blinding_on.cat3 \
	RSA_check_key.cat3 \
	RSA_generate_key.cat3 \
	RSA_get_ex_new_index.cat3 \
	RSA_new.cat3 \
	RSA_padding_add_PKCS1_type_1.cat3 \
	RSA_print.cat3 \
	RSA_private_encrypt.cat3 \
	RSA_public_encrypt.cat3 \
	RSA_set_method.cat3 \
	RSA_sign.cat3 \
	RSA_sign_ASN1_OCTET_STRING.cat3 \
	RSA_size.cat3 \
	SHA1.cat3 \
	SSL_CIPHER_get_name.cat3 \
	SSL_COMP_add_compression_method.cat3 \
	SSL_CTX_add_extra_chain_cert.cat3 \
	SSL_CTX_add_session.cat3 \
	SSL_CTX_ctrl.cat3 \
	SSL_CTX_flush_sessions.cat3 \
	SSL_CTX_free.cat3 \
	SSL_CTX_get_ex_new_index.cat3 \
	SSL_CTX_get_verify_mode.cat3 \
	SSL_CTX_load_verify_locations.cat3 \
	SSL_CTX_new.cat3 \
	SSL_CTX_sess_number.cat3 \
	SSL_CTX_sess_set_cache_size.cat3 \
	SSL_CTX_sess_set_get_cb.cat3 \
	SSL_CTX_sessions.cat3 \
	SSL_CTX_set_cert_store.cat3 \
	SSL_CTX_set_cert_verify_callback.cat3 \
	SSL_CTX_set_cipher_list.cat3 \
	SSL_CTX_set_client_CA_list.cat3 \
	SSL_CTX_set_client_cert_cb.cat3 \
	SSL_CTX_set_default_passwd_cb.cat3 \
	SSL_CTX_set_generate_session_id.cat3 \
	SSL_CTX_set_info_callback.cat3 \
	SSL_CTX_set_max_cert_list.cat3 \
	SSL_CTX_set_mode.cat3 \
	SSL_CTX_set_msg_callback.cat3 \
	SSL_CTX_set_options.cat3 \
	SSL_CTX_set_quiet_shutdown.cat3 \
	SSL_CTX_set_session_cache_mode.cat3 \
	SSL_CTX_set_session_id_context.cat3 \
	SSL_CTX_set_ssl_version.cat3 \
	SSL_CTX_set_timeout.cat3 \
	SSL_CTX_set_tmp_dh_callback.cat3 \
	SSL_CTX_set_tmp_rsa_callback.cat3 \
	SSL_CTX_set_verify.cat3 \
	SSL_CTX_use_certificate.cat3 \
	SSL_SESSION_free.cat3 \
	SSL_SESSION_get_ex_new_index.cat3 \
	SSL_SESSION_get_time.cat3 \
	SSL_accept.cat3 \
	SSL_alert_type_string.cat3 \
	SSL_clear.cat3 \
	SSL_connect.cat3 \
	SSL_free.cat3 \
	SSL_get_SSL_CTX.cat3 \
	SSL_get_ciphers.cat3 \
	SSL_get_client_CA_list.cat3 \
	SSL_get_current_cipher.cat3 \
	SSL_get_default_timeout.cat3 \
	SSL_get_error.cat3 \
	SSL_get_ex_data_X509_STORE_CTX_idx.cat3 \
	SSL_get_ex_new_index.cat3 \
	SSL_get_fd.cat3 \
	SSL_get_peer_cert_chain.cat3 \
	SSL_get_peer_certificate.cat3 \
	SSL_get_rbio.cat3 \
	SSL_get_session.cat3 \
	SSL_get_verify_result.cat3 \
	SSL_get_version.cat3 \
	SSL_library_init.cat3 \
	SSL_load_client_CA_file.cat3 \
	SSL_new.cat3 \
	SSL_pending.cat3 \
	SSL_read.cat3 \
	SSL_rstate_string.cat3 \
	SSL_session_reused.cat3 \
	SSL_set_bio.cat3 \
	SSL_set_connect_state.cat3 \
	SSL_set_fd.cat3 \
	SSL_set_session.cat3 \
	SSL_set_shutdown.cat3 \
	SSL_set_verify_result.cat3 \
	SSL_shutdown.cat3 \
	SSL_state_string.cat3 \
	SSL_want.cat3 \
	SSL_write.cat3 \
	blowfish.cat3 \
	bn.cat3 \
	bn_internal.cat3 \
	crypto.cat3 \
	d2i_DHparams.cat3 \
	d2i_RSAPublicKey.cat3 \
	d2i_SSL_SESSION.cat3 \
	des_modes.cat7 \
	des_random_key.cat3 \
	dh.cat3 \
	dsa.cat3 \
	lh_stats.cat3 \
	lhash.cat3 \
	openssl.cat1 \
	rsa.cat3 \
	ssl.cat3

.if MANPS
PSALL=	${MANALL:S/.cat1/.ps1/g:S/.cat2/.ps2/g:S/.cat3/.ps3/g:S/.cat4/.ps4/g:S/.cat5/.ps5/g:S/.cat6/.ps6/g:S/.cat7/.ps7/g:S/.cat8/.ps8/g:S/.cat9/.ps9/g}
.endif

# these are is a real problem, since they re-document functions described in
# other pages.
#
# err.pod -> ERR_get_error.pod
# ERR_peek_error ERR_get_error_line
# ERR_peek_error_line ERR_get_error_line_data ERR_peek_error_line_data
# ERR_GET_LIB ERR_GET_FUNC ERR_GET_REASON ERR_clear_error ERR_error_string
# ERR_lib_error_string ERR_func_error_string ERR_reason_error_string
# ERR_print_errors ERR_print_errors_fp ERR_load_crypto_strings ERR_free_strings
# ERR_remove_state ERR_put_error ERR_add_error_data ERR_load_strings ERR_PACK
# ERR_get_next_error_library
#
# rand.pod -> RAND_bytes.pod
# RAND_pseudo_bytes RAND_seed RAND_add RAND_status RAND_event
# RAND_screen RAND_load_file RAND_write_file RAND_file_name RAND_egd
# RAND_set_rand_method RAND_get_rand_method RAND_SSLeay RAND_cleanup

# buffer.pod -> BUF_MEM_new.pod
# BUF_MEM_free BUF_MEM_grow BUF_strdup
BUF_MEM_new.cat3: buffer.pod
	( cp ${.ALLSRC} BUF_MEM_new.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  BUF_MEM_new.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
BUF_MEM_new.ps3: buffer.pod
	( cp ${.ALLSRC} BUF_MEM_new.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  BUF_MEM_new.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# rc4.pod -> RC4.pod
# RC4_set_key
RC4.cat3: rc4.pod
	( cp ${.ALLSRC} RC4.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  RC4.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
RC4.ps3: rc4.pod
	( cp ${.ALLSRC} RC4.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  RC4.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# threads.pod -> CRYPTO_set_locking_callback.pod
# CRYPTO_set_id_callback CRYPTO_num_locks
CRYPTO_set_locking_callback.cat3: threads.pod
	( cp ${.ALLSRC} CRYPTO_set_locking_callback.pm && \
	  pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  CRYPTO_set_locking_callback.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
CRYPTO_set_locking_callback.ps3: threads.pod
	( cp ${.ALLSRC} CRYPTO_set_locking_callback.pm && \
	  pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  CRYPTO_set_locking_callback.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# ripemd.pod -> RIPEMD160.pod
# RIPEMD160_Init RIPEMD160_Update RIPEMD160_Final
RIPEMD160.cat3: ripemd.pod
	( cp ${.ALLSRC} RIPEMD160.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  RIPEMD160.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
RIPEMD160.ps3: ripemd.pod
	( cp ${.ALLSRC} RIPEMD160.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  RIPEMD160.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# sha.pod -> SHA1.pod
SHA1.cat3: sha.pod
	( cp ${.ALLSRC} SHA1.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  SHA1.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
SHA1.ps3: sha.pod
	( cp ${.ALLSRC} SHA1.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  SHA1.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# md5.pod -> MD5.pod
MD5.cat3: md5.pod
	( cp ${.ALLSRC} MD5.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  MD5.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
MD5.ps3: md5.pod
	( cp ${.ALLSRC} MD5.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  MD5.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# mdc2.pod -> MDC2.pod
MDC2.cat3: mdc2.pod
	( cp ${.ALLSRC} MDC2.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  MDC2.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
MDC2.ps3: mdc2.pod
	( cp ${.ALLSRC} MDC2.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  MDC2.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# hmac.pod -> HMAC.pod
HMAC.cat3: hmac.pod
	( cp ${.ALLSRC} HMAC.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  HMAC.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
HMAC.ps3: hmac.pod
	( cp ${.ALLSRC} HMAC.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  HMAC.pm ) | nroff -Tps -man > ${.TARGET}
.endif

# des.pod -> des_random_key, des_set_key, des_key_sched, des_set_key_checked,
# des_set_key_unchecked, des_set_odd_parity, des_is_weak_key, des_ecb_encrypt,
# des_ecb2_encrypt, des_ecb3_encrypt, des_ncbc_encrypt, des_cfb_encrypt,
# des_ofb_encrypt, des_pcbc_encrypt, des_cfb64_encrypt, des_ofb64_encrypt,
# des_xcbc_encrypt, des_ede2_cbc_encrypt, des_ede2_cfb64_encrypt,
# des_ede2_ofb64_encrypt, des_ede3_cbc_encrypt, des_ede3_cbcm_encrypt,
# des_ede3_cfb64_encrypt, des_ede3_ofb64_encrypt, des_read_password,
# des_read_2passwords, des_read_pw_string, des_cbc_cksum, des_quad_cksum,
# des_string_to_key, des_string_to_2keys, des_fcrypt, des_crypt,
# des_enc_read, des_enc_write
des_random_key.cat3: des.pod
	( cp ${.ALLSRC} des_random_key.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  des_random_key.pm ) | nroff -Tascii -man > ${.TARGET}
.if MANPS
des_random_key.ps3: des.pod
	( cp ${.ALLSRC} des_random_key.pm && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  des_random_key.pm ) | nroff -Tps -man > ${.TARGET}
.endif

MLINKS+=\
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
	EVP_DigestInit.3 EVP_DigestFinal.3 \
	EVP_DigestInit.3 EVP_DigestUpdate.3 \
	EVP_DigestInit.3 EVP_MD_CTX_block_size.3 \
	EVP_DigestInit.3 EVP_MD_CTX_copy.3 \
	EVP_DigestInit.3 EVP_MD_CTX_md.3 \
	EVP_DigestInit.3 EVP_MD_CTX_size.3 \
	EVP_DigestInit.3 EVP_MD_CTX_type.3 \
	EVP_DigestInit.3 EVP_MD_block_size.3 \
	EVP_DigestInit.3 EVP_MD_block_size.3 \
	EVP_DigestInit.3 EVP_MD_pkey_type.3 \
	EVP_DigestInit.3 EVP_MD_size.3 \
	EVP_DigestInit.3 EVP_MD_size.3 \
	EVP_DigestInit.3 EVP_MD_type.3 \
	EVP_DigestInit.3 EVP_MD_type.3 \
	EVP_DigestInit.3 EVP_dss.3 \
	EVP_DigestInit.3 EVP_dss1.3 \
	EVP_DigestInit.3 EVP_get_digestbyname.3 \
	EVP_DigestInit.3 EVP_get_digestbyname.3 \
	EVP_DigestInit.3 EVP_get_digestbynid.3 \
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
	EVP_EncryptInit.3 EVP_get_cipherbyname.3 \
	EVP_EncryptInit.3 EVP_get_cipherbynid.3 \
	EVP_EncryptInit.3 EVP_get_cipherbynid.3 \
	EVP_EncryptInit.3 EVP_get_cipherbyobj.3 \
	EVP_EncryptInit.3 OBJ_nid2sn.3 \
	EVP_EncryptInit.3 OBJ_obj2nid.3 \
	EVP_OpenInit.3 EVP_OpenFinal.3 \
	EVP_OpenInit.3 EVP_OpenUpdate.3 \
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
	MDC2.3 MDC2_Final.3 \
	MDC2.3 MDC2_Init.3 \
	MDC2.3 MDC2_Update.3 \
	OPENSSL_VERSION_NUMBER.3 SSLeay.3 \
	OPENSSL_VERSION_NUMBER.3 SSLeay_version.3 \
	OpenSSL_add_all_algorithms.3 EVP_cleanup.3 \
	OpenSSL_add_all_algorithms.3 OpenSSL_add_all_ciphers.3 \
	OpenSSL_add_all_algorithms.3 OpenSSL_add_all_digests.3 \
	RAND_add.3 RAND_event.3 \
	RAND_add.3 RAND_screen.3 \
	RAND_add.3 RAND_seed.3 \
	RAND_add.3 RAND_status.3 \
	RAND_bytes.3 RAND_pseudo_bytes.3 \
	RAND_bytes.3 RAND_pseudo_bytes.3 \
	RAND_egd.3 RAND_egd_bytes.3 \
	RAND_load_file.3 RAND_file_name.3 \
	RAND_load_file.3 RAND_file_name.3 \
	RAND_load_file.3 RAND_write_file.3 \
	RAND_load_file.3 RAND_write_file.3 \
	RAND_set_rand_method.3 RAND_SSLeay.3 \
	RAND_set_rand_method.3 RAND_SSLeay.3 \
	RAND_set_rand_method.3 RAND_get_rand_method.3 \
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
	RSA_print.3 DHparams_print.3 \
	RSA_print.3 DHparams_print_fp.3 \
	RSA_print.3 DHparams_print_fp.3 \
	RSA_print.3 DSA_print.3 \
	RSA_print.3 DSA_print_fp.3 \
	RSA_print.3 DSA_print_fp.3 \
	RSA_print.3 DSAparams_print.3 \
	RSA_print.3 DSAparams_print.3 \
	RSA_print.3 DSAparams_print_fp.3 \
	RSA_print.3 DSAparams_print_fp.3 \
	RSA_print.3 RSA_print_fp.3 \
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
	blowfish.3 BF_cbc.3 \
	blowfish.3 BF_cbc_encrypt.3 \
	blowfish.3 BF_cfb64_encrypt.3 \
	blowfish.3 BF_decrypt.3 \
	blowfish.3 BF_ecb.3 \
	blowfish.3 BF_ecb_encrypt.3 \
	blowfish.3 BF_encrypt.3 \
	blowfish.3 BF_ofb64_encrypt.3 \
	blowfish.3 BF_options.3 \
	blowfish.3 BF_set_key.3 \
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
	d2i_DHparams.3 i2d_DHparams.3 \
	d2i_RSAPublicKey.3 d2i_Netscape_RSA.3 \
	d2i_RSAPublicKey.3 d2i_RSAPrivateKey.3 \
	d2i_RSAPublicKey.3 i2d_Netscape_RSA.3 \
	d2i_RSAPublicKey.3 i2d_RSAPrivateKey.3 \
	d2i_RSAPublicKey.3 i2d_RSAPublicKey.3 \
	d2i_SSL_SESSION.3 i2d_SSL_SESSION.3 \
	des_crypt.3 des_string_to_2keys.3 \
	des_random_key.3 des_cbc_cksum.3 \
	des_random_key.3 des_cfb64_encrypt.3 \
	des_random_key.3 des_cfb_encrypt.3 \
	des_random_key.3 des_crypt.3 \
	des_random_key.3 des_ecb2_encrypt.3 \
	des_random_key.3 des_ecb3_encrypt.3 \
	des_random_key.3 des_ecb_encrypt.3 \
	des_random_key.3 des_ede2_cbc_encrypt.3 \
	des_random_key.3 des_ede2_cfb64_encrypt.3 \
	des_random_key.3 des_ede2_ofb64_encrypt.3 \
	des_random_key.3 des_ede3_cbc_encrypt.3 \
	des_random_key.3 des_ede3_cbcm_encrypt.3 \
	des_random_key.3 des_ede3_cfb64_encrypt.3 \
	des_random_key.3 des_ede3_ofb64_encrypt.3 \
	des_random_key.3 des_enc_read.3 \
	des_random_key.3 des_enc_write.3 \
	des_random_key.3 des_fcrypt.3 \
	des_random_key.3 des_is_weak_key.3 \
	des_random_key.3 des_key_sched.3 \
	des_random_key.3 des_ncbc_encrypt.3 \
	des_random_key.3 des_ofb64_encrypt.3 \
	des_random_key.3 des_ofb_encrypt.3 \
	des_random_key.3 des_pcbc_encrypt.3 \
	des_random_key.3 des_quad_cksum.3 \
	des_random_key.3 des_read_2passwords.3 \
	des_random_key.3 des_read_password.3 \
	des_random_key.3 des_read_pw_string.3 \
	des_random_key.3 des_set_key.3 \
	des_random_key.3 des_set_key_checked.3 \
	des_random_key.3 des_set_key_unchecked.3 \
	des_random_key.3 des_set_odd_parity.3 \
	des_random_key.3 des_string_to_2keys.3 \
	des_random_key.3 des_string_to_key.3 \
	des_random_key.3 des_xcbc_encrypt.3 \
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
	dsa.3 d2i_DSAPrivateKey.3 \
	dsa.3 d2i_DSAPublicKey.3 \
	dsa.3 d2i_DSA_SIG.3 \
	dsa.3 d2i_DSAparams.3 \
	dsa.3 i2d_DSAPrivateKey.3 \
	dsa.3 i2d_DSAPublicKey.3 \
	dsa.3 i2d_DSA_SIG.3 \
	dsa.3 i2d_DSAparams.3 \
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
	lhash.3 lh_retrieve.3

.include <bsd.man.mk>
.else
maninstall:

.endif

# XXX .PATH order is critical because of non-unique filenames
.PATH: ${.CURDIR}/../src/doc/crypto ${.CURDIR}/../src/doc/ssl ${.CURDIR}/../src/doc/apps
.SUFFIXES: .pod
.pod.cat3:
	( cd `dirname ${.ALLSRC}` && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  `basename ${.ALLSRC}` ) | nroff -Tascii -man > ${.TARGET}
.pod.cat7:
	( cd `dirname ${.ALLSRC}` && pod2man --section=7 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  `basename ${.ALLSRC}` ) | nroff -Tascii -man > ${.TARGET}
.pod.cat1:
	( cd `dirname ${.ALLSRC}` && pod2man --section=1 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  `basename ${.ALLSRC}` ) | nroff -Tascii -man > ${.TARGET}

.pod.ps3:
	( cd `dirname ${.ALLSRC}` && pod2man --section=3 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  `basename ${.ALLSRC}` ) | nroff -Tps -man > ${.TARGET}
.pod.ps7:
	( cd `dirname ${.ALLSRC}` && pod2man --section=7 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  `basename ${.ALLSRC}` ) | nroff -Tps -man > ${.TARGET}
.pod.ps1:
	( cd `dirname ${.ALLSRC}` && pod2man --section=1 --official \
	  --center='OpenSSL' --release="OpenBSD `uname -r`" \
	  `basename ${.ALLSRC}` ) | nroff -Tps -man > ${.TARGET}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

CLEANFILES+=\
	BUF_MEM_new.pm \
	CRYPTO_set_locking_callback.pm \
	HMAC.pm \
	MDC2.pm \
	RC4.pm \
	RIPEMD160.pm \
	SHA1.pm \
	des_random_key.pm \
	MD5.pm \

clean:
	rm -f ${CLEANFILES}
