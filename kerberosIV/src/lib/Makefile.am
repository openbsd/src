# $KTH: Makefile.am,v 1.2 2000/11/05 17:16:34 joda Exp $

include $(top_srcdir)/Makefile.am.common

if OTP
dir_otp = otp
endif

SUBDIRS	= @DIR_roken@ vers com_err des krb kdb kadm acl \
	kafs auth editline sl $(dir_otp)
