#	$OpenBSD: Makefile,v 1.2 1996/08/20 05:14:10 tholo Exp $
#	$NetBSD: Makefile,v 1.13 1995/04/23 07:58:46 cgd Exp $

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CFLAGS += -g -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE_ARCH)

SUBDIR=	ldconfig ldd

.PATH: $(.CURDIR)/$(MACHINE_ARCH)

.include <bsd.prog.mk>
.if !defined(NOPIC)
SUBDIR+= rtld
.endif
