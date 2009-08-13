# $OpenBSD: Makefile.krb5.inc,v 1.16 2009/08/13 19:54:58 jmc Exp $

DIR_KRB5INC=	${.CURDIR}/../../kerberosV/include
DIR_KRB5LIB=	${KRB5SRC}/lib/krb5
HDR_KRB5INC=	krb5-types.h
HDR_KRB5LIB=	krb5.h krb5-protos.h krb5-private.h
HDR_GEN+=	krb5_err.h heim_err.h k524_err.h

MAN+=	krb524_convert_creds_kdc.3
MLINKS+=krb524_convert_creds_kdc.3 krb524_convert_creds_kdc_ccache.3

MAN+=	krb5_acl_match_file.3
MLINKS+=krb5_acl_match_file.3 krb5_acl_match_string.3

MAN+=	krb5.conf.5

MAN+=	kerberos.8

MAN+=	krb5.3

MAN+=	krb5_init_context.3
MLINKS+=krb5_init_context.3 krb5_free_context.3
MLINKS+=krb5_init_context.3 krb5_init_ets.3
MLINKS+=krb5_init_context.3 krb5_add_et_list.3
MLINKS+=krb5_init_context.3 krb5_add_extra_addresses.3
MLINKS+=krb5_init_context.3 krb5_add_ignore_addresses.3
MLINKS+=krb5_init_context.3 krb5_get_extra_addresses.3
MLINKS+=krb5_init_context.3 krb5_get_ignore_addresses.3
MLINKS+=krb5_init_context.3 krb5_set_extra_addresses.3
MLINKS+=krb5_init_context.3 krb5_set_ignore_addresses.3
MLINKS+=krb5_init_context.3 krb5_set_fcache_version.3
MLINKS+=krb5_init_context.3 krb5_get_fcache_version.3
MLINKS+=krb5_init_context.3 krb5_set_config_files.3
MLINKS+=krb5_init_context.3 krb5_prepend_config_files.3
MLINKS+=krb5_init_context.3 krb5_prepend_config_files_default.3
MLINKS+=krb5_init_context.3 krb5_get_default_config_files.3
MLINKS+=krb5_init_context.3 krb5_free_config_files.3
MLINKS+=krb5_init_context.3 krb5_set_use_admin_kdc.3
MLINKS+=krb5_init_context.3 krb5_get_use_admin_kdc.3

MAN+=	krb5_warn.3
MLINKS+=krb5_warn.3 krb5_abort.3
MLINKS+=krb5_warn.3 krb5_abortx.3
MLINKS+=krb5_warn.3 krb5_clear_error_string.3
MLINKS+=krb5_warn.3 krb5_err.3
MLINKS+=krb5_warn.3 krb5_errx.3
MLINKS+=krb5_warn.3 krb5_free_error_string.3
MLINKS+=krb5_warn.3 krb5_get_err_text.3
MLINKS+=krb5_warn.3 krb5_get_error_string.3
MLINKS+=krb5_warn.3 krb5_have_error_string.3
MLINKS+=krb5_warn.3 krb5_set_error_string.3
MLINKS+=krb5_warn.3 krb5_set_warn_dest.3
MLINKS+=krb5_warn.3 krb5_vabort.3
MLINKS+=krb5_warn.3 krb5_vabortx.3
MLINKS+=krb5_warn.3 krb5_verr.3
MLINKS+=krb5_warn.3 krb5_verrx.3
MLINKS+=krb5_warn.3 krb5_vset_error_string.3
MLINKS+=krb5_warn.3 krb5_vwarn.3
MLINKS+=krb5_warn.3 krb5_vwarnx.3
MLINKS+=krb5_warn.3 krb5_warnx.3

MAN+=	krb5_425_conv_principal.3
MLINKS+=krb5_425_conv_principal.3 krb5_425_conv_principal_ext.3
MLINKS+=krb5_425_conv_principal.3 krb5_524_conv_principal.3

MAN+=	krb5_appdefault.3
MLINKS+=krb5_appdefault.3 krb5_appdefault_boolean.3
MLINKS+=krb5_appdefault.3 krb5_appdefault_string.3
MLINKS+=krb5_appdefault.3 krb5_appdefault_time.3

