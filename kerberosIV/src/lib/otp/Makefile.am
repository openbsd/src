# $KTH: Makefile.am,v 1.11 1999/10/19 23:52:14 assar Exp $

include $(top_srcdir)/Makefile.am.common

noinst_PROGRAMS = otptest

otptest_LDADD = libotp.la \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_roken) \
	$(DBLIB)

include_HEADERS = otp.h

lib_LTLIBRARIES = libotp.la
libotp_la_LDFLAGS = -version-info 0:1:0

libotp_la_SOURCES = \
	otp.c \
	otp_challenge.c \
	otp_db.c \
	otp_md.c \
	otp_parse.c \
	otp_print.c \
	otp_verify.c \
	otp_locl.h \
	otp_md.h \
	roken_rename.h
