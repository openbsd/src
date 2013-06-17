# $Id: Makefile.am,v 1.1 2013/06/17 19:11:44 robert Exp $

include $(top_srcdir)/Makefile.am.common

lib_LTLIBRARIES = libheimsqlite.la

noinst_HEADERS = sqlite3.h sqlite3ext.h

libheimsqlite_la_SOURCES = sqlite3.c

libheimsqlite_la_LIBADD = $(PTHREAD_LIBADD)

EXTRA_DIST = NTMakefile