MAN+=	krb5_config.3
MLINKS+=krb5_config.3 krb5_config_file_free.3
MLINKS+=krb5_config.3 krb5_config_free_strings.3
MLINKS+=krb5_config.3 krb5_config_get.3
MLINKS+=krb5_config.3 krb5_config_get_bool.3
MLINKS+=krb5_config.3 krb5_config_get_bool_default.3
MLINKS+=krb5_config.3 krb5_config_get_int.3
MLINKS+=krb5_config.3 krb5_config_get_int_default.3
MLINKS+=krb5_config.3 krb5_config_get_list.3
MLINKS+=krb5_config.3 krb5_config_get_next.3
MLINKS+=krb5_config.3 krb5_config_get_string.3
MLINKS+=krb5_config.3 krb5_config_get_string_default.3
MLINKS+=krb5_config.3 krb5_config_get_strings.3
MLINKS+=krb5_config.3 krb5_config_get_time.3
MLINKS+=krb5_config.3 krb5_config_get_time_default.3
MLINKS+=krb5_config.3 krb5_config_parse_file.3
MLINKS+=krb5_config.3 krb5_config_parse_file_multi.3
MLINKS+=krb5_config.3 krb5_config_vget.3
MLINKS+=krb5_config.3 krb5_config_vget_bool.3
MLINKS+=krb5_config.3 krb5_config_vget_bool_default.3
MLINKS+=krb5_config.3 krb5_config_vget_int.3
MLINKS+=krb5_config.3 krb5_config_vget_int_default.3
MLINKS+=krb5_config.3 krb5_config_vget_list.3
MLINKS+=krb5_config.3 krb5_config_vget_next.3
MLINKS+=krb5_config.3 krb5_config_vget_string.3
MLINKS+=krb5_config.3 krb5_config_vget_string_default.3
MLINKS+=krb5_config.3 krb5_config_vget_strings.3
MLINKS+=krb5_config.3 krb5_config_vget_time.3
MLINKS+=krb5_config.3 krb5_config_vget_time_default.3

MAN+=	krb5_openlog.3
MLINKS+=krb5_openlog.3 krb5_initlog.3
MLINKS+=krb5_openlog.3 krb5_closelog.3
MLINKS+=krb5_openlog.3 krb5_addlog_dest.3
MLINKS+=krb5_openlog.3 krb5_addlog_func.3
MLINKS+=krb5_openlog.3 krb5_log.3
MLINKS+=krb5_openlog.3 krb5_vlog.3
MLINKS+=krb5_openlog.3 krb5_log_msg.3
MLINKS+=krb5_openlog.3 krb5_vlog_msg.3

MAN+=	krb5_auth_context.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_addflags.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_free.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_genaddrs.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_generatelocalsubkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getaddrs.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getauthenticator.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getflags.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getlocalsubkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getrcache.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getremotesubkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_getuserkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_init.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_initivector.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_removeflags.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setaddrs.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setaddrs_from_fd.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setflags.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setivector.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setlocalsubkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setrcache.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setremotesubkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_con_setuserkey.3
MLINKS+=krb5_auth_context.3 krb5_auth_getcksumtype.3
MLINKS+=krb5_auth_context.3 krb5_auth_getkeytype.3
MLINKS+=krb5_auth_context.3 krb5_auth_getlocalseqnumber.3
MLINKS+=krb5_auth_context.3 krb5_auth_getremoteseqnumber.3
MLINKS+=krb5_auth_context.3 krb5_auth_setcksumtype.3
MLINKS+=krb5_auth_context.3 krb5_auth_setkeytype.3
MLINKS+=krb5_auth_context.3 krb5_auth_setlocalseqnumber.3
MLINKS+=krb5_auth_context.3 krb5_auth_setremoteseqnumber.3
MLINKS+=krb5_auth_context.3 krb5_free_authenticator.3

MAN+=	krb5_context.3

MAN+=	krb5_create_checksum.3
MLINKS+=krb5_create_checksum.3 krb5_checksum.3
MLINKS+=krb5_create_checksum.3 krb5_checksum_disable.3
MLINKS+=krb5_create_checksum.3 krb5_checksum_is_collision_proof.3
MLINKS+=krb5_create_checksum.3 krb5_checksum_is_keyed.3
MLINKS+=krb5_create_checksum.3 krb5_checksumsize.3
MLINKS+=krb5_create_checksum.3 krb5_cksumtype_valid.3
MLINKS+=krb5_create_checksum.3 krb5_copy_checksum.3
MLINKS+=krb5_create_checksum.3 krb5_crypto_get_checksum_type.3
MLINKS+=krb5_create_checksum.3 krb5_free_checksum.3
MLINKS+=krb5_create_checksum.3 krb5_free_checksum_contents.3
MLINKS+=krb5_create_checksum.3 krb5_hmac.3
MLINKS+=krb5_create_checksum.3 krb5_verify_checksum.3

