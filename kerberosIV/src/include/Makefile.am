# $KTH: Makefile.am,v 1.3 2000/12/13 14:38:59 assar Exp $

include $(top_srcdir)/Makefile.am.common

SUBDIR = sys

noinst_PROGRAMS = bits

INCLUDES += -DHOST=\"$(CANONICAL_HOST)\"

include_HEADERS = ktypes.h protos.h

CLEANFILES = roken.h krb_err.h kadm_err.h

ktypes.h: bits$(EXEEXT)
	./bits$(EXEEXT) ktypes.h

CLEANFILES =					\
	acl.h					\
	base64.h				\
	com_err.h				\
	com_right.h				\
	des.h					\
	editline.h				\
	err.h					\
	getarg.h				\
	glob.h					\
	kadm.h					\
	kadm_err.h				\
	kafs.h					\
	kdc.h					\
	klog.h					\
	krb-archaeology.h			\
	krb-protos.h				\
	krb.h					\
	krb_db.h				\
	krb_err.h				\
	krb_log.h				\
	ktypes.h				\
	md4.h					\
	md5.h					\
	otp.h					\
	parse_bytes.h				\
	parse_time.h				\
	parse_units.h				\
	prot.h					\
	protos.h				\
	roken.h					\
	rc4.h					\
	resolve.h				\
	roken-common.h				\
	sha.h					\
	sl.h					\
	xdbm.h

protos.h: $(srcdir)/protos.hin
	$(LN_S) $(srcdir)/protos.hin protos.h
