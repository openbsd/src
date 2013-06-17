# $Id: Makefile.am,v 1.1 2013/06/17 19:11:45 robert Exp $

include $(top_srcdir)/Makefile.am.common

SUBDIRS = bin db kdc gss ldap can java

if ENABLE_SHARED
if HAVE_DLOPEN
SUBDIRS += plugin
endif
endif

EXTRA_DIST = NTMakefile
