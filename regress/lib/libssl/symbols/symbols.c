#include <openssl/dtls1.h>
#include <openssl/ssl.h>
#include <openssl/ssl2.h>
#include <openssl/ssl23.h>
#include <openssl/ssl3.h>
#include <openssl/tls1.h>

#include <openssl/srtp.h>

#ifdef BIO_f_ssl
#undef BIO_f_ssl
#endif
#ifdef BIO_new_buffer_ssl_connect
#undef BIO_new_buffer_ssl_connect
#endif
#ifdef BIO_new_ssl
#undef BIO_new_ssl
#endif
#ifdef BIO_new_ssl_connect
#undef BIO_new_ssl_connect
#endif
#ifdef BIO_ssl_copy_session_id
#undef BIO_ssl_copy_session_id
#endif
#ifdef BIO_ssl_shutdown
#undef BIO_ssl_shutdown
#endif
#ifdef DTLS_client_method
#undef DTLS_client_method
#endif
#ifdef DTLS_method
#undef DTLS_method
#endif
#ifdef DTLS_server_method
#undef DTLS_server_method
#endif
#ifdef DTLSv1_2_client_method
#undef DTLSv1_2_client_method
#endif
#ifdef DTLSv1_2_method
#undef DTLSv1_2_method
#endif
#ifdef DTLSv1_2_server_method
#undef DTLSv1_2_server_method
#endif
#ifdef DTLSv1_client_method
#undef DTLSv1_client_method
#endif
#ifdef DTLSv1_method
#undef DTLSv1_method
#endif
#ifdef DTLSv1_server_method
#undef DTLSv1_server_method
#endif
#ifdef ERR_load_SSL_strings
#undef ERR_load_SSL_strings
#endif
#ifdef OPENSSL_init_ssl
#undef OPENSSL_init_ssl
#endif
#ifdef PEM_read_SSL_SESSION
#undef PEM_read_SSL_SESSION
#endif
#ifdef PEM_read_bio_SSL_SESSION
#undef PEM_read_bio_SSL_SESSION
#endif
#ifdef PEM_write_SSL_SESSION
#undef PEM_write_SSL_SESSION
#endif
#ifdef PEM_write_bio_SSL_SESSION
#undef PEM_write_bio_SSL_SESSION
#endif
#ifdef SSL_CIPHER_description
#undef SSL_CIPHER_description
#endif
#ifdef SSL_CIPHER_find
#undef SSL_CIPHER_find
#endif
#ifdef SSL_CIPHER_get_auth_nid
#undef SSL_CIPHER_get_auth_nid
#endif
#ifdef SSL_CIPHER_get_bits
#undef SSL_CIPHER_get_bits
#endif
#ifdef SSL_CIPHER_get_by_id
#undef SSL_CIPHER_get_by_id
#endif
#ifdef SSL_CIPHER_get_by_value
#undef SSL_CIPHER_get_by_value
#endif
#ifdef SSL_CIPHER_get_cipher_nid
#undef SSL_CIPHER_get_cipher_nid
#endif
#ifdef SSL_CIPHER_get_digest_nid
#undef SSL_CIPHER_get_digest_nid
#endif
#ifdef SSL_CIPHER_get_id
#undef SSL_CIPHER_get_id
#endif
#ifdef SSL_CIPHER_get_kx_nid
#undef SSL_CIPHER_get_kx_nid
#endif
#ifdef SSL_CIPHER_get_name
#undef SSL_CIPHER_get_name
#endif
#ifdef SSL_CIPHER_get_value
#undef SSL_CIPHER_get_value
#endif
#ifdef SSL_CIPHER_get_version
#undef SSL_CIPHER_get_version
#endif
#ifdef SSL_CIPHER_is_aead
#undef SSL_CIPHER_is_aead
#endif
#ifdef SSL_COMP_add_compression_method
#undef SSL_COMP_add_compression_method
#endif
#ifdef SSL_COMP_get_compression_methods
#undef SSL_COMP_get_compression_methods
#endif
#ifdef SSL_COMP_get_name
#undef SSL_COMP_get_name
#endif
#ifdef SSL_CTX_add0_chain_cert
#undef SSL_CTX_add0_chain_cert
#endif
#ifdef SSL_CTX_add1_chain_cert
#undef SSL_CTX_add1_chain_cert
#endif
#ifdef SSL_CTX_add_client_CA
#undef SSL_CTX_add_client_CA
#endif
#ifdef SSL_CTX_add_session
#undef SSL_CTX_add_session
#endif
#ifdef SSL_CTX_callback_ctrl
#undef SSL_CTX_callback_ctrl
#endif
#ifdef SSL_CTX_check_private_key
#undef SSL_CTX_check_private_key
#endif
#ifdef SSL_CTX_clear_chain_certs
#undef SSL_CTX_clear_chain_certs
#endif
#ifdef SSL_CTX_ctrl
#undef SSL_CTX_ctrl
#endif
#ifdef SSL_CTX_flush_sessions
#undef SSL_CTX_flush_sessions
#endif
#ifdef SSL_CTX_free
#undef SSL_CTX_free
#endif
#ifdef SSL_CTX_get0_certificate
#undef SSL_CTX_get0_certificate
#endif
#ifdef SSL_CTX_get0_chain_certs
#undef SSL_CTX_get0_chain_certs
#endif
#ifdef SSL_CTX_get0_param
#undef SSL_CTX_get0_param
#endif
#ifdef SSL_CTX_get0_privatekey
#undef SSL_CTX_get0_privatekey
#endif
#ifdef SSL_CTX_get_cert_store
#undef SSL_CTX_get_cert_store
#endif
#ifdef SSL_CTX_get_ciphers
#undef SSL_CTX_get_ciphers
#endif
#ifdef SSL_CTX_get_client_CA_list
#undef SSL_CTX_get_client_CA_list
#endif
#ifdef SSL_CTX_get_client_cert_cb
#undef SSL_CTX_get_client_cert_cb
#endif
#ifdef SSL_CTX_get_default_passwd_cb
#undef SSL_CTX_get_default_passwd_cb
#endif
#ifdef SSL_CTX_get_default_passwd_cb_userdata
#undef SSL_CTX_get_default_passwd_cb_userdata
#endif
#ifdef SSL_CTX_get_ex_data
#undef SSL_CTX_get_ex_data
#endif
#ifdef SSL_CTX_get_ex_new_index
#undef SSL_CTX_get_ex_new_index
#endif
#ifdef SSL_CTX_get_info_callback
#undef SSL_CTX_get_info_callback
#endif
#ifdef SSL_CTX_get_keylog_callback
#undef SSL_CTX_get_keylog_callback
#endif
#ifdef SSL_CTX_get_max_early_data
#undef SSL_CTX_get_max_early_data
#endif
#ifdef SSL_CTX_get_max_proto_version
#undef SSL_CTX_get_max_proto_version
#endif
#ifdef SSL_CTX_get_min_proto_version
#undef SSL_CTX_get_min_proto_version
#endif
#ifdef SSL_CTX_get_num_tickets
#undef SSL_CTX_get_num_tickets
#endif
#ifdef SSL_CTX_get_quiet_shutdown
#undef SSL_CTX_get_quiet_shutdown
#endif
#ifdef SSL_CTX_get_security_level
#undef SSL_CTX_get_security_level
#endif
#ifdef SSL_CTX_get_ssl_method
#undef SSL_CTX_get_ssl_method
#endif
#ifdef SSL_CTX_get_timeout
#undef SSL_CTX_get_timeout
#endif
#ifdef SSL_CTX_get_verify_callback
#undef SSL_CTX_get_verify_callback
#endif
#ifdef SSL_CTX_get_verify_depth
#undef SSL_CTX_get_verify_depth
#endif
#ifdef SSL_CTX_get_verify_mode
#undef SSL_CTX_get_verify_mode
#endif
#ifdef SSL_CTX_load_verify_locations
#undef SSL_CTX_load_verify_locations
#endif
#ifdef SSL_CTX_load_verify_mem
#undef SSL_CTX_load_verify_mem
#endif
#ifdef SSL_CTX_new
#undef SSL_CTX_new
#endif
#ifdef SSL_CTX_remove_session
#undef SSL_CTX_remove_session
#endif
#ifdef SSL_CTX_sess_get_get_cb
#undef SSL_CTX_sess_get_get_cb
#endif
#ifdef SSL_CTX_sess_get_new_cb
#undef SSL_CTX_sess_get_new_cb
#endif
#ifdef SSL_CTX_sess_get_remove_cb
#undef SSL_CTX_sess_get_remove_cb
#endif
#ifdef SSL_CTX_sess_set_get_cb
#undef SSL_CTX_sess_set_get_cb
#endif
#ifdef SSL_CTX_sess_set_new_cb
#undef SSL_CTX_sess_set_new_cb
#endif
#ifdef SSL_CTX_sess_set_remove_cb
#undef SSL_CTX_sess_set_remove_cb
#endif
#ifdef SSL_CTX_sessions
#undef SSL_CTX_sessions
#endif
#ifdef SSL_CTX_set0_chain
#undef SSL_CTX_set0_chain
#endif
#ifdef SSL_CTX_set1_chain
#undef SSL_CTX_set1_chain
#endif
#ifdef SSL_CTX_set1_groups
#undef SSL_CTX_set1_groups
#endif
#ifdef SSL_CTX_set1_groups_list
#undef SSL_CTX_set1_groups_list
#endif
#ifdef SSL_CTX_set1_param
#undef SSL_CTX_set1_param
#endif
#ifdef SSL_CTX_set_alpn_protos
#undef SSL_CTX_set_alpn_protos
#endif
#ifdef SSL_CTX_set_alpn_select_cb
#undef SSL_CTX_set_alpn_select_cb
#endif
#ifdef SSL_CTX_set_cert_store
#undef SSL_CTX_set_cert_store
#endif
#ifdef SSL_CTX_set_cert_verify_callback
#undef SSL_CTX_set_cert_verify_callback
#endif
#ifdef SSL_CTX_set_cipher_list
#undef SSL_CTX_set_cipher_list
#endif
#ifdef SSL_CTX_set_ciphersuites
#undef SSL_CTX_set_ciphersuites
#endif
#ifdef SSL_CTX_set_client_CA_list
#undef SSL_CTX_set_client_CA_list
#endif
#ifdef SSL_CTX_set_client_cert_cb
#undef SSL_CTX_set_client_cert_cb
#endif
#ifdef SSL_CTX_set_client_cert_engine
#undef SSL_CTX_set_client_cert_engine
#endif
#ifdef SSL_CTX_set_cookie_generate_cb
#undef SSL_CTX_set_cookie_generate_cb
#endif
#ifdef SSL_CTX_set_cookie_verify_cb
#undef SSL_CTX_set_cookie_verify_cb
#endif
#ifdef SSL_CTX_set_default_passwd_cb
#undef SSL_CTX_set_default_passwd_cb
#endif
#ifdef SSL_CTX_set_default_passwd_cb_userdata
#undef SSL_CTX_set_default_passwd_cb_userdata
#endif
#ifdef SSL_CTX_set_default_verify_paths
#undef SSL_CTX_set_default_verify_paths
#endif
#ifdef SSL_CTX_set_ex_data
#undef SSL_CTX_set_ex_data
#endif
#ifdef SSL_CTX_set_generate_session_id
#undef SSL_CTX_set_generate_session_id
#endif
#ifdef SSL_CTX_set_info_callback
#undef SSL_CTX_set_info_callback
#endif
#ifdef SSL_CTX_set_keylog_callback
#undef SSL_CTX_set_keylog_callback
#endif
#ifdef SSL_CTX_set_max_early_data
#undef SSL_CTX_set_max_early_data
#endif
#ifdef SSL_CTX_set_max_proto_version
#undef SSL_CTX_set_max_proto_version
#endif
#ifdef SSL_CTX_set_min_proto_version
#undef SSL_CTX_set_min_proto_version
#endif
#ifdef SSL_CTX_set_msg_callback
#undef SSL_CTX_set_msg_callback
#endif
#ifdef SSL_CTX_set_next_proto_select_cb
#undef SSL_CTX_set_next_proto_select_cb
#endif
#ifdef SSL_CTX_set_next_protos_advertised_cb
#undef SSL_CTX_set_next_protos_advertised_cb
#endif
#ifdef SSL_CTX_set_num_tickets
#undef SSL_CTX_set_num_tickets
#endif
#ifdef SSL_CTX_set_post_handshake_auth
#undef SSL_CTX_set_post_handshake_auth
#endif
#ifdef SSL_CTX_set_purpose
#undef SSL_CTX_set_purpose
#endif
#ifdef SSL_CTX_set_quic_method
#undef SSL_CTX_set_quic_method
#endif
#ifdef SSL_CTX_set_quiet_shutdown
#undef SSL_CTX_set_quiet_shutdown
#endif
#ifdef SSL_CTX_set_security_level
#undef SSL_CTX_set_security_level
#endif
#ifdef SSL_CTX_set_session_id_context
#undef SSL_CTX_set_session_id_context
#endif
#ifdef SSL_CTX_set_ssl_version
#undef SSL_CTX_set_ssl_version
#endif
#ifdef SSL_CTX_set_timeout
#undef SSL_CTX_set_timeout
#endif
#ifdef SSL_CTX_set_tlsext_use_srtp
#undef SSL_CTX_set_tlsext_use_srtp
#endif
#ifdef SSL_CTX_set_tmp_dh_callback
#undef SSL_CTX_set_tmp_dh_callback
#endif
#ifdef SSL_CTX_set_tmp_ecdh_callback
#undef SSL_CTX_set_tmp_ecdh_callback
#endif
#ifdef SSL_CTX_set_tmp_rsa_callback
#undef SSL_CTX_set_tmp_rsa_callback
#endif
#ifdef SSL_CTX_set_trust
#undef SSL_CTX_set_trust
#endif
#ifdef SSL_CTX_set_verify
#undef SSL_CTX_set_verify
#endif
#ifdef SSL_CTX_set_verify_depth
#undef SSL_CTX_set_verify_depth
#endif
#ifdef SSL_CTX_up_ref
#undef SSL_CTX_up_ref
#endif
#ifdef SSL_CTX_use_PrivateKey
#undef SSL_CTX_use_PrivateKey
#endif
#ifdef SSL_CTX_use_PrivateKey_ASN1
#undef SSL_CTX_use_PrivateKey_ASN1
#endif
#ifdef SSL_CTX_use_PrivateKey_file
#undef SSL_CTX_use_PrivateKey_file
#endif
#ifdef SSL_CTX_use_RSAPrivateKey
#undef SSL_CTX_use_RSAPrivateKey
#endif
#ifdef SSL_CTX_use_RSAPrivateKey_ASN1
#undef SSL_CTX_use_RSAPrivateKey_ASN1
#endif
#ifdef SSL_CTX_use_RSAPrivateKey_file
#undef SSL_CTX_use_RSAPrivateKey_file
#endif
#ifdef SSL_CTX_use_certificate
#undef SSL_CTX_use_certificate
#endif
#ifdef SSL_CTX_use_certificate_ASN1
#undef SSL_CTX_use_certificate_ASN1
#endif
#ifdef SSL_CTX_use_certificate_chain_file
#undef SSL_CTX_use_certificate_chain_file
#endif
#ifdef SSL_CTX_use_certificate_chain_mem
#undef SSL_CTX_use_certificate_chain_mem
#endif
#ifdef SSL_CTX_use_certificate_file
#undef SSL_CTX_use_certificate_file
#endif
#ifdef SSL_SESSION_free
#undef SSL_SESSION_free
#endif
#ifdef SSL_SESSION_get0_cipher
#undef SSL_SESSION_get0_cipher
#endif
#ifdef SSL_SESSION_get0_id_context
#undef SSL_SESSION_get0_id_context
#endif
#ifdef SSL_SESSION_get0_peer
#undef SSL_SESSION_get0_peer
#endif
#ifdef SSL_SESSION_get_compress_id
#undef SSL_SESSION_get_compress_id
#endif
#ifdef SSL_SESSION_get_ex_data
#undef SSL_SESSION_get_ex_data
#endif
#ifdef SSL_SESSION_get_ex_new_index
#undef SSL_SESSION_get_ex_new_index
#endif
#ifdef SSL_SESSION_get_id
#undef SSL_SESSION_get_id
#endif
#ifdef SSL_SESSION_get_master_key
#undef SSL_SESSION_get_master_key
#endif
#ifdef SSL_SESSION_get_max_early_data
#undef SSL_SESSION_get_max_early_data
#endif
#ifdef SSL_SESSION_get_protocol_version
#undef SSL_SESSION_get_protocol_version
#endif
#ifdef SSL_SESSION_get_ticket_lifetime_hint
#undef SSL_SESSION_get_ticket_lifetime_hint
#endif
#ifdef SSL_SESSION_get_time
#undef SSL_SESSION_get_time
#endif
#ifdef SSL_SESSION_get_timeout
#undef SSL_SESSION_get_timeout
#endif
#ifdef SSL_SESSION_has_ticket
#undef SSL_SESSION_has_ticket
#endif
#ifdef SSL_SESSION_is_resumable
#undef SSL_SESSION_is_resumable
#endif
#ifdef SSL_SESSION_new
#undef SSL_SESSION_new
#endif
#ifdef SSL_SESSION_print
#undef SSL_SESSION_print
#endif
#ifdef SSL_SESSION_print_fp
#undef SSL_SESSION_print_fp
#endif
#ifdef SSL_SESSION_set1_id
#undef SSL_SESSION_set1_id
#endif
#ifdef SSL_SESSION_set1_id_context
#undef SSL_SESSION_set1_id_context
#endif
#ifdef SSL_SESSION_set_ex_data
#undef SSL_SESSION_set_ex_data
#endif
#ifdef SSL_SESSION_set_max_early_data
#undef SSL_SESSION_set_max_early_data
#endif
#ifdef SSL_SESSION_set_time
#undef SSL_SESSION_set_time
#endif
#ifdef SSL_SESSION_set_timeout
#undef SSL_SESSION_set_timeout
#endif
#ifdef SSL_SESSION_up_ref
#undef SSL_SESSION_up_ref
#endif
#ifdef SSL_accept
#undef SSL_accept
#endif
#ifdef SSL_add0_chain_cert
#undef SSL_add0_chain_cert
#endif
#ifdef SSL_add1_chain_cert
#undef SSL_add1_chain_cert
#endif
#ifdef SSL_add_client_CA
#undef SSL_add_client_CA
#endif
#ifdef SSL_add_dir_cert_subjects_to_stack
#undef SSL_add_dir_cert_subjects_to_stack
#endif
#ifdef SSL_add_file_cert_subjects_to_stack
#undef SSL_add_file_cert_subjects_to_stack
#endif
#ifdef SSL_alert_desc_string
#undef SSL_alert_desc_string
#endif
#ifdef SSL_alert_desc_string_long
#undef SSL_alert_desc_string_long
#endif
#ifdef SSL_alert_type_string
#undef SSL_alert_type_string
#endif
#ifdef SSL_alert_type_string_long
#undef SSL_alert_type_string_long
#endif
#ifdef SSL_cache_hit
#undef SSL_cache_hit
#endif
#ifdef SSL_callback_ctrl
#undef SSL_callback_ctrl
#endif
#ifdef SSL_check_private_key
#undef SSL_check_private_key
#endif
#ifdef SSL_clear
#undef SSL_clear
#endif
#ifdef SSL_clear_chain_certs
#undef SSL_clear_chain_certs
#endif
#ifdef SSL_connect
#undef SSL_connect
#endif
#ifdef SSL_copy_session_id
#undef SSL_copy_session_id
#endif
#ifdef SSL_ctrl
#undef SSL_ctrl
#endif
#ifdef SSL_do_handshake
#undef SSL_do_handshake
#endif
#ifdef SSL_dup
#undef SSL_dup
#endif
#ifdef SSL_dup_CA_list
#undef SSL_dup_CA_list
#endif
#ifdef SSL_export_keying_material
#undef SSL_export_keying_material
#endif
#ifdef SSL_free
#undef SSL_free
#endif
#ifdef SSL_get0_alpn_selected
#undef SSL_get0_alpn_selected
#endif
#ifdef SSL_get0_chain_certs
#undef SSL_get0_chain_certs
#endif
#ifdef SSL_get0_next_proto_negotiated
#undef SSL_get0_next_proto_negotiated
#endif
#ifdef SSL_get0_param
#undef SSL_get0_param
#endif
#ifdef SSL_get0_peername
#undef SSL_get0_peername
#endif
#ifdef SSL_get0_verified_chain
#undef SSL_get0_verified_chain
#endif
#ifdef SSL_get1_session
#undef SSL_get1_session
#endif
#ifdef SSL_get1_supported_ciphers
#undef SSL_get1_supported_ciphers
#endif
#ifdef SSL_get_SSL_CTX
#undef SSL_get_SSL_CTX
#endif
#ifdef SSL_get_certificate
#undef SSL_get_certificate
#endif
#ifdef SSL_get_cipher_list
#undef SSL_get_cipher_list
#endif
#ifdef SSL_get_ciphers
#undef SSL_get_ciphers
#endif
#ifdef SSL_get_client_CA_list
#undef SSL_get_client_CA_list
#endif
#ifdef SSL_get_client_ciphers
#undef SSL_get_client_ciphers
#endif
#ifdef SSL_get_client_random
#undef SSL_get_client_random
#endif
#ifdef SSL_get_current_cipher
#undef SSL_get_current_cipher
#endif
#ifdef SSL_get_current_compression
#undef SSL_get_current_compression
#endif
#ifdef SSL_get_current_expansion
#undef SSL_get_current_expansion
#endif
#ifdef SSL_get_default_timeout
#undef SSL_get_default_timeout
#endif
#ifdef SSL_get_early_data_status
#undef SSL_get_early_data_status
#endif
#ifdef SSL_get_error
#undef SSL_get_error
#endif
#ifdef SSL_get_ex_data
#undef SSL_get_ex_data
#endif
#ifdef SSL_get_ex_data_X509_STORE_CTX_idx
#undef SSL_get_ex_data_X509_STORE_CTX_idx
#endif
#ifdef SSL_get_ex_new_index
#undef SSL_get_ex_new_index
#endif
#ifdef SSL_get_fd
#undef SSL_get_fd
#endif
#ifdef SSL_get_finished
#undef SSL_get_finished
#endif
#ifdef SSL_get_info_callback
#undef SSL_get_info_callback
#endif
#ifdef SSL_get_max_early_data
#undef SSL_get_max_early_data
#endif
#ifdef SSL_get_max_proto_version
#undef SSL_get_max_proto_version
#endif
#ifdef SSL_get_min_proto_version
#undef SSL_get_min_proto_version
#endif
#ifdef SSL_get_num_tickets
#undef SSL_get_num_tickets
#endif
#ifdef SSL_get_peer_cert_chain
#undef SSL_get_peer_cert_chain
#endif
#ifdef SSL_get_peer_certificate
#undef SSL_get_peer_certificate
#endif
#ifdef SSL_get_peer_finished
#undef SSL_get_peer_finished
#endif
#ifdef SSL_get_peer_quic_transport_params
#undef SSL_get_peer_quic_transport_params
#endif
#ifdef SSL_get_privatekey
#undef SSL_get_privatekey
#endif
#ifdef SSL_get_quiet_shutdown
#undef SSL_get_quiet_shutdown
#endif
#ifdef SSL_get_rbio
#undef SSL_get_rbio
#endif
#ifdef SSL_get_read_ahead
#undef SSL_get_read_ahead
#endif
#ifdef SSL_get_rfd
#undef SSL_get_rfd
#endif
#ifdef SSL_get_security_level
#undef SSL_get_security_level
#endif
#ifdef SSL_get_selected_srtp_profile
#undef SSL_get_selected_srtp_profile
#endif
#ifdef SSL_get_server_random
#undef SSL_get_server_random
#endif
#ifdef SSL_get_servername
#undef SSL_get_servername
#endif
#ifdef SSL_get_servername_type
#undef SSL_get_servername_type
#endif
#ifdef SSL_get_session
#undef SSL_get_session
#endif
#ifdef SSL_get_shared_ciphers
#undef SSL_get_shared_ciphers
#endif
#ifdef SSL_get_shutdown
#undef SSL_get_shutdown
#endif
#ifdef SSL_get_srtp_profiles
#undef SSL_get_srtp_profiles
#endif
#ifdef SSL_get_ssl_method
#undef SSL_get_ssl_method
#endif
#ifdef SSL_get_verify_callback
#undef SSL_get_verify_callback
#endif
#ifdef SSL_get_verify_depth
#undef SSL_get_verify_depth
#endif
#ifdef SSL_get_verify_mode
#undef SSL_get_verify_mode
#endif
#ifdef SSL_get_verify_result
#undef SSL_get_verify_result
#endif
#ifdef SSL_get_version
#undef SSL_get_version
#endif
#ifdef SSL_get_wbio
#undef SSL_get_wbio
#endif
#ifdef SSL_get_wfd
#undef SSL_get_wfd
#endif
#ifdef SSL_has_matching_session_id
#undef SSL_has_matching_session_id
#endif
#ifdef SSL_is_dtls
#undef SSL_is_dtls
#endif
#ifdef SSL_is_quic
#undef SSL_is_quic
#endif
#ifdef SSL_is_server
#undef SSL_is_server
#endif
#ifdef SSL_library_init
#undef SSL_library_init
#endif
#ifdef SSL_load_client_CA_file
#undef SSL_load_client_CA_file
#endif
#ifdef SSL_load_error_strings
#undef SSL_load_error_strings
#endif
#ifdef SSL_new
#undef SSL_new
#endif
#ifdef SSL_peek
#undef SSL_peek
#endif
#ifdef SSL_peek_ex
#undef SSL_peek_ex
#endif
#ifdef SSL_pending
#undef SSL_pending
#endif
#ifdef SSL_process_quic_post_handshake
#undef SSL_process_quic_post_handshake
#endif
#ifdef SSL_provide_quic_data
#undef SSL_provide_quic_data
#endif
#ifdef SSL_quic_max_handshake_flight_len
#undef SSL_quic_max_handshake_flight_len
#endif
#ifdef SSL_quic_read_level
#undef SSL_quic_read_level
#endif
#ifdef SSL_quic_write_level
#undef SSL_quic_write_level
#endif
#ifdef SSL_read
#undef SSL_read
#endif
#ifdef SSL_read_early_data
#undef SSL_read_early_data
#endif
#ifdef SSL_read_ex
#undef SSL_read_ex
#endif
#ifdef SSL_renegotiate
#undef SSL_renegotiate
#endif
#ifdef SSL_renegotiate_abbreviated
#undef SSL_renegotiate_abbreviated
#endif
#ifdef SSL_renegotiate_pending
#undef SSL_renegotiate_pending
#endif
#ifdef SSL_rstate_string
#undef SSL_rstate_string
#endif
#ifdef SSL_rstate_string_long
#undef SSL_rstate_string_long
#endif
#ifdef SSL_select_next_proto
#undef SSL_select_next_proto
#endif
#ifdef SSL_set0_chain
#undef SSL_set0_chain
#endif
#ifdef SSL_set0_rbio
#undef SSL_set0_rbio
#endif
#ifdef SSL_set1_chain
#undef SSL_set1_chain
#endif
#ifdef SSL_set1_groups
#undef SSL_set1_groups
#endif
#ifdef SSL_set1_groups_list
#undef SSL_set1_groups_list
#endif
#ifdef SSL_set1_host
#undef SSL_set1_host
#endif
#ifdef SSL_set1_param
#undef SSL_set1_param
#endif
#ifdef SSL_set_SSL_CTX
#undef SSL_set_SSL_CTX
#endif
#ifdef SSL_set_accept_state
#undef SSL_set_accept_state
#endif
#ifdef SSL_set_alpn_protos
#undef SSL_set_alpn_protos
#endif
#ifdef SSL_set_bio
#undef SSL_set_bio
#endif
#ifdef SSL_set_cipher_list
#undef SSL_set_cipher_list
#endif
#ifdef SSL_set_ciphersuites
#undef SSL_set_ciphersuites
#endif
#ifdef SSL_set_client_CA_list
#undef SSL_set_client_CA_list
#endif
#ifdef SSL_set_connect_state
#undef SSL_set_connect_state
#endif
#ifdef SSL_set_debug
#undef SSL_set_debug
#endif
#ifdef SSL_set_ex_data
#undef SSL_set_ex_data
#endif
#ifdef SSL_set_fd
#undef SSL_set_fd
#endif
#ifdef SSL_set_generate_session_id
#undef SSL_set_generate_session_id
#endif
#ifdef SSL_set_hostflags
#undef SSL_set_hostflags
#endif
#ifdef SSL_set_info_callback
#undef SSL_set_info_callback
#endif
#ifdef SSL_set_max_early_data
#undef SSL_set_max_early_data
#endif
#ifdef SSL_set_max_proto_version
#undef SSL_set_max_proto_version
#endif
#ifdef SSL_set_min_proto_version
#undef SSL_set_min_proto_version
#endif
#ifdef SSL_set_msg_callback
#undef SSL_set_msg_callback
#endif
#ifdef SSL_set_num_tickets
#undef SSL_set_num_tickets
#endif
#ifdef SSL_set_post_handshake_auth
#undef SSL_set_post_handshake_auth
#endif
#ifdef SSL_set_psk_use_session_callback
#undef SSL_set_psk_use_session_callback
#endif
#ifdef SSL_set_purpose
#undef SSL_set_purpose
#endif
#ifdef SSL_set_quic_method
#undef SSL_set_quic_method
#endif
#ifdef SSL_set_quic_transport_params
#undef SSL_set_quic_transport_params
#endif
#ifdef SSL_set_quic_use_legacy_codepoint
#undef SSL_set_quic_use_legacy_codepoint
#endif
#ifdef SSL_set_quiet_shutdown
#undef SSL_set_quiet_shutdown
#endif
#ifdef SSL_set_read_ahead
#undef SSL_set_read_ahead
#endif
#ifdef SSL_set_rfd
#undef SSL_set_rfd
#endif
#ifdef SSL_set_security_level
#undef SSL_set_security_level
#endif
#ifdef SSL_set_session
#undef SSL_set_session
#endif
#ifdef SSL_set_session_id_context
#undef SSL_set_session_id_context
#endif
#ifdef SSL_set_session_secret_cb
#undef SSL_set_session_secret_cb
#endif
#ifdef SSL_set_session_ticket_ext
#undef SSL_set_session_ticket_ext
#endif
#ifdef SSL_set_session_ticket_ext_cb
#undef SSL_set_session_ticket_ext_cb
#endif
#ifdef SSL_set_shutdown
#undef SSL_set_shutdown
#endif
#ifdef SSL_set_ssl_method
#undef SSL_set_ssl_method
#endif
#ifdef SSL_set_state
#undef SSL_set_state
#endif
#ifdef SSL_set_tlsext_use_srtp
#undef SSL_set_tlsext_use_srtp
#endif
#ifdef SSL_set_tmp_dh_callback
#undef SSL_set_tmp_dh_callback
#endif
#ifdef SSL_set_tmp_ecdh_callback
#undef SSL_set_tmp_ecdh_callback
#endif
#ifdef SSL_set_tmp_rsa_callback
#undef SSL_set_tmp_rsa_callback
#endif
#ifdef SSL_set_trust
#undef SSL_set_trust
#endif
#ifdef SSL_set_verify
#undef SSL_set_verify
#endif
#ifdef SSL_set_verify_depth
#undef SSL_set_verify_depth
#endif
#ifdef SSL_set_verify_result
#undef SSL_set_verify_result
#endif
#ifdef SSL_set_wfd
#undef SSL_set_wfd
#endif
#ifdef SSL_shutdown
#undef SSL_shutdown
#endif
#ifdef SSL_state
#undef SSL_state
#endif
#ifdef SSL_state_string
#undef SSL_state_string
#endif
#ifdef SSL_state_string_long
#undef SSL_state_string_long
#endif
#ifdef SSL_up_ref
#undef SSL_up_ref
#endif
#ifdef SSL_use_PrivateKey
#undef SSL_use_PrivateKey
#endif
#ifdef SSL_use_PrivateKey_ASN1
#undef SSL_use_PrivateKey_ASN1
#endif
#ifdef SSL_use_PrivateKey_file
#undef SSL_use_PrivateKey_file
#endif
#ifdef SSL_use_RSAPrivateKey
#undef SSL_use_RSAPrivateKey
#endif
#ifdef SSL_use_RSAPrivateKey_ASN1
#undef SSL_use_RSAPrivateKey_ASN1
#endif
#ifdef SSL_use_RSAPrivateKey_file
#undef SSL_use_RSAPrivateKey_file
#endif
#ifdef SSL_use_certificate
#undef SSL_use_certificate
#endif
#ifdef SSL_use_certificate_ASN1
#undef SSL_use_certificate_ASN1
#endif
#ifdef SSL_use_certificate_chain_file
#undef SSL_use_certificate_chain_file
#endif
#ifdef SSL_use_certificate_file
#undef SSL_use_certificate_file
#endif
#ifdef SSL_verify_client_post_handshake
#undef SSL_verify_client_post_handshake
#endif
#ifdef SSL_version
#undef SSL_version
#endif
extern const char *SSL_version_str;
#ifdef SSL_version_str
#undef SSL_version_str
#endif
#ifdef SSL_want
#undef SSL_want
#endif
#ifdef SSL_write
#undef SSL_write
#endif
#ifdef SSL_write_early_data
#undef SSL_write_early_data
#endif
#ifdef SSL_write_ex
#undef SSL_write_ex
#endif
#ifdef SSLv23_client_method
#undef SSLv23_client_method
#endif
#ifdef SSLv23_method
#undef SSLv23_method
#endif
#ifdef SSLv23_server_method
#undef SSLv23_server_method
#endif
#ifdef TLS_client_method
#undef TLS_client_method
#endif
#ifdef TLS_method
#undef TLS_method
#endif
#ifdef TLS_server_method
#undef TLS_server_method
#endif
#ifdef TLSv1_1_client_method
#undef TLSv1_1_client_method
#endif
#ifdef TLSv1_1_method
#undef TLSv1_1_method
#endif
#ifdef TLSv1_1_server_method
#undef TLSv1_1_server_method
#endif
#ifdef TLSv1_2_client_method
#undef TLSv1_2_client_method
#endif
#ifdef TLSv1_2_method
#undef TLSv1_2_method
#endif
#ifdef TLSv1_2_server_method
#undef TLSv1_2_server_method
#endif
#ifdef TLSv1_client_method
#undef TLSv1_client_method
#endif
#ifdef TLSv1_method
#undef TLSv1_method
#endif
#ifdef TLSv1_server_method
#undef TLSv1_server_method
#endif
#ifdef d2i_SSL_SESSION
#undef d2i_SSL_SESSION
#endif
#ifdef i2d_SSL_SESSION
#undef i2d_SSL_SESSION
#endif

