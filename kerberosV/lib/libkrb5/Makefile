# $OpenBSD: Makefile,v 1.8 2013/11/03 11:56:40 ajacoutot Exp $

LIB=	krb5

INCS=	heim_err.h \
	heim_threads.h \
	k524_err.h \
	krb_err.h \
	krb5_err.h \
	krb5-protos.h \
	krb5-private.h \
	krb5-types.h \
	krb5.h \
	krb5_err.h \
	krb5_ccapi.h

KRB5INCS=	locate_plugin.h \
		send_to_kdc_plugin.h \
		ccache_plugin.h
KRB5INCSDIR=	${INCSDIR}/krb5
INCSGROUPS=	INCS KRB5INCS

MPAGES=	krb524_convert_creds_kdc.3 \
	krb5_425_conv_principal.3 \
	krb5_acl_match_file.3 \
	krb5_aname_to_localname.3 \
	krb5_appdefault.3 \
	krb5_auth_context.3 \
	krb5_c_make_checksum.3 \
	krb5_check_transited.3 \
	krb5_create_checksum.3 \
	krb5_creds.3 \
	krb5_digest.3 \
	krb5_eai_to_heim_errno.3 \
	krb5_encrypt.3 \
	krb5_find_padata.3 \
	krb5_generate_random_block.3 \
	krb5_get_all_client_addrs.3 \
	krb5_get_credentials.3 \
	krb5_get_creds.3 \
	krb5_get_forwarded_creds.3 \
	krb5_get_in_cred.3 \
	krb5_get_init_creds.3 \
	krb5_get_krbhst.3 \
	krb5_getportbyname.3 \
	krb5_init_context.3 \
	krb5_is_thread_safe.3 \
	krb5_krbhst_init.3 \
	krb5_mk_req.3 \
	krb5_mk_safe.3 \
	krb5_openlog.3 \
	krb5_parse_name.3 \
	krb5_principal.3 \
	krb5_rcache.3 \
	krb5_rd_error.3 \
	krb5_rd_safe.3 \
	krb5_set_default_realm.3 \
	krb5_set_password.3 \
	krb5_string_to_key.3 \
	krb5_timeofday.3 \
	krb5_verify_init_creds.3 \
	krb5_verify_user.3

MPAGES+=	krb5.conf.5
MPAGES+=	kerberos.8

MSRC=	${KRB5DIR}/lib/krb5 \
	${KRB5DIR}/doc/doxyout/krb5/man/man3