MAN+=	krb5_crypto_init.3
MLINKS+=krb5_crypto_init.3 krb5_crypto_destroy.3

MAN+=	krb5_encrypt.3
MLINKS+=krb5_encrypt.3 krb5_crypto_getblocksize.3
MLINKS+=krb5_encrypt.3 krb5_crypto_getconfoundersize.3
MLINKS+=krb5_encrypt.3 krb5_crypto_getenctype.3
MLINKS+=krb5_encrypt.3 krb5_crypto_getpadsize.3
MLINKS+=krb5_encrypt.3 krb5_decrypt.3
MLINKS+=krb5_encrypt.3 krb5_decrypt_EncryptedData.3
MLINKS+=krb5_encrypt.3 krb5_decrypt_ivec.3
MLINKS+=krb5_encrypt.3 krb5_decrypt_ticket.3
MLINKS+=krb5_encrypt.3 krb5_encrypt_EncryptedData.3
MLINKS+=krb5_encrypt.3 krb5_encrypt_ivec.3
MLINKS+=krb5_encrypt.3 krb5_enctype_disable.3
MLINKS+=krb5_encrypt.3 krb5_enctype_keysize.3
MLINKS+=krb5_encrypt.3 krb5_enctype_to_string.3
MLINKS+=krb5_encrypt.3 krb5_enctype_valid.3
MLINKS+=krb5_encrypt.3 krb5_get_wrapped_length.3
MLINKS+=krb5_encrypt.3 krb5_string_to_enctype.3

MAN+=	krb5_keytab.3
MLINKS+=krb5_keytab.3 krb5_kt_ops.3
MLINKS+=krb5_keytab.3 krb5_keytab_entry.3
MLINKS+=krb5_keytab.3 krb5_kt_cursor.3
MLINKS+=krb5_keytab.3 krb5_kt_add_entry.3
MLINKS+=krb5_keytab.3 krb5_kt_close.3
MLINKS+=krb5_keytab.3 krb5_kt_compare.3
MLINKS+=krb5_keytab.3 krb5_kt_copy_entry_contents.3
MLINKS+=krb5_keytab.3 krb5_kt_default.3
MLINKS+=krb5_keytab.3 krb5_kt_default_modify_name.3
MLINKS+=krb5_keytab.3 krb5_kt_default_name.3
MLINKS+=krb5_keytab.3 krb5_kt_end_seq_get.3
MLINKS+=krb5_keytab.3 krb5_kt_free_entry.3
MLINKS+=krb5_keytab.3 krb5_kt_get_entry.3
MLINKS+=krb5_keytab.3 krb5_kt_get_name.3
MLINKS+=krb5_keytab.3 krb5_kt_get_type.3
MLINKS+=krb5_keytab.3 krb5_kt_next_entry.3
MLINKS+=krb5_keytab.3 krb5_kt_read_service_key.3
MLINKS+=krb5_keytab.3 krb5_kt_register.3
MLINKS+=krb5_keytab.3 krb5_kt_remove_entry.3
MLINKS+=krb5_keytab.3 krb5_kt_resolve.3
MLINKS+=krb5_keytab.3 krb5_kt_start_seq_get.3

MAN+=	krb5_verify_user.3
MLINKS+=krb5_verify_user.3 krb5_verify_user_lrealm.3
MLINKS+=krb5_verify_user.3 krb5_verify_user_opt.3
MLINKS+=krb5_verify_user.3 krb5_verify_opt_init.3
MLINKS+=krb5_verify_user.3 krb5_verify_opt_set_ccache.3
MLINKS+=krb5_verify_user.3 krb5_verify_opt_set_flags.3
MLINKS+=krb5_verify_user.3 krb5_verify_opt_set_service.3
MLINKS+=krb5_verify_user.3 krb5_verify_opt_set_secure.3
MLINKS+=krb5_verify_user.3 krb5_verify_opt_set_keytab.3

