#	$OpenBSD: Makefile,v 1.2 2015/07/27 02:27:32 yasuoka Exp $

LIB=    	radius
SRCS=		radius.c radius_attr.c radius_msgauth.c radius_userpass.c \
		radius_mppe.c radius_eapmsk.c
INCS=		radius.h

CFLAGS+=	-Wall

MAN=		radius_new_request_packet.3
MLINKS+=	radius_new_request_packet.3 radius_new_response_packet.3
MLINKS+=	radius_new_request_packet.3 radius_convert_packet.3
MLINKS+=	radius_new_request_packet.3 radius_delete_packet.3
MLINKS+=	radius_new_request_packet.3 radius_get_id.3
MLINKS+=	radius_new_request_packet.3 radius_get_code.3
MLINKS+=	radius_new_request_packet.3 radius_get_authenticator.3
MLINKS+=	radius_new_request_packet.3 radius_get_authenticator_retval.3
MLINKS+=	radius_new_request_packet.3 radius_get_length.3
MLINKS+=	radius_new_request_packet.3 radius_update_id.3
MLINKS+=	radius_new_request_packet.3 radius_set_id.3
MLINKS+=	radius_new_request_packet.3 radius_set_request_packet.3
MLINKS+=	radius_new_request_packet.3 radius_get_request_packet.3
MLINKS+=	radius_new_request_packet.3 radius_set_response_authenticator.3
MLINKS+=	radius_new_request_packet.3 \
		    radius_check_response_authenticator.3
MLINKS+=	radius_new_request_packet.3 \
		    radius_set_accounting_request_authenticator.3
MLINKS+=	radius_new_request_packet.3 \
		    radius_check_accounting_request_authenticator.3
MLINKS+=	radius_new_request_packet.3 \
		    radius_get_request_authenticator_retval.3
MLINKS+=	radius_new_request_packet.3 radius_get_data.3
MLINKS+=	radius_new_request_packet.3 radius_get_raw_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_raw_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_raw_attr_cat.3
MLINKS+=	radius_new_request_packet.3 radius_put_raw_attr_cat.3
MLINKS+=	radius_new_request_packet.3 radius_get_raw_attr_ptr.3
MLINKS+=	radius_new_request_packet.3 radius_set_raw_attr.3
MLINKS+=	radius_new_request_packet.3 radius_del_attr_all.3
MLINKS+=	radius_new_request_packet.3 radius_has_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_raw_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_raw_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_raw_attr_cat.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_raw_attr_cat.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_raw_attr_ptr.3
MLINKS+=	radius_new_request_packet.3 radius_set_vs_raw_attr.3
MLINKS+=	radius_new_request_packet.3 radius_del_vs_attr_all.3
MLINKS+=	radius_new_request_packet.3 radius_has_vs_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_uint16_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_uint32_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_uint64_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_ipv4_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_ipv6_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_uint16_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_uint32_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_uint64_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_ipv4_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_ipv6_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_uint16_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_uint32_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_uint64_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_ipv4_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_ipv6_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_string_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_string_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_uint16_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_uint32_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_uint64_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_ipv4_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_ipv6_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_uint16_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_uint32_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_uint64_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_ipv4_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_ipv6_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_vs_uint16_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_vs_uint32_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_vs_uint64_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_vs_ipv4_attr.3
MLINKS+=	radius_new_request_packet.3 radius_set_vs_ipv6_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_vs_string_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_vs_string_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_message_authenticator.3
MLINKS+=	radius_new_request_packet.3 radius_set_message_authenticator.3
MLINKS+=	radius_new_request_packet.3 \
		    radius_check_message_authenticator.3
MLINKS+=	radius_new_request_packet.3 \
		    radius_encrypt_user_password_attr.3
MLINKS+=	radius_new_request_packet.3 \
		    radius_decrypt_user_password_attr.3
MLINKS+=	radius_new_request_packet.3 radius_encrypt_mppe_key_attr.3
MLINKS+=	radius_new_request_packet.3 radius_decrypt_mppe_key_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_user_password_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_user_password_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_mppe_send_key_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_mppe_send_key_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_mppe_recv_key_attr.3
MLINKS+=	radius_new_request_packet.3 radius_put_mppe_recv_key_attr.3
MLINKS+=	radius_new_request_packet.3 radius_get_eap_msk.3
MLINKS+=	radius_new_request_packet.3 radius_send.3
MLINKS+=	radius_new_request_packet.3 radius_sendmsg.3
MLINKS+=	radius_new_request_packet.3 radius_sendto.3
MLINKS+=	radius_new_request_packet.3 radius_recv.3
MLINKS+=	radius_new_request_packet.3 radius_recvfrom.3
MLINKS+=	radius_new_request_packet.3 radius_recvmsg.3

.include <bsd.lib.mk>

includes:
	@cd ${.CURDIR}; for i in $(INCS); do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		    -m 444 $$i ${DESTDIR}/usr/include"; \
		echo $$j; \
		eval "$$j"; \
	done
