# $Id: Makefile.am,v 1.3 2013/06/17 19:11:35 robert Exp $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_krb4) 

noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
	sockbuf.c \
	buffer.c \
	common.h

EXTRA_DIST = NTMakefile