MAN+=	krb5_set_password.3
MLINKS+=krb5_set_password.3 krb5_change_password.3
MLINKS+=krb5_set_password.3 krb5_set_password_using_ccache.3
MLINKS+=krb5_set_password.3 krb5_passwd_result_to_string.3

MAN+=	krb5_ccache.3
MLINKS+=krb5_ccache.3 krb5_cc_cursor.3
MLINKS+=krb5_ccache.3 krb5_cc_ops.3
MLINKS+=krb5_ccache.3 krb5_fcc_ops.3
MLINKS+=krb5_ccache.3 krb5_mcc_ops.3
MLINKS+=krb5_ccache.3 krb5_cc_clear_mcred.3
MLINKS+=krb5_ccache.3 krb5_cc_close.3
MLINKS+=krb5_ccache.3 krb5_cc_copy_cache.3
MLINKS+=krb5_ccache.3 krb5_cc_default.3
MLINKS+=krb5_ccache.3 krb5_cc_default_name.3
MLINKS+=krb5_ccache.3 krb5_cc_destroy.3
MLINKS+=krb5_ccache.3 krb5_cc_end_seq_get.3
MLINKS+=krb5_ccache.3 krb5_cc_gen_new.3
MLINKS+=krb5_ccache.3 krb5_cc_get_name.3
MLINKS+=krb5_ccache.3 krb5_cc_get_ops.3
MLINKS+=krb5_ccache.3 krb5_cc_get_prefix_ops.3
MLINKS+=krb5_ccache.3 krb5_cc_get_principal.3
MLINKS+=krb5_ccache.3 krb5_cc_get_type.3
MLINKS+=krb5_ccache.3 krb5_cc_get_version.3
MLINKS+=krb5_ccache.3 krb5_cc_initialize.3
MLINKS+=krb5_ccache.3 krb5_cc_next_cred.3
MLINKS+=krb5_ccache.3 krb5_cc_next_cred_match.3
MLINKS+=krb5_ccache.3 krb5_cc_new_unique.3
MLINKS+=krb5_ccache.3 krb5_cc_register.3
MLINKS+=krb5_ccache.3 krb5_cc_remove_cred.3
MLINKS+=krb5_ccache.3 krb5_cc_resolve.3
MLINKS+=krb5_ccache.3 krb5_cc_retrieve_cred.3
MLINKS+=krb5_ccache.3 krb5_cc_set_default_name.3
MLINKS+=krb5_ccache.3 krb5_cc_set_flags.3
MLINKS+=krb5_ccache.3 krb5_cc_store_cred.3

MAN+=	krb5_address.3
MLINKS+=krb5_address.3 krb5_addresses.3
MLINKS+=krb5_address.3 krb5_sockaddr2address.3
MLINKS+=krb5_address.3 krb5_sockaddr2port.3
MLINKS+=krb5_address.3 krb5_addr2sockaddr.3
MLINKS+=krb5_address.3 krb5_max_sockaddr_size.3
MLINKS+=krb5_address.3 krb5_sockaddr_uninteresting.3
MLINKS+=krb5_address.3 krb5_h_addr2sockaddr.3
MLINKS+=krb5_address.3 krb5_h_addr2addr.3
MLINKS+=krb5_address.3 krb5_anyaddr.3
MLINKS+=krb5_address.3 krb5_print_address.3
MLINKS+=krb5_address.3 krb5_parse_address.3
MLINKS+=krb5_address.3 krb5_address_order.3
MLINKS+=krb5_address.3 krb5_address_compare.3
MLINKS+=krb5_address.3 krb5_address_search.3
MLINKS+=krb5_address.3 krb5_free_address.3
MLINKS+=krb5_address.3 krb5_free_addresses.3
MLINKS+=krb5_address.3 krb5_copy_address.3
MLINKS+=krb5_address.3 krb5_copy_addresses.3
MLINKS+=krb5_address.3 krb5_append_addresses.3
MLINKS+=krb5_address.3 krb5_make_addrport.3

MAN+=	krb5_aname_to_localname.3

MAN+=	krb5_data.3
MLINKS+=krb5_data.3 krb5_data_zero.3
MLINKS+=krb5_data.3 krb5_data_free.3
MLINKS+=krb5_data.3 krb5_free_data_contents.3
MLINKS+=krb5_data.3 krb5_free_data.3
MLINKS+=krb5_data.3 krb5_data_alloc.3
MLINKS+=krb5_data.3 krb5_data_realloc.3
MLINKS+=krb5_data.3 krb5_data_copy.3
MLINKS+=krb5_data.3 krb5_copy_data.3

