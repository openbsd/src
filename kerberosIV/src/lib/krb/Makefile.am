# $KTH: Makefile.am,v 1.6 2001/09/13 05:01:43 assar Exp $



include $(top_srcdir)/Makefile.am.common

INCLUDES += -DROKEN_RENAME

incdir = $(includedir)
inc_DATA = krb_err.h
idir = $(top_builddir)/include

include_HEADERS =  klog.h krb-archaeology.h krb-protos.h \
	krb.h prot.h krb_log.h krb_err.h

build_HEADERZ = $(include_HEADERS)

noinst_PROGRAMS = sizetest

man_MANS = kerberos.1 kerberos.3 krb_realmofhost.3 krb_sendauth.3	\
	krb_set_tkt_string.3 kuserok.3 tf_util.3 krb.conf.5 krb.equiv.5	\
	krb.extra.5 krb.realms.5


lib_LTLIBRARIES = libkrb.la
libkrb_la_LDFLAGS = -version-info 2:0:1

libkrb_la_LIBADD = $(LIB_des)

libkrb_la_SOURCES = \
	check_time.c \
	cr_err_reply.c \
	create_auth_reply.c \
	create_ciph.c \
	create_death_packet.c \
	create_ticket.c \
	debug_decl.c \
	decomp_ticket.c \
	defaults.c \
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
	random_block.c \
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
	solaris_compat.c \
	stime.c \
	str2key.c \
	tf_util.c \
	time.c \
	tkt_string.c \
	unparse_name.c \
	verify_user.c \
	krb_ip_realm.c \
	$(EXTRA_SOURCE) \
	$(SHLIB_EXTRA_SOURCE)

EXTRA_SOURCE = 	\
	base64.c \
	concat.c \
	flock.c \
	gethostname.c \
	gettimeofday.c \
	getuid.c \
	net_read.c \
	net_write.c \
	resolve.c \
	snprintf.c \
	strcasecmp.c \
	strlcat.c \
	strlcpy.c \
	strdup.c \
	strncasecmp.c \
	strnlen.c \
	strtok_r.c \
	swab.c

SHLIB_EXTRA_SOURCE = \
	com_err.c \
	error.c

CLEANFILES = krb_err.c krb_err.h
  
krb_err.c krb_err.h: krb_err.et

base64.c:
	$(LN_S) $(srcdir)/../roken/base64.c .
concat.c:
	$(LN_S) $(srcdir)/../roken/concat.c .
flock.c:
	$(LN_S) $(srcdir)/../roken/flock.c .
gethostname.c:
	$(LN_S) $(srcdir)/../roken/gethostname.c .
gettimeofday.c:
	$(LN_S) $(srcdir)/../roken/gettimeofday.c .
getuid.c:
	$(LN_S) $(srcdir)/../roken/getuid.c .
snprintf.c:
	$(LN_S) $(srcdir)/../roken/snprintf.c .
strcasecmp.c:
	$(LN_S) $(srcdir)/../roken/strcasecmp.c .
strlcat.c:
	$(LN_S) $(srcdir)/../roken/strlcat.c .
strlcpy.c:
	$(LN_S) $(srcdir)/../roken/strlcpy.c .
strncasecmp.c:
	$(LN_S) $(srcdir)/../roken/strncasecmp.c .
strnlen.c:
	$(LN_S) $(srcdir)/../roken/strnlen.c .
strdup.c:
	$(LN_S) $(srcdir)/../roken/strdup.c .
strtok_r.c:
	$(LN_S) $(srcdir)/../roken/strtok_r.c .
swab.c:
	$(LN_S) $(srcdir)/../roken/swab.c .
resolve.c:
	$(LN_S) $(srcdir)/../roken/resolve.c .
net_read.c:
	$(LN_S) $(srcdir)/../roken/net_read.c .
net_write.c:
	$(LN_S) $(srcdir)/../roken/net_write.c .
com_err.c:
	$(LN_S) $(srcdir)/../com_err/com_err.c .
error.c:
	$(LN_S) $(srcdir)/../com_err/error.c .


$(OBJECTS): ../../include/config.h
$(OBJECTS): krb_locl.h krb.h krb_err.h
rw.o: ../../include/version.h
