# $KTH: Makefile.am,v 1.3 2001/08/26 01:40:43 assar Exp $

include $(top_srcdir)/Makefile.am.common

if OTP
dir_otp = otp
endif

SUBDIRS	= @DIR_roken@ vers com_err @DIR_des@ krb kdb kadm acl \
	kafs auth editline sl $(dir_otp)