MAN+=	krb5_get_all_client_addrs.3
MLINKS+=krb5_get_all_client_addrs.3 krb5_get_all_server_addrs.3

MAN+=	krb5_get_krbhst.3
MLINKS+=krb5_get_krbhst.3 krb5_get_krb_admin_hst.3
MLINKS+=krb5_get_krbhst.3 krb5_get_krb_changepw_hst.3
MLINKS+=krb5_get_krbhst.3 krb5_get_krb524hst.3
MLINKS+=krb5_get_krbhst.3 krb5_free_krbhst.3

MAN+=	krb5_krbhst_init.3
MLINKS+=krb5_krbhst_init.3 krb5_krbhst_init_flags.3
MLINKS+=krb5_krbhst_init.3 krb5_krbhst_next.3
MLINKS+=krb5_krbhst_init.3 krb5_krbhst_next_as_string.3
MLINKS+=krb5_krbhst_init.3 krb5_krbhst_reset.3
MLINKS+=krb5_krbhst_init.3 krb5_krbhst_free.3
MLINKS+=krb5_krbhst_init.3 krb5_krbhst_format_string.3
MLINKS+=krb5_krbhst_init.3 krb5_krbhst_get_addrinfo.3

MAN+=	krb5_kuserok.3

MAN+=	krb5_set_default_realm.3
MLINKS+=krb5_set_default_realm.3 krb5_copy_host_realm.3
MLINKS+=krb5_set_default_realm.3 krb5_free_host_realm.3
MLINKS+=krb5_set_default_realm.3 krb5_get_default_realm.3
MLINKS+=krb5_set_default_realm.3 krb5_get_default_realms.3
MLINKS+=krb5_set_default_realm.3 krb5_get_host_realm.3

MAN+=	krb5_timeofday.3
MLINKS+=krb5_timeofday.3 krb5_set_real_time.3
MLINKS+=krb5_timeofday.3 krb5_us_timeofday.3
MLINKS+=krb5_timeofday.3 krb5_format_time.3
MLINKS+=krb5_timeofday.3 krb5_string_to_deltat.3

MAN+=	krb5_get_init_creds.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_keytab.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_alloc.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_free.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_init.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_address_list.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_anonymous.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_default_flags.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_etype_list.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_forwardable.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_pa_password.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_paq_request.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_preauth_list.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_proxiable.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_renew_life.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_salt.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_opt_set_tkt_life.3
MLINKS+=krb5_get_init_creds.3 krb5_get_init_creds_password.3
MLINKS+=krb5_get_init_creds.3 krb5_prompt.3
MLINKS+=krb5_get_init_creds.3 krb5_prompter_posix.3

MAN+=	krb5_verify_init_creds.3
MLINKS+=krb5_verify_init_creds.3 krb5_verify_init_creds_opt_init.3
MLINKS+=krb5_verify_init_creds.3 krb5_verify_init_creds_opt_set_ap_req_nofail.3

MAN+=	krb5_c_make_checksum.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_block_size.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_decrypt.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_encrypt.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_encrypt_length.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_enctype_compare.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_get_checksum.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_is_coll_proof_cksum.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_is_keyed_cksum.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_make_random_key.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_set_checksum.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_valid_cksumtype.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_valid_enctype.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_verify_checksum.3
MLINKS+=krb5_c_make_checksum.3 krb5_c_checksum_length.3

MAN+=	krb5_check_transited.3
MLINKS+=krb5_check_transited.3 krb5_check_transited_realms.3
MLINKS+=krb5_check_transited.3 krb5_domain_x500_decode.3
MLINKS+=krb5_check_transited.3 krb5_domain_x500_encode.3

MAN+=	krb5_compare_creds.3

MAN+=	krb5_creds.3
MLINKS+=krb5_creds.3 krb5_copy_creds.3
MLINKS+=krb5_creds.3 krb5_copy_creds_contents.3
MLINKS+=krb5_creds.3 krb5_free_creds.3
MLINKS+=krb5_creds.3 krb5_free_cred_contents.3

MAN+=	krb5_eai_to_heim_errno.3
MLINKS+=krb5_eai_to_heim_errno.3 krb5_h_errno_to_heim_errno.3

MAN+=	krb5_expand_hostname.3
MLINKS+=krb5_expand_hostname.3 krb5_expand_hostname_realms.3

