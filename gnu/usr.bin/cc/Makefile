# $FreeBSD: src/gnu/usr.bin/cc/Makefile,v 1.41.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.5 2025/09/29 18:29:23 miod Exp $

.include <bsd.own.mk>

# The order of some of these are rather important.  Some depend on previous
# subdirs.

SUBDIR=	cc_tools libcpp libdecnumber cc_int cc cc1 include doc

.if ${COMPILER_VERSION:L} == "gcc4" || make(obj)
SUBDIR+= cpp
.endif

SUBDIR+= cc1plus c++ c++filt
SUBDIR+= gcov
SUBDIR+= libgcc
SUBDIR+= libgcov
SUBDIR+= collect2

.include <bsd.subdir.mk>