MLINKS= \
	krb524_convert_creds_kdc.3 krb524_convert_creds_kdc_ccache.3 \
	krb5_425_conv_principal.3 krb5_425_conv_principal_ext.3 \
	krb5_425_conv_principal.3 krb5_524_conv_principal.3 \
	krb5_acl_match_file.3 krb5_acl_match_string.3 \
	krb5_appdefault.3 krb5_appdefault_boolean.3 \
	krb5_appdefault.3 krb5_appdefault_string.3 \
	krb5_appdefault.3 krb5_appdefault_time.3 \
	krb5_auth_context.3 krb5_auth_con_addflags.3 \
	krb5_auth_context.3 krb5_auth_con_free.3 \
	krb5_auth_context.3 krb5_auth_con_genaddrs.3 \
	krb5_auth_context.3 krb5_auth_con_generatelocalsubkey.3 \
	krb5_auth_context.3 krb5_auth_con_getaddrs.3 \
	krb5_auth_context.3 krb5_auth_con_getauthenticator.3 \
	krb5_auth_context.3 krb5_auth_con_getflags.3 \
	krb5_auth_context.3 krb5_auth_con_getkey.3 \
	krb5_auth_context.3 krb5_auth_con_getlocalsubkey.3 \
	krb5_auth_context.3 krb5_auth_con_getrcache.3 \
	krb5_auth_context.3 krb5_auth_con_getremotesubkey.3 \
	krb5_auth_context.3 krb5_auth_con_getuserkey.3 \
	krb5_auth_context.3 krb5_auth_con_init.3 \
	krb5_auth_context.3 krb5_auth_con_initivector.3 \
	krb5_auth_context.3 krb5_auth_con_removeflags.3 \
	krb5_auth_context.3 krb5_auth_con_setaddrs.3 \
	krb5_auth_context.3 krb5_auth_con_setaddrs_from_fd.3 \
	krb5_auth_context.3 krb5_auth_con_setflags.3 \
	krb5_auth_context.3 krb5_auth_con_setivector.3 \
	krb5_auth_context.3 krb5_auth_con_setkey.3 \
	krb5_auth_context.3 krb5_auth_con_setlocalsubkey.3 \
	krb5_auth_context.3 krb5_auth_con_setrcache.3 \
	krb5_auth_context.3 krb5_auth_con_setremotesubkey.3 \
	krb5_auth_context.3 krb5_auth_con_setuserkey.3 \
	krb5_auth_context.3 krb5_auth_getcksumtype.3 \
	krb5_auth_context.3 krb5_auth_getkeytype.3 \
	krb5_auth_context.3 krb5_auth_getlocalseqnumber.3 \
	krb5_auth_context.3 krb5_auth_getremoteseqnumber.3 \
	krb5_auth_context.3 krb5_auth_setcksumtype.3 \
	krb5_auth_context.3 krb5_auth_setkeytype.3 \
	krb5_auth_context.3 krb5_auth_setlocalseqnumber.3 \
	krb5_auth_context.3 krb5_auth_setremoteseqnumber.3 \
	krb5_auth_context.3 krb5_free_authenticator.3 \
	krb5_c_make_checksum.3 krb5_c_block_size.3 \
	krb5_c_make_checksum.3 krb5_c_checksum_length.3 \
	krb5_c_make_checksum.3 krb5_c_decrypt.3 \
	krb5_c_make_checksum.3 krb5_c_encrypt.3 \
	krb5_c_make_checksum.3 krb5_c_encrypt_length.3 \
	krb5_c_make_checksum.3 krb5_c_enctype_compare.3 \
	krb5_c_make_checksum.3 krb5_c_get_checksum.3 \
	krb5_c_make_checksum.3 krb5_c_is_coll_proof_cksum.3 \
	krb5_c_make_checksum.3 krb5_c_is_keyed_cksum.3 \
	krb5_c_make_checksum.3 krb5_c_keylength.3 \
	krb5_c_make_checksum.3 krb5_c_make_random_key.3 \
	krb5_c_make_checksum.3 krb5_c_set_checksum.3 \
	krb5_c_make_checksum.3 krb5_c_valid_cksumtype.3 \
	krb5_c_make_checksum.3 krb5_c_valid_enctype.3 \
	krb5_c_make_checksum.3 krb5_c_verify_checksum.3 \
	krb5_check_transited.3 krb5_check_transited_realms.3 \
	krb5_check_transited.3 krb5_domain_x500_decode.3 \
	krb5_check_transited.3 krb5_domain_x500_encode.3 \
	krb5_create_checksum.3 krb5_checksum.3 \
	krb5_create_checksum.3 krb5_checksum_disable.3 \
	krb5_create_checksum.3 krb5_checksum_is_collision_proof.3 \
	krb5_create_checksum.3 krb5_checksum_is_keyed.3 \
	krb5_create_checksum.3 krb5_checksumsize.3 \
	krb5_create_checksum.3 krb5_cksumtype_valid.3 \
	krb5_create_checksum.3 krb5_copy_checksum.3 \
	krb5_create_checksum.3 krb5_crypto_get_checksum_type.3 \
	krb5_create_checksum.3 krb5_free_checksum.3 \
	krb5_create_checksum.3 krb5_free_checksum_contents.3 \
	krb5_create_checksum.3 krb5_hmac.3 \
	krb5_create_checksum.3 krb5_verify_checksum.3 \
	krb5_creds.3 krb5_copy_creds.3 \
	krb5_creds.3 krb5_copy_creds_contents.3 \
	krb5_creds.3 krb5_free_cred_contents.3 \
	krb5_creds.3 krb5_free_creds.3 \
	krb5_digest.3 krb5_digest_alloc.3 \
	krb5_digest.3 krb5_digest_free.3 \
	krb5_digest.3 krb5_digest_get_a1_hash.3 \
	krb5_digest.3 krb5_digest_get_client_binding.3 \
	krb5_digest.3 krb5_digest_get_identifier.3 \
	krb5_digest.3 krb5_digest_get_opaque.3 \
	krb5_digest.3 krb5_digest_get_responseData.3 \
	krb5_digest.3 krb5_digest_get_rsp.3 \
	krb5_digest.3 krb5_digest_get_server_nonce.3 \
	krb5_digest.3 krb5_digest_get_tickets.3 \
	krb5_digest.3 krb5_digest_init_request.3 \
	krb5_digest.3 krb5_digest_request.3 \
	krb5_digest.3 krb5_digest_set_authentication_user.3 \
	krb5_digest.3 krb5_digest_set_authid.3 \
	krb5_digest.3 krb5_digest_set_client_nonce.3 \
	krb5_digest.3 krb5_digest_set_digest.3 \
	krb5_digest.3 krb5_digest_set_hostname.3 \
	krb5_digest.3 krb5_digest_set_identifier.3 \
	krb5_digest.3 krb5_digest_set_method.3 \
	krb5_digest.3 krb5_digest_set_nonceCount.3 \
	krb5_digest.3 krb5_digest_set_opaque.3 \
	krb5_digest.3 krb5_digest_set_qop.3 \
	krb5_digest.3 krb5_digest_set_realm.3 \
	krb5_digest.3 krb5_digest_set_server_cb.3 \
	krb5_digest.3 krb5_digest_set_server_nonce.3 \
	krb5_digest.3 krb5_digest_set_type.3 \
	krb5_digest.3 krb5_digest_set_uri.3 \
	krb5_digest.3 krb5_digest_set_username.3 \
	krb5_eai_to_heim_errno.3 krb5_h_errno_to_heim_errno.3 \
	krb5_encrypt.3 krb5_crypto_getblocksize.3 \
	krb5_encrypt.3 krb5_crypto_getconfoundersize.3 \
	krb5_encrypt.3 krb5_crypto_getenctype.3 \
	krb5_encrypt.3 krb5_crypto_getpadsize.3 \
	krb5_encrypt.3 krb5_crypto_overhead.3 \
	krb5_encrypt.3 krb5_decrypt.3 \
	krb5_encrypt.3 krb5_decrypt_EncryptedData.3 \
	krb5_encrypt.3 krb5_decrypt_ivec.3 \
	krb5_encrypt.3 krb5_decrypt_ticket.3 \
	krb5_encrypt.3 krb5_encrypt_EncryptedData.3 \
	krb5_encrypt.3 krb5_encrypt_ivec.3 \
	krb5_encrypt.3 krb5_enctype_disable.3 \
	krb5_encrypt.3 krb5_enctype_keysize.3 \
	krb5_encrypt.3 krb5_enctype_to_string.3 \
	krb5_encrypt.3 krb5_enctype_valid.3 \
	krb5_encrypt.3 krb5_get_wrapped_length.3 \
	krb5_encrypt.3 krb5_string_to_enctype.3 \
	krb5_find_padata.3 krb5_padata_add.3 \
	krb5_get_all_client_addrs.3 krb5_get_all_server_addrs.3 \
	krb5_get_credentials.3 krb5_get_cred_from_kdc.3 \
	krb5_get_credentials.3 krb5_get_cred_from_kdc_opt.3 \
	krb5_get_credentials.3 krb5_get_credentials_with_flags.3 \
	krb5_get_credentials.3 krb5_get_kdc_cred.3 \
	krb5_get_credentials.3 krb5_get_renewed_creds.3 \
	krb5_get_creds.3 krb5_get_creds_opt_add_options.3 \
	krb5_get_creds.3 krb5_get_creds_opt_alloc.3 \
	krb5_get_creds.3 krb5_get_creds_opt_free.3 \
	krb5_get_creds.3 krb5_get_creds_opt_set_enctype.3 \
	krb5_get_creds.3 krb5_get_creds_opt_set_impersonate.3 \
	krb5_get_creds.3 krb5_get_creds_opt_set_options.3 \
	krb5_get_creds.3 krb5_get_creds_opt_set_ticket.3 \
	krb5_get_forwarded_creds.3 krb5_fwd_tgt_creds.3 \
	krb5_get_in_cred.3 krb5_free_kdc_rep.3 \
	krb5_get_in_cred.3 krb5_get_in_tkt.3 \
	krb5_get_in_cred.3 krb5_get_in_tkt_with_keytab.3 \
	krb5_get_in_cred.3 krb5_get_in_tkt_with_password.3 \
	krb5_get_in_cred.3 krb5_get_in_tkt_with_skey.3 \
	krb5_get_in_cred.3 krb5_password_key_proc.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_keytab.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_alloc.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_free.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_init.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_address_list.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_addressless.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_anonymous.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_canonicalize.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_default_flags.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_etype_list.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_forwardable.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_pa_password.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_paq_request.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_preauth_list.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_proxiable.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_renew_life.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_salt.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_tkt_life.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_opt_set_win2k.3 \
	krb5_get_init_creds.3 krb5_get_init_creds_password.3 \
	krb5_get_init_creds.3 krb5_prompt.3 \
	krb5_get_init_creds.3 krb5_prompter_posix.3 \
	krb5_get_krbhst.3 krb5_free_krbhst.3 \
	krb5_get_krbhst.3 krb5_get_krb524hst.3 \
	krb5_get_krbhst.3 krb5_get_krb_admin_hst.3 \
	krb5_get_krbhst.3 krb5_get_krb_changepw_hst.3 \
	krb5_init_context.3 krb5_add_et_list.3 \
	krb5_init_context.3 krb5_add_extra_addresses.3 \
	krb5_init_context.3 krb5_add_ignore_addresses.3 \
	krb5_init_context.3 krb5_context.3 \
	krb5_init_context.3 krb5_free_config_files.3 \
	krb5_init_context.3 krb5_free_context.3 \
	krb5_init_context.3 krb5_get_default_config_files.3 \
	krb5_init_context.3 krb5_get_dns_canonize_hostname.3 \
	krb5_init_context.3 krb5_get_extra_addresses.3 \
	krb5_init_context.3 krb5_get_fcache_version.3 \
	krb5_init_context.3 krb5_get_ignore_addresses.3 \
	krb5_init_context.3 krb5_get_kdc_sec_offset.3 \
	krb5_init_context.3 krb5_get_max_time_skew.3 \
	krb5_init_context.3 krb5_get_use_admin_kdc.3 \
	krb5_init_context.3 krb5_init_ets.3 \
	krb5_init_context.3 krb5_prepend_config_files.3 \
	krb5_init_context.3 krb5_prepend_config_files_default.3 \
	krb5_init_context.3 krb5_set_config_files.3 \
	krb5_init_context.3 krb5_set_dns_canonize_hostname.3 \
	krb5_init_context.3 krb5_set_extra_addresses.3 \
	krb5_init_context.3 krb5_set_fcache_version.3 \
	krb5_init_context.3 krb5_set_ignore_addresses.3 \
	krb5_init_context.3 krb5_set_max_time_skew.3 \
	krb5_init_context.3 krb5_set_use_admin_kdc.3 \
	krb5_krbhst_init.3 krb5_krbhst_format_string.3 \
	krb5_krbhst_init.3 krb5_krbhst_free.3 \
	krb5_krbhst_init.3 krb5_krbhst_get_addrinfo.3 \
	krb5_krbhst_init.3 krb5_krbhst_init_flags.3 \
	krb5_krbhst_init.3 krb5_krbhst_next.3 \
	krb5_krbhst_init.3 krb5_krbhst_next_as_string.3 \
	krb5_krbhst_init.3 krb5_krbhst_reset.3 \
	krb5_mk_safe.3 krb5_mk_priv.3 \
	krb5_mk_req.3 krb5_build_ap_req.3 \
	krb5_mk_req.3 krb5_mk_rep.3 \
	krb5_mk_req.3 krb5_mk_rep_exact.3 \
	krb5_mk_req.3 krb5_mk_rep_extended.3 \
	krb5_mk_req.3 krb5_mk_req_exact.3 \
	krb5_mk_req.3 krb5_mk_req_extended.3 \
	krb5_mk_req.3 krb5_rd_rep.3 \
	krb5_mk_req.3 krb5_rd_req.3 \
	krb5_mk_req.3 krb5_rd_req_with_keyblock.3 \
	krb5_mk_req.3 krb5_verify_ap_req.3 \
	krb5_openlog.3 krb5_addlog_dest.3 \
	krb5_openlog.3 krb5_addlog_func.3 \
	krb5_openlog.3 krb5_closelog.3 \
	krb5_openlog.3 krb5_initlog.3 \
	krb5_openlog.3 krb5_log.3 \
	krb5_openlog.3 krb5_log_msg.3 \
	krb5_openlog.3 krb5_vlog.3 \
	krb5_openlog.3 krb5_vlog_msg.3 \
	krb5_principal.3 krb5_build_principal.3 \
	krb5_principal.3 krb5_build_principal_ext.3 \
	krb5_principal.3 krb5_build_principal_va.3 \
	krb5_principal.3 krb5_build_principal_va_ext.3 \
	krb5_principal.3 krb5_copy_principal.3 \
	krb5_principal.3 krb5_free_principal.3 \
	krb5_principal.3 krb5_get_default_principal.3 \
	krb5_principal.3 krb5_make_principal.3 \
	krb5_principal.3 krb5_parse.3 \
	krb5_principal.3 krb5_parse_name_flags.3 \
	krb5_principal.3 krb5_parse_nametype.3 \
	krb5_principal.3 krb5_princ_realm.3 \
	krb5_principal.3 krb5_princ_set_realm.3 \
	krb5_principal.3 krb5_principal_compare.3 \
	krb5_principal.3 krb5_principal_compare_any_realm.3 \
	krb5_principal.3 krb5_principal_get_comp_string.3 \
	krb5_principal.3 krb5_principal_get_realm.3 \
	krb5_principal.3 krb5_principal_get_type.3 \
	krb5_principal.3 krb5_principal_match.3 \
	krb5_principal.3 krb5_principal_set_type.3 \
	krb5_principal.3 krb5_realm_compare.3 \
	krb5_principal.3 krb5_sname_to_principal.3 \
	krb5_principal.3 krb5_sock_to_principal.3 \
	krb5_principal.3 krb5_unparse_name.3 \
	krb5_principal.3 krb5_unparse_name_fixed.3 \
	krb5_principal.3 krb5_unparse_name_fixed_flags.3 \
	krb5_principal.3 krb5_unparse_name_fixed_short.3 \
	krb5_principal.3 krb5_unparse_name_flags.3 \
	krb5_principal.3 krb5_unparse_name_short.3 \
	krb5_rcache.3 krb5_get_server_rcache.3 \
	krb5_rcache.3 krb5_rc_close.3 \
	krb5_rcache.3 krb5_rc_default.3 \
	krb5_rcache.3 krb5_rc_default_name.3 \
	krb5_rcache.3 krb5_rc_default_type.3 \
	krb5_rcache.3 krb5_rc_destroy.3 \
	krb5_rcache.3 krb5_rc_expunge.3 \
	krb5_rcache.3 krb5_rc_get_lifespan.3 \
	krb5_rcache.3 krb5_rc_get_name.3 \
	krb5_rcache.3 krb5_rc_get_type.3 \
	krb5_rcache.3 krb5_rc_initialize.3 \
	krb5_rcache.3 krb5_rc_recover.3 \
	krb5_rcache.3 krb5_rc_resolve.3 \
	krb5_rcache.3 krb5_rc_resolve_full.3 \
	krb5_rcache.3 krb5_rc_resolve_type.3 \
	krb5_rcache.3 krb5_rc_store.3 \
	krb5_rd_error.3 krb5_error_from_rd_error.3 \
	krb5_rd_error.3 krb5_free_error.3 \
	krb5_rd_error.3 krb5_free_error_contents.3 \
	krb5_rd_safe.3 krb5_rd_priv.3 \
	krb5_set_default_realm.3 krb5_copy_host_realm.3 \
	krb5_set_default_realm.3 krb5_free_host_realm.3 \
	krb5_set_default_realm.3 krb5_get_default_realm.3 \
	krb5_set_default_realm.3 krb5_get_default_realms.3 \
	krb5_set_default_realm.3 krb5_get_host_realm.3 \
	krb5_set_password.3 krb5_change_password.3 \
	krb5_set_password.3 krb5_passwd_result_to_string.3 \
	krb5_set_password.3 krb5_set_password_using_ccache.3 \
	krb5_string_to_key.3 krb5_free_salt.3 \
	krb5_string_to_key.3 krb5_get_pw_salt.3 \
	krb5_string_to_key.3 krb5_string_to_key_data.3 \
	krb5_string_to_key.3 krb5_string_to_key_data_salt.3 \
	krb5_string_to_key.3 krb5_string_to_key_data_salt_opaque.3 \
	krb5_string_to_key.3 krb5_string_to_key_salt.3 \
	krb5_string_to_key.3 krb5_string_to_key_salt_opaque.3 \
	krb5_timeofday.3 krb5_format_time.3 \
	krb5_timeofday.3 krb5_set_real_time.3 \
	krb5_timeofday.3 krb5_string_to_deltat.3 \
	krb5_timeofday.3 krb5_us_timeofday.3 \
	krb5_verify_init_creds.3 krb5_verify_init_creds_opt_init.3 \
	krb5_verify_init_creds.3 krb5_verify_init_creds_opt_set_ap_req_nofail.3 \
	krb5_verify_user.3 krb5_verify_opt_alloc.3 \
	krb5_verify_user.3 krb5_verify_opt_free.3 \
	krb5_verify_user.3 krb5_verify_opt_init.3 \
	krb5_verify_user.3 krb5_verify_opt_set_ccache.3 \
	krb5_verify_user.3 krb5_verify_opt_set_flags.3 \
	krb5_verify_user.3 krb5_verify_opt_set_keytab.3 \
	krb5_verify_user.3 krb5_verify_opt_set_secure.3 \
	krb5_verify_user.3 krb5_verify_opt_set_service.3 \
	krb5_verify_user.3 krb5_verify_user_lrealm.3 \
	krb5_verify_user.3 krb5_verify_user_opt.3