MAN+=	krb5_find_padata.3
MLINKS+=krb5_find_padata.3 krb5_padata_add.3

MAN+=	krb5_generate_random_block.3

MAN+=	krb5_get_credentials.3
MLINKS+=krb5_get_credentials.3 krb5_get_credentials_with_flags.3
MLINKS+=krb5_get_credentials.3 krb5_get_cred_from_kdc.3
MLINKS+=krb5_get_credentials.3 krb5_get_cred_from_kdc_opt.3
MLINKS+=krb5_get_credentials.3 krb5_get_kdc_cred.3

MAN+=	krb5_get_forwarded_creds.3
MLINKS+=krb5_get_forwarded_creds.3 krb5_fwd_tgt_creds.3

MAN+=	krb5_get_in_cred.3
MLINKS+=krb5_get_in_cred.3 krb5_get_in_tkt.3
MLINKS+=krb5_get_in_cred.3 krb5_get_in_tkt_with_password.3
MLINKS+=krb5_get_in_cred.3 krb5_get_in_tkt_with_keytab.3
MLINKS+=krb5_get_in_cred.3 krb5_get_in_tkt_with_skey.3
MLINKS+=krb5_get_in_cred.3 krb5_free_kdc_rep.3
MLINKS+=krb5_get_in_cred.3 krb5_password_key_proc.3

MAN+=	krb5_getportbyname.3

MAN+=	krb5_is_thread_safe.3

MAN+=	krb5_keyblock.3
MLINKS+=krb5_keyblock.3 krb5_keyblock_get_enctype.3
MLINKS+=krb5_keyblock.3 krb5_copy_keyblock.3
MLINKS+=krb5_keyblock.3 krb5_copy_keyblock_contents.3
MLINKS+=krb5_keyblock.3 krb5_free_keyblock.3
MLINKS+=krb5_keyblock.3 krb5_free_keyblock_contents.3
MLINKS+=krb5_keyblock.3 krb5_generate_random_keyblock.3
MLINKS+=krb5_keyblock.3 krb5_generate_subkey.3
MLINKS+=krb5_keyblock.3 krb5_generate_subkey_extended.3
MLINKS+=krb5_keyblock.3 krb5_keyblock_init.3
MLINKS+=krb5_keyblock.3 krb5_keyblock_zero.3
MLINKS+=krb5_keyblock.3 krb5_random_to_key.3

MAN+=	krb5_mk_safe.3
MLINKS+=krb5_mk_safe.3 krb5_mk_priv.3

MAN+=	krb5_principal.3
MLINKS+=krb5_principal.3 krb5_get_default_principal.3
MLINKS+=krb5_principal.3 krb5_build_principal.3
MLINKS+=krb5_principal.3 krb5_build_principal_ext.3
MLINKS+=krb5_principal.3 krb5_build_principal_va.3
MLINKS+=krb5_principal.3 krb5_build_principal_va_ext.3
MLINKS+=krb5_principal.3 krb5_copy_principal.3
MLINKS+=krb5_principal.3 krb5_free_principal.3
MLINKS+=krb5_principal.3 krb5_make_principal.3
MLINKS+=krb5_principal.3 krb5_parse_name.3
MLINKS+=krb5_principal.3 krb5_princ_realm.3
MLINKS+=krb5_principal.3 krb5_princ_set_realm.3
MLINKS+=krb5_principal.3 krb5_principal_compare.3
MLINKS+=krb5_principal.3 krb5_principal_compare_any_realm.3
MLINKS+=krb5_principal.3 krb5_principal_get_comp_string.3
MLINKS+=krb5_principal.3 krb5_principal_get_realm.3
MLINKS+=krb5_principal.3 krb5_principal_get_type.3
MLINKS+=krb5_principal.3 krb5_principal_match.3
MLINKS+=krb5_principal.3 krb5_principal_set_type.3
MLINKS+=krb5_principal.3 krb5_realm_compare.3
MLINKS+=krb5_principal.3 krb5_sname_to_principal.3
MLINKS+=krb5_principal.3 krb5_sock_to_principal.3
MLINKS+=krb5_principal.3 krb5_unparse_name.3
MLINKS+=krb5_principal.3 krb5_unparse_name_fixed.3
MLINKS+=krb5_principal.3 krb5_unparse_name_fixed_short.3
MLINKS+=krb5_principal.3 krb5_unparse_name_short.3

