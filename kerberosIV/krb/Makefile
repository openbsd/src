#	@(#)Makefile	8.1 (Berkeley) 6/1/93

LIB=	krb
CFLAGS+=-I${.CURDIR} -DPARANOIA
SRCS=	cr_err_reply.c \
          create_auth_reply.c \
          create_ciph.c \
          create_death_packet.c \
          create_ticket.c \
          dest_tkt.c \
          get_in_tkt.c \
          get_svc_in_tkt.c \
          getrealm.c \
          k_localtime.c \
          krb_err_txt.c \
          krb_get_in_tkt.c \
          kuserok.c \
          parse_name.c \
          kntoln.c \
          mk_auth.c \
          krb_check_auth.c \
          mk_err.c \
          mk_safe.c \
          rd_err.c \
          rd_safe.c \
          recvauth.c \
          mk_priv.c \
          rd_req.c \
          decomp_ticket.c \
          lifetime.c \
          month_sname.c \
          stime.c \
          read_service_key.c \
          getst.c \
          sendauth.c \
          netread.c \
          netwrite.c \
          rd_priv.c \
          krb_equiv.c \
          str2key.c \
          get_ad_tkt.c \
          mk_req.c \
          get_cred.c \
          get_tf_realm.c \
          get_tf_fullname.c \
          one.c \
          save_credentials.c \
          send_to_kdc.c \
          get_host.c \
          get_krbrlm.c \
          k_gethostname.c \
          tf_util.c \
          debug_decl.c \
          k_flock.c \
          tkt_string.c \
          getaddrs.c \
          k_getsockinst.c \
          k_getport.c \
          lsb_addr_comp.c \
          name2name.c \
          get_default_principal.c \
          realm_parse.c \
          verify_user.c \
          rw.c \
          kdc_reply.c \
          encrypt_ktext.c \
          check_time.c \
          krb_err.c \
          et_list.c \
          resolve.c \
          unparse_name.c \
          logging.c \
          k_concat.c \
	  strtok_r.c \
	  base64.c 

# XXX base64.c and strtok_r.c should really be somewhere else. 
# (or replaced) (they are from libroken)

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
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 krb_err.h ${DESTDIR}/usr/include/kerberosIV

.include <bsd.lib.mk>