SRCS=	acache.c \
	acl.c \
	add_et_list.c \
	addr_families.c \
	aname_to_localname.c \
	appdefault.c \
	asn1_glue.c \
	auth_context.c \
	build_ap_req.c \
	build_auth.c \
	cache.c \
	changepw.c \
	codec.c \
	config_file.c \
	constants.c \
	context.c \
	convert_creds.c \
	copy_host_realm.c \
	crc.c \
	creds.c \
	crypto-aes.c \
	crypto-algs.c \
	crypto-arcfour.c \
	crypto-des-common.c \
	crypto-des.c \
	crypto-des3.c \
	crypto-evp.c \
	crypto-null.c \
	crypto-rand.c \
	crypto.c \
	data.c \
	deprecated.c \
	digest.c \
	doxygen.c \
	eai_to_heim_errno.c \
	error_string.c \
	expand_hostname.c \
	expand_path.c \
	fcache.c \
	free.c \
	free_host_realm.c \
	generate_seq_number.c \
	generate_subkey.c \
	get_addrs.c \
	get_cred.c \
	get_default_principal.c \
	get_default_realm.c \
	get_for_creds.c \
	get_host_realm.c \
	get_in_tkt.c \
	get_port.c \
	init_creds.c \
	init_creds_pw.c \
	kcm.c \
	keyblock.c \
	keytab.c \
	keytab_any.c \
	keytab_file.c \
	keytab_keyfile.c \
	keytab_memory.c \
	krbhst.c \
	kuserok.c \
	log.c \
	mcache.c \
	misc.c \
	mit_glue.c \
	mk_error.c \
	mk_priv.c \
	mk_rep.c \
	mk_req.c \
	mk_req_ext.c \
	mk_safe.c \
	n-fold.c \
	net_read.c \
	net_write.c \
	pac.c \
	padata.c \
	pcache.c \
	pkinit.c \
	plugin.c \
	principal.c \
	prog_setup.c \
	prompter_posix.c \
	rd_cred.c \
	rd_error.c \
	rd_priv.c \
	rd_rep.c \
	rd_req.c \
	rd_safe.c \
	read_message.c \
	recvauth.c \
	replay.c \
	salt-aes.c \
	salt-arcfour.c \
	salt-des.c \
	salt-des3.c \
	salt.c \
	send_to_kdc.c \
	sendauth.c \
	set_default_realm.c \
	sock_principal.c \
	store-int.c \
	store.c \
	store_emem.c \
	store_fd.c \
	store_mem.c \
	ticket.c \
	time.c \
	transited.c \
	verify_init.c \
	verify_user.c \
	version.c \
	warn.c \
	write_message.c

SRCS+=	heim_err.c \
	heim_err.h \
	k524_err.c \
	k524_err.h \
	krb5_err.c \
	krb5_err.h \
	krb_err.c \
	krb_err.h

CPPFLAGS=	-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/ipc \
		-I${KRB5DIR}/base -I.

CPPFLAGS+=	-DBUILD_KRB5_LIB

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/krb5 ${KRB5DIR}/lib/asn1 ${KRB5DIR}/include ${.CURDIR}/../../include ${KRB5DIR}/doc/doxyout/krb5/man/man3/
