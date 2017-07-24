# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.41.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.3 2017/07/24 19:23:57 robert Exp $

.include <bsd.own.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR=	cc_tools libcpp libdecnumber cc_int cc cc1 include doc

.if ${COMPILER_VERSION:L} == "gcc4"
SUBDIR+= cpp
.endif

SUBDIR+= cc1plus c++ c++filt
SUBDIR+= cc1obj
SUBDIR+= gcov
SUBDIR+= libgcc
SUBDIR+= libgcov
SUBDIR+= collect2

.include <bsd.subdir.mk>
