# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.41.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2013/05/18 18:18:21 miod Exp $

.include <bsd.own.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR=	cc_tools libcpp libdecnumber cc_int cc cc1 include doc

SUBDIR+= cpp
SUBDIR+= cc1plus c++ c++filt
SUBDIR+= cc1obj
SUBDIR+= gcov
SUBDIR+= libgcc
SUBDIR+= libgcov
SUBDIR+= collect2

.include <bsd.subdir.mk>
