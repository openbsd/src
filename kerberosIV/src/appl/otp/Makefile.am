# $KTH: Makefile.am,v 1.7 1999/04/09 18:23:51 assar Exp $

include $(top_srcdir)/Makefile.am.common

bin_PROGRAMS = otp otpprint
bin_SUIDS = otp
otp_SOURCES = otp.c otp_locl.h
otpprint_SOURCES = otpprint.c otp_locl.h

LDADD = \
	$(top_builddir)/lib/otp/libotp.la \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_roken) \
	$(DBLIB)
