# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.41.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.1.1.1 2009/10/15 20:50:19 robert Exp $

.include <bsd.own.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR=	cc_tools libcpp libdecnumber cc_int cc cc1 include doc

SUBDIR+= cpp
SUBDIR+= cc1plus c++ c++filt
SUBDIR+= cc1obj
SUBDIR+= gcov
SUBDIR+= libgcc
SUBDIR+= collect2

.include <bsd.subdir.mk>