MAN+=	krb5_rcache.3
MLINKS+=krb5_rcache.3 krb5_rc_close.3
MLINKS+=krb5_rcache.3 krb5_rc_default.3
MLINKS+=krb5_rcache.3 krb5_rc_default_name.3
MLINKS+=krb5_rcache.3 krb5_rc_default_type.3
MLINKS+=krb5_rcache.3 krb5_rc_destroy.3
MLINKS+=krb5_rcache.3 krb5_rc_expunge.3
MLINKS+=krb5_rcache.3 krb5_rc_get_lifespan.3
MLINKS+=krb5_rcache.3 krb5_rc_get_name.3
MLINKS+=krb5_rcache.3 krb5_rc_get_type.3
MLINKS+=krb5_rcache.3 krb5_rc_initialize.3
MLINKS+=krb5_rcache.3 krb5_rc_recover.3
MLINKS+=krb5_rcache.3 krb5_rc_resolve.3
MLINKS+=krb5_rcache.3 krb5_rc_resolve_full.3
MLINKS+=krb5_rcache.3 krb5_rc_resolve_type.3
MLINKS+=krb5_rcache.3 krb5_rc_store.3
MLINKS+=krb5_rcache.3 krb5_get_server_rcache.3

MAN+=	krb5_rd_error.3
MLINKS+=krb5_rd_error.3 krb5_free_error.3
MLINKS+=krb5_rd_error.3 krb5_free_error_contents.3
MLINKS+=krb5_rd_error.3 krb5_error_from_rd_error.3

MAN+=	krb5_rd_safe.3
MLINKS+=krb5_rd_safe.3 krb5_rd_priv.3

MAN+=	krb5_storage.3
MLINKS+=krb5_storage.3 krb5_storage_emem.3
MLINKS+=krb5_storage.3 krb5_storage_from_data.3
MLINKS+=krb5_storage.3 krb5_storage_from_fd.3
MLINKS+=krb5_storage.3 krb5_storage_from_mem.3
MLINKS+=krb5_storage.3 krb5_storage_set_flags.3
MLINKS+=krb5_storage.3 krb5_storage_clear_flags.3
MLINKS+=krb5_storage.3 krb5_storage_is_flags.3
MLINKS+=krb5_storage.3 krb5_storage_set_byteorder.3
MLINKS+=krb5_storage.3 krb5_storage_get_byteorder.3
MLINKS+=krb5_storage.3 krb5_storage_set_eof_code.3
MLINKS+=krb5_storage.3 krb5_storage_seek.3
MLINKS+=krb5_storage.3 krb5_storage_read.3
MLINKS+=krb5_storage.3 krb5_storage_write.3
MLINKS+=krb5_storage.3 krb5_storage_free.3
MLINKS+=krb5_storage.3 krb5_storage_to_data.3
MLINKS+=krb5_storage.3 krb5_store_int32.3
MLINKS+=krb5_storage.3 krb5_ret_int32.3
MLINKS+=krb5_storage.3 krb5_store_int16.3
MLINKS+=krb5_storage.3 krb5_ret_int16.3
MLINKS+=krb5_storage.3 krb5_store_int8.3
MLINKS+=krb5_storage.3 krb5_ret_int8.3
MLINKS+=krb5_storage.3 krb5_store_data.3
MLINKS+=krb5_storage.3 krb5_ret_data.3
MLINKS+=krb5_storage.3 krb5_store_string.3
MLINKS+=krb5_storage.3 krb5_ret_string.3
MLINKS+=krb5_storage.3 krb5_store_stringz.3
MLINKS+=krb5_storage.3 krb5_ret_stringz.3
MLINKS+=krb5_storage.3 krb5_store_principal.3
MLINKS+=krb5_storage.3 krb5_ret_principal.3
MLINKS+=krb5_storage.3 krb5_store_keyblock.3
MLINKS+=krb5_storage.3 krb5_ret_keyblock.3
MLINKS+=krb5_storage.3 krb5_store_times.3
MLINKS+=krb5_storage.3 krb5_ret_times.3
MLINKS+=krb5_storage.3 krb5_store_address.3
MLINKS+=krb5_storage.3 krb5_ret_address.3
MLINKS+=krb5_storage.3 krb5_store_addrs.3
MLINKS+=krb5_storage.3 krb5_ret_addrs.3
MLINKS+=krb5_storage.3 krb5_store_authdata.3
MLINKS+=krb5_storage.3 krb5_ret_authdata.3
MLINKS+=krb5_storage.3 krb5_store_creds.3
MLINKS+=krb5_storage.3 krb5_ret_creds.3