int
main(void)
{
	size_t i;
	struct {
		const char *const name;
		const void *addr;
	} symbols[] = {
		{
			.name = "SSL_CTX_use_certificate_ASN1",
			.addr = &SSL_CTX_use_certificate_ASN1,
		},
		{
			.name = "SSL_CTX_set_tmp_dh_callback",
			.addr = &SSL_CTX_set_tmp_dh_callback,
		},
		{
			.name = "SSL_CTX_get_min_proto_version",
			.addr = &SSL_CTX_get_min_proto_version,
		},
		{
			.name = "SSL_CTX_sessions",
			.addr = &SSL_CTX_sessions,
		},
		{
			.name = "SSL_SESSION_print",
			.addr = &SSL_SESSION_print,
		},
		{
			.name = "SSL_SESSION_get_ex_data",
			.addr = &SSL_SESSION_get_ex_data,
		},
		{
			.name = "BIO_new_ssl_connect",
			.addr = &BIO_new_ssl_connect,
		},
		{
			.name = "SSL_CTX_get_security_level",
			.addr = &SSL_CTX_get_security_level,
		},
		{
			.name = "SSL_CTX_get_ex_data",
			.addr = &SSL_CTX_get_ex_data,
		},
		{
			.name = "SSL_SESSION_get_compress_id",
			.addr = &SSL_SESSION_get_compress_id,
		},
		{
			.name = "SSL_select_next_proto",
			.addr = &SSL_select_next_proto,
		},
		{
			.name = "SSL_dup",
			.addr = &SSL_dup,
		},
		{
			.name = "SSL_CIPHER_get_name",
			.addr = &SSL_CIPHER_get_name,
		},
		{
			.name = "TLSv1_1_server_method",
			.addr = &TLSv1_1_server_method,
		},
		{
			.name = "SSL_quic_write_level",
			.addr = &SSL_quic_write_level,
		},
		{
			.name = "SSL_load_client_CA_file",
			.addr = &SSL_load_client_CA_file,
		},
		{
			.name = "SSL_get_servername_type",
			.addr = &SSL_get_servername_type,
		},
		{
			.name = "SSL_CTX_set_trust",
			.addr = &SSL_CTX_set_trust,
		},
		{
			.name = "SSL_shutdown",
			.addr = &SSL_shutdown,
		},
		{
			.name = "SSL_up_ref",
			.addr = &SSL_up_ref,
		},
		{
			.name = "SSL_set_client_CA_list",
			.addr = &SSL_set_client_CA_list,
		},
		{
			.name = "SSL_get_certificate",
			.addr = &SSL_get_certificate,
		},
		{
			.name = "SSL_add_file_cert_subjects_to_stack",
			.addr = &SSL_add_file_cert_subjects_to_stack,
		},
		{
			.name = "DTLSv1_2_client_method",
			.addr = &DTLSv1_2_client_method,
		},
		{
			.name = "SSL_write",
			.addr = &SSL_write,
		},
		{
			.name = "SSL_use_certificate",
			.addr = &SSL_use_certificate,
		},
		{
			.name = "SSL_get_peer_quic_transport_params",
			.addr = &SSL_get_peer_quic_transport_params,
		},
		{
			.name = "SSL_CTX_use_PrivateKey_file",
			.addr = &SSL_CTX_use_PrivateKey_file,
		},
		{
			.name = "SSL_get_max_early_data",
			.addr = &SSL_get_max_early_data,
		},
		{
			.name = "SSL_CTX_add_session",
			.addr = &SSL_CTX_add_session,
		},
		{
			.name = "TLSv1_2_server_method",
			.addr = &TLSv1_2_server_method,
		},
		{
			.name = "SSL_get_verify_result",
			.addr = &SSL_get_verify_result,
		},
		{
			.name = "SSL_SESSION_print_fp",
			.addr = &SSL_SESSION_print_fp,
		},
		{
			.name = "SSL_CTX_set_quiet_shutdown",
			.addr = &SSL_CTX_set_quiet_shutdown,
		},
		{
			.name = "SSL_CIPHER_description",
			.addr = &SSL_CIPHER_description,
		},
		{
			.name = "SSL_read_early_data",
			.addr = &SSL_read_early_data,
		},
		{
			.name = "SSL_CTX_clear_chain_certs",
			.addr = &SSL_CTX_clear_chain_certs,
		},
		{
			.name = "SSL_version",
			.addr = &SSL_version,
		},
		{
			.name = "SSL_CTX_use_PrivateKey_ASN1",
			.addr = &SSL_CTX_use_PrivateKey_ASN1,
		},
		{
			.name = "DTLS_client_method",
			.addr = &DTLS_client_method,
		},
		{
			.name = "PEM_write_bio_SSL_SESSION",
			.addr = &PEM_write_bio_SSL_SESSION,
		},
		{
			.name = "SSL_state",
			.addr = &SSL_state,
		},
		{
			.name = "SSL_set_generate_session_id",
			.addr = &SSL_set_generate_session_id,
		},
		{
			.name = "SSL_SESSION_set_time",
			.addr = &SSL_SESSION_set_time,
		},
		{
			.name = "SSL_CTX_get0_privatekey",
			.addr = &SSL_CTX_get0_privatekey,
		},
		{
			.name = "SSL_CTX_get_default_passwd_cb_userdata",
			.addr = &SSL_CTX_get_default_passwd_cb_userdata,
		},
		{
			.name = "SSL_CTX_set_cookie_generate_cb",
			.addr = &SSL_CTX_set_cookie_generate_cb,
		},
		{
			.name = "SSL_CTX_sess_set_get_cb",
			.addr = &SSL_CTX_sess_set_get_cb,
		},
		{
			.name = "SSL_is_quic",
			.addr = &SSL_is_quic,
		},
		{
			.name = "SSL_SESSION_set_timeout",
			.addr = &SSL_SESSION_set_timeout,
		},
		{
			.name = "SSL_CTX_get_ssl_method",
			.addr = &SSL_CTX_get_ssl_method,
		},
		{
			.name = "SSL_set_quiet_shutdown",
			.addr = &SSL_set_quiet_shutdown,
		},
		{
			.name = "SSL_CTX_set_timeout",
			.addr = &SSL_CTX_set_timeout,
		},
		{
			.name = "SSL_CIPHER_get_by_id",
			.addr = &SSL_CIPHER_get_by_id,
		},
		{
			.name = "SSL_CTX_add_client_CA",
			.addr = &SSL_CTX_add_client_CA,
		},
		{
			.name = "SSL_state_string",
			.addr = &SSL_state_string,
		},
		{
			.name = "SSL_clear",
			.addr = &SSL_clear,
		},
		{
			.name = "SSL_CTX_get_max_proto_version",
			.addr = &SSL_CTX_get_max_proto_version,
		},
		{
			.name = "SSL_get_peer_finished",
			.addr = &SSL_get_peer_finished,
		},
		{
			.name = "SSL_set_min_proto_version",
			.addr = &SSL_set_min_proto_version,
		},
		{
			.name = "SSL_set_session_secret_cb",
			.addr = &SSL_set_session_secret_cb,
		},
		{
			.name = "SSL_get_peer_certificate",
			.addr = &SSL_get_peer_certificate,
		},
		{
			.name = "SSL_ctrl",
			.addr = &SSL_ctrl,
		},
		{
			.name = "SSL_CTX_set_info_callback",
			.addr = &SSL_CTX_set_info_callback,
		},
		{
			.name = "SSL_CTX_set_keylog_callback",
			.addr = &SSL_CTX_set_keylog_callback,
		},
		{
			.name = "SSL_CTX_add0_chain_cert",
			.addr = &SSL_CTX_add0_chain_cert,
		},
		{
			.name = "SSL_renegotiate_pending",
			.addr = &SSL_renegotiate_pending,
		},
		{
			.name = "SSL_CIPHER_get_bits",
			.addr = &SSL_CIPHER_get_bits,
		},
		{
			.name = "SSL_CIPHER_get_digest_nid",
			.addr = &SSL_CIPHER_get_digest_nid,
		},
		{
			.name = "SSL_set_tmp_ecdh_callback",
			.addr = &SSL_set_tmp_ecdh_callback,
		},
		{
			.name = "DTLSv1_2_server_method",
			.addr = &DTLSv1_2_server_method,
		},
		{
			.name = "SSL_CTX_get_ex_new_index",
			.addr = &SSL_CTX_get_ex_new_index,
		},
		{
			.name = "SSL_SESSION_new",
			.addr = &SSL_SESSION_new,
		},
		{
			.name = "SSL_get_verify_mode",
			.addr = &SSL_get_verify_mode,
		},
		{
			.name = "SSL_CTX_set_purpose",
			.addr = &SSL_CTX_set_purpose,
		},
		{
			.name = "SSL_add_client_CA",
			.addr = &SSL_add_client_CA,
		},
		{
			.name = "SSL_accept",
			.addr = &SSL_accept,
		},
		{
			.name = "SSL_use_PrivateKey_file",
			.addr = &SSL_use_PrivateKey_file,
		},
		{
			.name = "SSL_set_connect_state",
			.addr = &SSL_set_connect_state,
		},
		{
			.name = "SSL_clear_chain_certs",
			.addr = &SSL_clear_chain_certs,
		},
		{
			.name = "DTLS_server_method",
			.addr = &DTLS_server_method,
		},
		{
			.name = "SSL_get_cipher_list",
			.addr = &SSL_get_cipher_list,
		},
		{
			.name = "SSL_CTX_set_cert_store",
			.addr = &SSL_CTX_set_cert_store,
		},
		{
			.name = "SSL_CTX_get_client_CA_list",
			.addr = &SSL_CTX_get_client_CA_list,
		},
		{
			.name = "SSL_CTX_set_cert_verify_callback",
			.addr = &SSL_CTX_set_cert_verify_callback,
		},
		{
			.name = "SSLv23_client_method",
			.addr = &SSLv23_client_method,
		},
		{
			.name = "SSL_set1_groups",
			.addr = &SSL_set1_groups,
		},
		{
			.name = "SSL_get_verify_depth",
			.addr = &SSL_get_verify_depth,
		},
		{
			.name = "SSL_CTX_get_info_callback",
			.addr = &SSL_CTX_get_info_callback,
		},
		{
			.name = "SSL_copy_session_id",
			.addr = &SSL_copy_session_id,
		},
		{
			.name = "SSL_COMP_get_compression_methods",
			.addr = &SSL_COMP_get_compression_methods,
		},
		{
			.name = "SSL_SESSION_get_timeout",
			.addr = &SSL_SESSION_get_timeout,
		},
		{
			.name = "SSL_SESSION_get0_id_context",
			.addr = &SSL_SESSION_get0_id_context,
		},
		{
			.name = "SSL_CTX_use_certificate",
			.addr = &SSL_CTX_use_certificate,
		},
		{
			.name = "SSL_use_PrivateKey_ASN1",
			.addr = &SSL_use_PrivateKey_ASN1,
		},
		{
			.name = "SSL_get1_session",
			.addr = &SSL_get1_session,
		},
		{
			.name = "SSL_CTX_set_ssl_version",
			.addr = &SSL_CTX_set_ssl_version,
		},
		{
			.name = "SSL_CTX_set_security_level",
			.addr = &SSL_CTX_set_security_level,
		},
		{
			.name = "SSL_CTX_get_timeout",
			.addr = &SSL_CTX_get_timeout,
		},
		{
			.name = "SSL_CTX_load_verify_mem",
			.addr = &SSL_CTX_load_verify_mem,
		},
		{
			.name = "SSL_CTX_sess_set_new_cb",
			.addr = &SSL_CTX_sess_set_new_cb,
		},
		{
			.name = "SSL_set_num_tickets",
			.addr = &SSL_set_num_tickets,
		},
		{
			.name = "SSL_set_max_proto_version",
			.addr = &SSL_set_max_proto_version,
		},
		{
			.name = "SSL_set_info_callback",
			.addr = &SSL_set_info_callback,
		},
		{
			.name = "SSL_use_certificate_file",
			.addr = &SSL_use_certificate_file,
		},
		{
			.name = "SSL_CIPHER_get_cipher_nid",
			.addr = &SSL_CIPHER_get_cipher_nid,
		},
		{
			.name = "SSL_CTX_get_verify_callback",
			.addr = &SSL_CTX_get_verify_callback,
		},
		{
			.name = "SSL_set_rfd",
			.addr = &SSL_set_rfd,
		},
		{
			.name = "BIO_ssl_shutdown",
			.addr = &BIO_ssl_shutdown,
		},
		{
			.name = "SSL_verify_client_post_handshake",
			.addr = &SSL_verify_client_post_handshake,
		},
		{
			.name = "SSL_alert_desc_string",
			.addr = &SSL_alert_desc_string,
		},
		{
			.name = "SSL_CTX_set_max_early_data",
			.addr = &SSL_CTX_set_max_early_data,
		},
		{
			.name = "DTLS_method",
			.addr = &DTLS_method,
		},
		{
			.name = "SSL_CTX_set_tlsext_use_srtp",
			.addr = &SSL_CTX_set_tlsext_use_srtp,
		},
		{
			.name = "SSL_CTX_set_min_proto_version",
			.addr = &SSL_CTX_set_min_proto_version,
		},
		{
			.name = "SSL_set_wfd",
			.addr = &SSL_set_wfd,
		},
		{
			.name = "SSL_CTX_get_quiet_shutdown",
			.addr = &SSL_CTX_get_quiet_shutdown,
		},
		{
			.name = "SSL_get1_supported_ciphers",
			.addr = &SSL_get1_supported_ciphers,
		},
		{
			.name = "SSL_CTX_load_verify_locations",
			.addr = &SSL_CTX_load_verify_locations,
		},
		{
			.name = "TLSv1_2_method",
			.addr = &TLSv1_2_method,
		},
		{
			.name = "SSL_get0_chain_certs",
			.addr = &SSL_get0_chain_certs,
		},
		{
			.name = "SSL_SESSION_set1_id",
			.addr = &SSL_SESSION_set1_id,
		},
		{
			.name = "SSL_use_certificate_ASN1",
			.addr = &SSL_use_certificate_ASN1,
		},
		{
			.name = "SSL_set_verify",
			.addr = &SSL_set_verify,
		},
		{
			.name = "SSL_set_max_early_data",
			.addr = &SSL_set_max_early_data,
		},
		{
			.name = "SSL_CTX_use_RSAPrivateKey",
			.addr = &SSL_CTX_use_RSAPrivateKey,
		},
		{
			.name = "SSL_CTX_set_tmp_ecdh_callback",
			.addr = &SSL_CTX_set_tmp_ecdh_callback,
		},
		{
			.name = "SSL_CIPHER_is_aead",
			.addr = &SSL_CIPHER_is_aead,
		},
		{
			.name = "SSL_CTX_get0_certificate",
			.addr = &SSL_CTX_get0_certificate,
		},
		{
			.name = "SSL_has_matching_session_id",
			.addr = &SSL_has_matching_session_id,
		},
		{
			.name = "BIO_new_ssl",
			.addr = &BIO_new_ssl,
		},
		{
			.name = "SSL_CTX_get_default_passwd_cb",
			.addr = &SSL_CTX_get_default_passwd_cb,
		},
		{
			.name = "SSL_quic_max_handshake_flight_len",
			.addr = &SSL_quic_max_handshake_flight_len,
		},
		{
			.name = "SSL_free",
			.addr = &SSL_free,
		},
		{
			.name = "SSL_CTX_sess_get_remove_cb",
			.addr = &SSL_CTX_sess_get_remove_cb,
		},
		{
			.name = "SSL_get_min_proto_version",
			.addr = &SSL_get_min_proto_version,
		},
		{
			.name = "ERR_load_SSL_strings",
			.addr = &ERR_load_SSL_strings,
		},
		{
			.name = "SSLv23_server_method",
			.addr = &SSLv23_server_method,
		},
		{
			.name = "SSL_get_info_callback",
			.addr = &SSL_get_info_callback,
		},
		{
			.name = "SSL_want",
			.addr = &SSL_want,
		},
		{
			.name = "SSL_set_debug",
			.addr = &SSL_set_debug,
		},
		{
			.name = "SSL_get0_param",
			.addr = &SSL_get0_param,
		},
		{
			.name = "SSL_get_rfd",
			.addr = &SSL_get_rfd,
		},
		{
			.name = "SSL_get_current_compression",
			.addr = &SSL_get_current_compression,
		},
		{
			.name = "SSL_use_certificate_chain_file",
			.addr = &SSL_use_certificate_chain_file,
		},
		{
			.name = "SSL_is_server",
			.addr = &SSL_is_server,
		},
		{
			.name = "SSL_CTX_get_verify_mode",
			.addr = &SSL_CTX_get_verify_mode,
		},
		{
			.name = "SSL_set_ex_data",
			.addr = &SSL_set_ex_data,
		},
		{
			.name = "SSL_CTX_use_PrivateKey",
			.addr = &SSL_CTX_use_PrivateKey,
		},
		{
			.name = "SSL_get_wfd",
			.addr = &SSL_get_wfd,
		},
		{
			.name = "SSL_CIPHER_get_kx_nid",
			.addr = &SSL_CIPHER_get_kx_nid,
		},
		{
			.name = "SSL_provide_quic_data",
			.addr = &SSL_provide_quic_data,
		},
		{
			.name = "SSL_CTX_get0_param",
			.addr = &SSL_CTX_get0_param,
		},
		{
			.name = "SSL_CTX_sess_get_get_cb",
			.addr = &SSL_CTX_sess_get_get_cb,
		},
		{
			.name = "SSL_CTX_sess_set_remove_cb",
			.addr = &SSL_CTX_sess_set_remove_cb,
		},
		{
			.name = "TLS_method",
			.addr = &TLS_method,
		},
		{
			.name = "SSL_set_session",
			.addr = &SSL_set_session,
		},
		{
			.name = "SSL_get_finished",
			.addr = &SSL_get_finished,
		},
		{
			.name = "SSL_get_verify_callback",
			.addr = &SSL_get_verify_callback,
		},
		{
			.name = "SSL_SESSION_free",
			.addr = &SSL_SESSION_free,
		},
		{
			.name = "TLS_client_method",
			.addr = &TLS_client_method,
		},
		{
			.name = "SSL_use_RSAPrivateKey",
			.addr = &SSL_use_RSAPrivateKey,
		},
		{
			.name = "SSL_is_dtls",
			.addr = &SSL_is_dtls,
		},
		{
			.name = "SSL_check_private_key",
			.addr = &SSL_check_private_key,
		},
		{
			.name = "SSL_alert_desc_string_long",
			.addr = &SSL_alert_desc_string_long,
		},
		{
			.name = "SSL_SESSION_get0_peer",
			.addr = &SSL_SESSION_get0_peer,
		},
		{
			.name = "SSL_CIPHER_get_by_value",
			.addr = &SSL_CIPHER_get_by_value,
		},
		{
			.name = "SSL_CTX_get_keylog_callback",
			.addr = &SSL_CTX_get_keylog_callback,
		},
		{
			.name = "SSL_set0_rbio",
			.addr = &SSL_set0_rbio,
		},
		{
			.name = "BIO_ssl_copy_session_id",
			.addr = &BIO_ssl_copy_session_id,
		},
		{
			.name = "SSL_CTX_get_ciphers",
			.addr = &SSL_CTX_get_ciphers,
		},
		{
			.name = "SSL_peek",
			.addr = &SSL_peek,
		},
		{
			.name = "SSL_CTX_set_num_tickets",
			.addr = &SSL_CTX_set_num_tickets,
		},
		{
			.name = "SSL_CTX_set_post_handshake_auth",
			.addr = &SSL_CTX_set_post_handshake_auth,
		},
		{
			.name = "SSL_set_read_ahead",
			.addr = &SSL_set_read_ahead,
		},
		{
			.name = "SSL_set1_param",
			.addr = &SSL_set1_param,
		},
		{
			.name = "SSL_get_shutdown",
			.addr = &SSL_get_shutdown,
		},
		{
			.name = "SSL_CTX_callback_ctrl",
			.addr = &SSL_CTX_callback_ctrl,
		},
		{
			.name = "SSL_get_ex_data_X509_STORE_CTX_idx",
			.addr = &SSL_get_ex_data_X509_STORE_CTX_idx,
		},
		{
			.name = "SSL_CTX_set0_chain",
			.addr = &SSL_CTX_set0_chain,
		},
		{
			.name = "SSL_CTX_set_client_CA_list",
			.addr = &SSL_CTX_set_client_CA_list,
		},
		{
			.name = "SSL_set_session_id_context",
			.addr = &SSL_set_session_id_context,
		},
		{
			.name = "SSL_get_read_ahead",
			.addr = &SSL_get_read_ahead,
		},
		{
			.name = "SSL_get_ciphers",
			.addr = &SSL_get_ciphers,
		},
		{
			.name = "SSL_SESSION_get0_cipher",
			.addr = &SSL_SESSION_get0_cipher,
		},
		{
			.name = "SSL_set_psk_use_session_callback",
			.addr = &SSL_set_psk_use_session_callback,
		},
		{
			.name = "SSL_CTX_get_max_early_data",
			.addr = &SSL_CTX_get_max_early_data,
		},
		{
			.name = "SSL_CIPHER_get_version",
			.addr = &SSL_CIPHER_get_version,
		},
		{
			.name = "d2i_SSL_SESSION",
			.addr = &d2i_SSL_SESSION,
		},
		{
			.name = "SSL_version_str",
			.addr = &SSL_version_str,
		},
		{
			.name = "SSL_library_init",
			.addr = &SSL_library_init,
		},
		{
			.name = "SSL_get_default_timeout",
			.addr = &SSL_get_default_timeout,
		},
		{
			.name = "SSL_get0_next_proto_negotiated",
			.addr = &SSL_get0_next_proto_negotiated,
		},
		{
			.name = "SSL_SESSION_get_ticket_lifetime_hint",
			.addr = &SSL_SESSION_get_ticket_lifetime_hint,
		},
		{
			.name = "SSL_set_trust",
			.addr = &SSL_set_trust,
		},
		{
			.name = "SSL_CTX_set1_param",
			.addr = &SSL_CTX_set1_param,
		},
		{
			.name = "SSL_get_ex_data",
			.addr = &SSL_get_ex_data,
		},
		{
			.name = "PEM_read_bio_SSL_SESSION",
			.addr = &PEM_read_bio_SSL_SESSION,
		},
		{
			.name = "SSL_CTX_get_client_cert_cb",
			.addr = &SSL_CTX_get_client_cert_cb,
		},
		{
			.name = "SSL_SESSION_up_ref",
			.addr = &SSL_SESSION_up_ref,
		},
		{
			.name = "SSL_get_current_expansion",
			.addr = &SSL_get_current_expansion,
		},
		{
			.name = "SSL_SESSION_has_ticket",
			.addr = &SSL_SESSION_has_ticket,
		},
		{
			.name = "SSL_set_ciphersuites",
			.addr = &SSL_set_ciphersuites,
		},
		{
			.name = "SSL_add1_chain_cert",
			.addr = &SSL_add1_chain_cert,
		},
		{
			.name = "SSL_CTX_set_next_proto_select_cb",
			.addr = &SSL_CTX_set_next_proto_select_cb,
		},
		{
			.name = "SSL_CTX_get0_chain_certs",
			.addr = &SSL_CTX_get0_chain_certs,
		},
		{
			.name = "TLS_server_method",
			.addr = &TLS_server_method,
		},
		{
			.name = "SSL_state_string_long",
			.addr = &SSL_state_string_long,
		},
		{
			.name = "SSL_get_security_level",
			.addr = &SSL_get_security_level,
		},
		{
			.name = "SSL_CTX_sess_get_new_cb",
			.addr = &SSL_CTX_sess_get_new_cb,
		},
		{
			.name = "SSL_get_session",
			.addr = &SSL_get_session,
		},
		{
			.name = "SSL_callback_ctrl",
			.addr = &SSL_callback_ctrl,
		},
		{
			.name = "SSL_CTX_set_cipher_list",
			.addr = &SSL_CTX_set_cipher_list,
		},
		{
			.name = "SSL_cache_hit",
			.addr = &SSL_cache_hit,
		},
		{
			.name = "SSL_CTX_flush_sessions",
			.addr = &SSL_CTX_flush_sessions,
		},
		{
			.name = "SSL_peek_ex",
			.addr = &SSL_peek_ex,
		},
		{
			.name = "SSL_CTX_set_tmp_rsa_callback",
			.addr = &SSL_CTX_set_tmp_rsa_callback,
		},
		{
			.name = "SSL_CTX_set_session_id_context",
			.addr = &SSL_CTX_set_session_id_context,
		},
		{
			.name = "PEM_write_SSL_SESSION",
			.addr = &PEM_write_SSL_SESSION,
		},
		{
			.name = "SSL_set_verify_depth",
			.addr = &SSL_set_verify_depth,
		},
		{
			.name = "SSL_CTX_new",
			.addr = &SSL_CTX_new,
		},
		{
			.name = "SSL_set_hostflags",
			.addr = &SSL_set_hostflags,
		},
		{
			.name = "SSL_set_alpn_protos",
			.addr = &SSL_set_alpn_protos,
		},
		{
			.name = "PEM_read_SSL_SESSION",
			.addr = &PEM_read_SSL_SESSION,
		},
		{
			.name = "TLSv1_method",
			.addr = &TLSv1_method,
		},
		{
			.name = "SSL_alert_type_string_long",
			.addr = &SSL_alert_type_string_long,
		},
		{
			.name = "SSL_set1_groups_list",
			.addr = &SSL_set1_groups_list,
		},
		{
			.name = "SSL_new",
			.addr = &SSL_new,
		},
		{
			.name = "SSL_write_ex",
			.addr = &SSL_write_ex,
		},
		{
			.name = "SSL_set_shutdown",
			.addr = &SSL_set_shutdown,
		},
		{
			.name = "SSL_SESSION_get_max_early_data",
			.addr = &SSL_SESSION_get_max_early_data,
		},
		{
			.name = "BIO_new_buffer_ssl_connect",
			.addr = &BIO_new_buffer_ssl_connect,
		},
		{
			.name = "DTLSv1_method",
			.addr = &DTLSv1_method,
		},
		{
			.name = "SSL_set_session_ticket_ext",
			.addr = &SSL_set_session_ticket_ext,
		},
		{
			.name = "SSL_get_peer_cert_chain",
			.addr = &SSL_get_peer_cert_chain,
		},
		{
			.name = "SSL_connect",
			.addr = &SSL_connect,
		},
		{
			.name = "SSL_SESSION_get_master_key",
			.addr = &SSL_SESSION_get_master_key,
		},
		{
			.name = "SSL_CIPHER_get_value",
			.addr = &SSL_CIPHER_get_value,
		},
		{
			.name = "TLSv1_1_method",
			.addr = &TLSv1_1_method,
		},
		{
			.name = "DTLSv1_client_method",
			.addr = &DTLSv1_client_method,
		},
		{
			.name = "SSL_get_servername",
			.addr = &SSL_get_servername,
		},
		{
			.name = "SSL_get_num_tickets",
			.addr = &SSL_get_num_tickets,
		},
		{
			.name = "SSL_CTX_set_default_passwd_cb",
			.addr = &SSL_CTX_set_default_passwd_cb,
		},
		{
			.name = "SSL_set_SSL_CTX",
			.addr = &SSL_set_SSL_CTX,
		},
		{
			.name = "SSL_SESSION_get_ex_new_index",
			.addr = &SSL_SESSION_get_ex_new_index,
		},
		{
			.name = "SSL_get_wbio",
			.addr = &SSL_get_wbio,
		},
		{
			.name = "SSL_COMP_add_compression_method",
			.addr = &SSL_COMP_add_compression_method,
		},
		{
			.name = "SSL_do_handshake",
			.addr = &SSL_do_handshake,
		},
		{
			.name = "SSL_get0_alpn_selected",
			.addr = &SSL_get0_alpn_selected,
		},
		{
			.name = "SSL_set_tmp_dh_callback",
			.addr = &SSL_set_tmp_dh_callback,
		},
		{
			.name = "SSL_get_client_ciphers",
			.addr = &SSL_get_client_ciphers,
		},
		{
			.name = "SSL_SESSION_get_id",
			.addr = &SSL_SESSION_get_id,
		},
		{
			.name = "SSL_use_RSAPrivateKey_file",
			.addr = &SSL_use_RSAPrivateKey_file,
		},
		{
			.name = "SSL_set_quic_method",
			.addr = &SSL_set_quic_method,
		},
		{
			.name = "SSL_CTX_up_ref",
			.addr = &SSL_CTX_up_ref,
		},
		{
			.name = "SSL_CTX_set_cookie_verify_cb",
			.addr = &SSL_CTX_set_cookie_verify_cb,
		},
		{
			.name = "SSL_pending",
			.addr = &SSL_pending,
		},
		{
			.name = "SSL_get_version",
			.addr = &SSL_get_version,
		},
		{
			.name = "SSL_add_dir_cert_subjects_to_stack",
			.addr = &SSL_add_dir_cert_subjects_to_stack,
		},
		{
			.name = "SSL_set1_host",
			.addr = &SSL_set1_host,
		},
		{
			.name = "SSL_load_error_strings",
			.addr = &SSL_load_error_strings,
		},
		{
			.name = "SSL_set_purpose",
			.addr = &SSL_set_purpose,
		},
		{
			.name = "SSL_process_quic_post_handshake",
			.addr = &SSL_process_quic_post_handshake,
		},
		{
			.name = "SSL_use_PrivateKey",
			.addr = &SSL_use_PrivateKey,
		},
		{
			.name = "SSL_read",
			.addr = &SSL_read,
		},
		{
			.name = "SSL_COMP_get_name",
			.addr = &SSL_COMP_get_name,
		},
		{
			.name = "SSL_CTX_set1_groups",
			.addr = &SSL_CTX_set1_groups,
		},
		{
			.name = "SSL_get_fd",
			.addr = &SSL_get_fd,
		},
		{
			.name = "SSL_CTX_set_default_passwd_cb_userdata",
			.addr = &SSL_CTX_set_default_passwd_cb_userdata,
		},
		{
			.name = "SSL_use_RSAPrivateKey_ASN1",
			.addr = &SSL_use_RSAPrivateKey_ASN1,
		},
		{
			.name = "SSL_set_msg_callback",
			.addr = &SSL_set_msg_callback,
		},
		{
			.name = "SSL_CTX_use_certificate_chain_mem",
			.addr = &SSL_CTX_use_certificate_chain_mem,
		},
		{
			.name = "SSL_CTX_add1_chain_cert",
			.addr = &SSL_CTX_add1_chain_cert,
		},
		{
			.name = "SSL_CTX_ctrl",
			.addr = &SSL_CTX_ctrl,
		},
		{
			.name = "SSL_set_post_handshake_auth",
			.addr = &SSL_set_post_handshake_auth,
		},
		{
			.name = "SSL_renegotiate",
			.addr = &SSL_renegotiate,
		},
		{
			.name = "SSL_alert_type_string",
			.addr = &SSL_alert_type_string,
		},
		{
			.name = "DTLSv1_server_method",
			.addr = &DTLSv1_server_method,
		},
		{
			.name = "SSL_SESSION_is_resumable",
			.addr = &SSL_SESSION_is_resumable,
		},
		{
			.name = "SSL_CTX_use_RSAPrivateKey_file",
			.addr = &SSL_CTX_use_RSAPrivateKey_file,
		},
		{
			.name = "SSL_CIPHER_find",
			.addr = &SSL_CIPHER_find,
		},
		{
			.name = "SSL_CTX_get_cert_store",
			.addr = &SSL_CTX_get_cert_store,
		},
		{
			.name = "TLSv1_client_method",
			.addr = &TLSv1_client_method,
		},
		{
			.name = "SSL_get_error",
			.addr = &SSL_get_error,
		},
		{
			.name = "SSL_rstate_string_long",
			.addr = &SSL_rstate_string_long,
		},
		{
			.name = "SSL_get_selected_srtp_profile",
			.addr = &SSL_get_selected_srtp_profile,
		},
		{
			.name = "SSL_set_fd",
			.addr = &SSL_set_fd,
		},
		{
			.name = "SSL_CTX_remove_session",
			.addr = &SSL_CTX_remove_session,
		},
		{
			.name = "SSL_CTX_set_ciphersuites",
			.addr = &SSL_CTX_set_ciphersuites,
		},
		{
			.name = "SSL_CTX_set_client_cert_cb",
			.addr = &SSL_CTX_set_client_cert_cb,
		},
		{
			.name = "SSL_get_client_CA_list",
			.addr = &SSL_get_client_CA_list,
		},
		{
			.name = "SSL_get_SSL_CTX",
			.addr = &SSL_get_SSL_CTX,
		},
		{
			.name = "SSL_set_state",
			.addr = &SSL_set_state,
		},
		{
			.name = "SSL_read_ex",
			.addr = &SSL_read_ex,
		},
		{
			.name = "SSL_CTX_set_alpn_protos",
			.addr = &SSL_CTX_set_alpn_protos,
		},
		{
			.name = "SSL_get_client_random",
			.addr = &SSL_get_client_random,
		},
		{
			.name = "SSL_CTX_use_RSAPrivateKey_ASN1",
			.addr = &SSL_CTX_use_RSAPrivateKey_ASN1,
		},
		{
			.name = "SSL_set_security_level",
			.addr = &SSL_set_security_level,
		},
		{
			.name = "SSL_set_quic_transport_params",
			.addr = &SSL_set_quic_transport_params,
		},
		{
			.name = "SSL_CTX_set_verify_depth",
			.addr = &SSL_CTX_set_verify_depth,
		},
		{
			.name = "SSL_CTX_set_generate_session_id",
			.addr = &SSL_CTX_set_generate_session_id,
		},
		{
			.name = "SSL_set_session_ticket_ext_cb",
			.addr = &SSL_set_session_ticket_ext_cb,
		},
		{
			.name = "SSL_get0_peername",
			.addr = &SSL_get0_peername,
		},
		{
			.name = "SSL_CTX_set_next_protos_advertised_cb",
			.addr = &SSL_CTX_set_next_protos_advertised_cb,
		},
		{
			.name = "SSL_CTX_check_private_key",
			.addr = &SSL_CTX_check_private_key,
		},
		{
			.name = "SSL_set_cipher_list",
			.addr = &SSL_set_cipher_list,
		},
		{
			.name = "SSL_renegotiate_abbreviated",
			.addr = &SSL_renegotiate_abbreviated,
		},
		{
			.name = "OPENSSL_init_ssl",
			.addr = &OPENSSL_init_ssl,
		},
		{
			.name = "SSL_CTX_get_num_tickets",
			.addr = &SSL_CTX_get_num_tickets,
		},
		{
			.name = "DTLSv1_2_method",
			.addr = &DTLSv1_2_method,
		},
		{
			.name = "SSL_set1_chain",
			.addr = &SSL_set1_chain,
		},
		{
			.name = "SSL_CIPHER_get_auth_nid",
			.addr = &SSL_CIPHER_get_auth_nid,
		},
		{
			.name = "SSL_CTX_set1_groups_list",
			.addr = &SSL_CTX_set1_groups_list,
		},
		{
			.name = "SSL_get_quiet_shutdown",
			.addr = &SSL_get_quiet_shutdown,
		},
		{
			.name = "SSL_SESSION_set_ex_data",
			.addr = &SSL_SESSION_set_ex_data,
		},
		{
			.name = "SSL_CTX_use_certificate_chain_file",
			.addr = &SSL_CTX_use_certificate_chain_file,
		},
		{
			.name = "SSL_CIPHER_get_id",
			.addr = &SSL_CIPHER_get_id,
		},
		{
			.name = "SSL_CTX_set_ex_data",
			.addr = &SSL_CTX_set_ex_data,
		},
		{
			.name = "SSL_CTX_set_default_verify_paths",
			.addr = &SSL_CTX_set_default_verify_paths,
		},
		{
			.name = "SSL_get_privatekey",
			.addr = &SSL_get_privatekey,
		},
		{
			.name = "SSL_write_early_data",
			.addr = &SSL_write_early_data,
		},
		{
			.name = "SSL_CTX_set_verify",
			.addr = &SSL_CTX_set_verify,
		},
		{
			.name = "SSL_get_rbio",
			.addr = &SSL_get_rbio,
		},
		{
			.name = "SSL_SESSION_set_max_early_data",
			.addr = &SSL_SESSION_set_max_early_data,
		},
		{
			.name = "SSL_CTX_set_quic_method",
			.addr = &SSL_CTX_set_quic_method,
		},
		{
			.name = "TLSv1_server_method",
			.addr = &TLSv1_server_method,
		},
		{
			.name = "SSL_set_tmp_rsa_callback",
			.addr = &SSL_set_tmp_rsa_callback,
		},
		{
			.name = "SSL_dup_CA_list",
			.addr = &SSL_dup_CA_list,
		},
		{
			.name = "SSL_CTX_set1_chain",
			.addr = &SSL_CTX_set1_chain,
		},
		{
			.name = "SSL_CTX_set_alpn_select_cb",
			.addr = &SSL_CTX_set_alpn_select_cb,
		},
		{
			.name = "SSL_get_current_cipher",
			.addr = &SSL_get_current_cipher,
		},
		{
			.name = "TLSv1_1_client_method",
			.addr = &TLSv1_1_client_method,
		},
		{
			.name = "SSL_CTX_set_max_proto_version",
			.addr = &SSL_CTX_set_max_proto_version,
		},
		{
			.name = "SSL_set_bio",
			.addr = &SSL_set_bio,
		},
		{
			.name = "SSL_quic_read_level",
			.addr = &SSL_quic_read_level,
		},
		{
			.name = "SSL_CTX_set_client_cert_engine",
			.addr = &SSL_CTX_set_client_cert_engine,
		},
		{
			.name = "SSL_SESSION_get_time",
			.addr = &SSL_SESSION_get_time,
		},
		{
			.name = "SSL_set0_chain",
			.addr = &SSL_set0_chain,
		},
		{
			.name = "SSL_get_server_random",
			.addr = &SSL_get_server_random,
		},
		{
			.name = "SSL_get_ex_new_index",
			.addr = &SSL_get_ex_new_index,
		},
		{
			.name = "SSL_export_keying_material",
			.addr = &SSL_export_keying_material,
		},
		{
			.name = "SSL_set_ssl_method",
			.addr = &SSL_set_ssl_method,
		},
		{
			.name = "BIO_f_ssl",
			.addr = &BIO_f_ssl,
		},
		{
			.name = "SSLv23_method",
			.addr = &SSLv23_method,
		},
		{
			.name = "SSL_set_quic_use_legacy_codepoint",
			.addr = &SSL_set_quic_use_legacy_codepoint,
		},
		{
			.name = "SSL_rstate_string",
			.addr = &SSL_rstate_string,
		},
		{
			.name = "SSL_SESSION_get_protocol_version",
			.addr = &SSL_SESSION_get_protocol_version,
		},
		{
			.name = "i2d_SSL_SESSION",
			.addr = &i2d_SSL_SESSION,
		},
		{
			.name = "TLSv1_2_client_method",
			.addr = &TLSv1_2_client_method,
		},
		{
			.name = "SSL_add0_chain_cert",
			.addr = &SSL_add0_chain_cert,
		},
		{
			.name = "SSL_CTX_use_certificate_file",
			.addr = &SSL_CTX_use_certificate_file,
		},
		{
			.name = "SSL_CTX_free",
			.addr = &SSL_CTX_free,
		},
		{
			.name = "SSL_get_ssl_method",
			.addr = &SSL_get_ssl_method,
		},
		{
			.name = "SSL_CTX_get_verify_depth",
			.addr = &SSL_CTX_get_verify_depth,
		},
		{
			.name = "SSL_set_accept_state",
			.addr = &SSL_set_accept_state,
		},
		{
			.name = "SSL_get_srtp_profiles",
			.addr = &SSL_get_srtp_profiles,
		},
		{
			.name = "SSL_get_shared_ciphers",
			.addr = &SSL_get_shared_ciphers,
		},
		{
			.name = "SSL_get_max_proto_version",
			.addr = &SSL_get_max_proto_version,
		},
		{
			.name = "SSL_set_verify_result",
			.addr = &SSL_set_verify_result,
		},
		{
			.name = "SSL_CTX_set_msg_callback",
			.addr = &SSL_CTX_set_msg_callback,
		},
		{
			.name = "SSL_set_tlsext_use_srtp",
			.addr = &SSL_set_tlsext_use_srtp,
		},
		{
			.name = "SSL_get0_verified_chain",
			.addr = &SSL_get0_verified_chain,
		},
		{
			.name = "SSL_get_early_data_status",
			.addr = &SSL_get_early_data_status,
		},
		{
			.name = "SSL_SESSION_set1_id_context",
			.addr = &SSL_SESSION_set1_id_context,
		},
	};

	for (i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)
		fprintf(stderr, "%s: %p\n", symbols[i].name, symbols[i].addr);

	printf("OK\n");

	return 0;
}
