#	@(#)Makefile	8.1 (Berkeley) 6/1/93

LIB=	krb
CFLAGS+=-I${.CURDIR}
SRCS=	cr_err_reply.c create_auth_reply.c create_ciph.c \
	create_death_packet.c create_ticket.c debug_decl.c decomp_ticket.c \
	dest_tkt.c extract_ticket.c fgetst.c get_ad_tkt.c get_admhst.c \
	get_cred.c get_in_tkt.c get_krbhst.c get_krbrlm.c get_phost.c \
	get_pw_tkt.c get_request.c get_svc_in_tkt.c get_tf_fullname.c \
	get_tf_realm.c getrealm.c getst.c in_tkt.c k_localtime.c klog.c \
	kname_parse.c kntoln.c kparse.c krb_err.c krb_err_txt.c \
	krb_get_in_tkt.c kuserok.c lifetime.c log.c mk_err.c mk_priv.c \
	mk_req.c mk_safe.c month_sname.c netread.c netwrite.c one.c \
	pkt_cipher.c pkt_clen.c rd_err.c rd_priv.c rd_req.c rd_safe.c \
	read_service_key.c recvauth.c save_credentials.c send_to_kdc.c \
	sendauth.c str2key.c tf_util.c tkt_string.c
MAN=	kerberos.3 krb_realmofhost.3 krb_sendauth.3 krb_set_tkt_string.3 \
	kuserok.3 tf_util.3
MLINKS+=kerberos.3 krb_mk_req.3 kerberos.3 krb_rd_req.3 kerberos.3 krb_kntoln.3 \
	kerberos.3 krb_set_key.3 kerberos.3 krb_get_cred.3 kerberos.3 krb_mk_priv.3 \
	kerberos.3 krb_rd_priv.3 kerberos.3 krb_mk_safe.3 kerberos.3 krb_rd_safe.3 \
	kerberos.3 krb_mk_err.3 kerberos.3 krb_rd_err.3 kerberos.3 krb_ck_repl.3
MLINKS+=krb_realmofhost.3 krb_get_phost.3 krb_realmofhost.3 krb_get_krbhst.3 \
	krb_realmofhost.3 krb_get_admhst.3 krb_realmofhost.3 krb_get_lrealm.3
MLINKS+=krb_sendauth.3 krb_recvauth.3 krb_sendauth.3 krb_net_write.3 \
	krb_sendauth.3 krb_net_read.3
MLINKS+=tf_util.3 tf_init.3 tf_util.3 tf_get_pname.3 tf_util.3 tf_get_pinst.3 \
	tf_util.3 tf_get_cred.3 tf_util.3 tf_close.3

LDADD+=	${COM_ERR}
CLEANFILES+=	krb_err.c krb_err.h

krb_err.c krb_err.h: krb_err.et
	test -e krb_err.et || ln -s ${.CURDIR}/krb_err.et .
	compile_et krb_err.et
	-test -h krb_err.et && rm krb_err.et

includes: krb_err.h
	-cd ${.OBJDIR}; cmp -s krb_err.h ${DESTDIR}/usr/include/kerberosIV/krb_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 krb_err.h ${DESTDIR}/usr/include/kerberosIV

.include <bsd.lib.mk>