MAN+=	krb5_string_to_key.3
MLINKS+=krb5_string_to_key.3 krb5_string_to_key_data.3
MLINKS+=krb5_string_to_key.3 krb5_string_to_key_data_salt.3
MLINKS+=krb5_string_to_key.3 krb5_string_to_key_data_salt_opaque.3
MLINKS+=krb5_string_to_key.3 krb5_string_to_key_derived.3
MLINKS+=krb5_string_to_key.3 krb5_string_to_key_salt.3
MLINKS+=krb5_string_to_key.3 krb5_string_to_key_salt_opaque.3
MLINKS+=krb5_string_to_key.3 krb5_get_pw_salt.3
MLINKS+=krb5_string_to_key.3 krb5_free_salt.3

MAN+=	krb5_ticket.3
MLINKS+=krb5_ticket.3 krb5_free_ticket.3
MLINKS+=krb5_ticket.3 krb5_copy_ticket.3
MLINKS+=krb5_ticket.3 krb5_ticket_get_authorization_data_type.3
MLINKS+=krb5_ticket.3 krb5_ticket_get_client.3
MLINKS+=krb5_ticket.3 krb5_ticket_get_server.3

.PATH:	${DIR_KRB5LIB}

gen_files_krb5= \
	heim_err.h \
	k524_err.h \
	krb5_err.h \
	heim_err.c \
	k524_err.c \
	krb5_err.c

SRCS+=	acache.c \
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
	context.c \
	changepw.c \
	codec.c \
	config_file.c \
	config_file_netinfo.c \
	constants.c \
	context.c \
	convert_creds.c \
	copy_host_realm.c \
	crc.c \
	creds.c \
	crypto.c \
	data.c \
	eai_to_heim_errno.c \
	error_string.c \
	expand_hostname.c \
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
	get_in_tkt_pw.c \
	get_in_tkt_with_keytab.c \
	get_in_tkt_with_skey.c \
	get_port.c \
	init_creds.c \
	init_creds_pw.c \
	kcm.c \
	keyblock.c \
	krb5_keytab.c \
	keytab_any.c \
	keytab_file.c \
	keytab_keyfile.c \
	keytab_krb4.c \
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
	krb5_net_read.c \
	krb5_net_write.c \
	n-fold.c \
	padata.c \
	pkinit.c \
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
	send_to_kdc.c \
	sendauth.c \
	set_default_realm.c \
	sock_principal.c \
	store.c \
	store_emem.c \
	store_fd.c \
	store_mem.c \
	ticket.c \
	time.c \
	transited.c \
	v4_glue.c \
	verify_init.c \
	verify_user.c \
	version.c \
	krb5_warn.c \
	write_message.c \
	${gen_files_krb5}

CFLAGS+=-I${DIR_KRB5LIB} -I${KRB5SRC}/lib/ \
	-I../asn1 -I. \
	-DSHA1_CTX=SHA_CTX

regen_krb5:
	cd ${GENDIR}; compile_et ${DIR_KRB5LIB}/heim_err.et
	cd ${GENDIR}; compile_et ${DIR_KRB5LIB}/k524_err.et
	cd ${GENDIR}; compile_et ${DIR_KRB5LIB}/krb5_err.et

regen:	regen_krb5

# There is a warn.c both in lib/roken and lib/krb5.
krb5_warn.c:
	ln -sf ${DIR_KRB5LIB}/warn.c krb5_warn.c
CLEANFILES+=krb5_warn.c

# There is a net_write.c and net_read.c both in lib/roken and lib/krb5.
krb5_net_write.c:      ${DIR_KRB5LIB}/net_write.c
	ln -sf ${DIR_KRB5LIB}/net_write.c krb5_net_write.c
krb5_net_read.c:       ${DIR_KRB5LIB}/net_read.c
	ln -sf ${DIR_KRB5LIB}/net_read.c krb5_net_read.c

krb5_keytab.c:       ${DIR_KRB5LIB}/keytab.c
	ln -sf ${DIR_KRB5LIB}/keytab.c krb5_keytab.c

CLEANFILES+=krb5_net_{read,write}.c krb5_keytab.c
