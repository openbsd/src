# $OpenBSD: Makefile,v 1.1 2001/05/25 08:18:55 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

LIB=	krb5

INCLUDE_FILES=	${KRB5SRC}/lib/krb5/{krb5.h,krb5-protos.h,krb5-private.h} \
		${KRB5DIR}/include/krb5-types.h krb5_err.h heim_err.h

includes:	krb5_err.h heim_err.h

MAN=	krb5.conf.5 \
	krb5_425_conv_principal.3 \
	krb5_appdefault.3 \
	krb5_build_principal.3 \
	krb5_config.3 \
	krb5_free_principal.3 \
	krb5_openlog.3 \
	krb5_parse_name.3 \
	krb5_sname_to_principal.3 \
	krb5_unparse_name.3 \
	krb5_warn.3 \
	krb5_auth_context.3 \
	krb5_context.3 \
	krb5_init_context.3
# This not built yet
#	verify_krb5_conf.8

MLINKS+=krb5_init_context.3 krb5_free_context.3

SRCS=	add_et_list.c \
	addr_families.c \
	address.c \
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
	convert_creds.c \
	constants.c \
	context.c \
	copy_host_realm.c \
	crc.c \
	creds.c \
	crypto.c \
	data.c \
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
	keyblock.c \
	keytab.c \
	keytab_file.c \
	keytab_memory.c \
	keytab_krb4.c \
	keytab_keyfile.c \
	krbhst.c \
	kuserok.c \
	log.c \
	mcache.c \
	mk_error.c \
	mk_priv.c \
	mk_rep.c \
	mk_req.c \
	mk_req_ext.c \
	mk_safe.c \
	net_read.c \
	net_write.c \
	n-fold.c \
	padata.c \
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
	verify_init.c \
	verify_user.c \
	version.c \
	warn.c \
	write_message.c \
	krb5_err.c \
	heim_err.c

CLEANFILES+=	krb5_err.[ch] heim_err.[ch]

.PATH:	${KRB5SRC}/lib/krb5
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/ \
	-I${KRB5SRC}/lib/asn1 -I../asn1 -I. \
	-DSHA1_CTX=SHA_CTX

LDADD+=	-lc -L../asn1 -lasn1_pic ${LIBROKEN_PIC} -lcrypto

krb5_err.c krb5_err.h:	${KRB5SRC}/lib/krb5/krb5_err.et
	compile_et ${KRB5SRC}/lib/krb5/krb5_err.et

heim_err.c heim_err.h:	${KRB5SRC}/lib/krb5/heim_err.et
	compile_et ${KRB5SRC}/lib/krb5/heim_err.et

.include <bsd.lib.mk>