#	$OpenBSD: Makefile,v 1.2 2001/09/06 19:24:47 jason Exp $
#	$NetBSD: Makefile,v 1.13 2000/08/16 08:24:01 mrg Exp $

.include <bsd.own.mk>

WARNS?=1
PROG=	installboot
SRCS=	installboot.c loadfile.c
NOMAN=

LIBSA_I=${.CURDIR}/../../../../lib/libsa

.PATH.c: ${.CURDIR}/../../../../lib/libsa

CPPFLAGS+=-I${LIBSA_I} -I. -I${.CURDIR}/../../../.. -I${.OBJDIR}

LDSTATIC?=	-static

.if !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../../${MACHINE}/include machine)
	@([ -h sparc ] || ln -s ${.CURDIR}/../../../sparc/include sparc)
.NOPATH: machine sparc
CLEANFILES+= machine sparc
.endif

CFLAGS+= -U_STANDALONE

.include <bsd.prog.mk>
