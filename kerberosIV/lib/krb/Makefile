# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:25 hin Exp $

LIB	= krb

KRBDEPTH=../..
.include <../../Makefile.inc>

INCLUDE_FILES	= ${KRBSRC}/lib/krb/{krb.h,krb-protos.h,prot.h} krb_err.h

.PATH:	${KRBSRC}/lib/krb ${KRBSRC}/lib/roken ${KRBSRC}/lib/des

CFLAGS	+= -I${KRBSRC}/lib/des -I${KRBSRC}/lib/krb -I${BSDSRCDIR}/lib/libcom_err

SRCS	= \
	check_time.c \
	cr_err_reply.c \
	create_auth_reply.c \
	create_ciph.c \
	create_ticket.c \
	debug_decl.c \
	decomp_ticket.c \
	dest_tkt.c \
	encrypt_ktext.c \
	extra.c \
	get_ad_tkt.c \
	getfile.c \
	get_cred.c \
	get_default_principal.c \
	get_host.c \
	get_in_tkt.c \
	get_krbrlm.c \
	get_svc_in_tkt.c \
	get_tf_fullname.c \
	get_tf_realm.c \
	getaddrs.c \
	getrealm.c \
	getst.c \
	k_getport.c \
	k_getsockinst.c \
	k_localtime.c \
	kdc_reply.c \
	kntoln.c \
	krb_check_auth.c \
	krb_equiv.c \
	krb_err.c \
	krb_err_txt.c \
	krb_get_in_tkt.c \
	kuserok.c \
	lifetime.c \
	logging.c \
	lsb_addr_comp.c \
	mk_auth.c \
	mk_err.c \
	mk_priv.c \
	mk_req.c \
	mk_safe.c \
	month_sname.c \
	name2name.c \
	krb_net_read.c \
	krb_net_write.c \
	one.c \
	parse_name.c \
	rd_err.c \
	rd_priv.c \
	rd_req.c \
	rd_safe.c \
	read_service_key.c \
	realm_parse.c \
	recvauth.c \
	rw.c \
	save_credentials.c \
	send_to_kdc.c \
	sendauth.c \
	stime.c \
	str2key.c \
	tf_util.c \
	time.c \
	tkt_string.c \
	unparse_name.c \
	verify_user.c \
	defaults.c \
	krb_ip_realm.c


# These files are not built here: create_death_packet.c, solaris_compat.c


# these files reside in ../roken or ../com_err/
SRCS	+= \
	base64.c \
	concat.c \
	resolve.c \
	snprintf.c \
	strnlen.c \
	strtok_r.c \
	net_write.c \
	net_read.c

# These function is not available in OpenBSD's libdes
SRCS	+= \
	rnd_keys.c

beforedepend prereq: krb_err.h

includes:	krb_err.h

krb_err.c krb_err.h:	krb_err.et
	${COMPILE_ET} ${KRBSRC}/lib/krb/krb_err.et

CLEANFILES	+= krb_err.h krb_err.c

# XXX This has just been stolen from the old Makefile, may be
# some missing manpages.

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

.include <bsd.lib.mk>
