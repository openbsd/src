#	$NetBSD: Makefile,v 1.3 1995/04/20 22:42:19 cgd Exp $
#
# C Optimizer (-O) breaks this program - don't use !!
CFLAGS=-g
PROG=	testldt
NOMAN=	noman

DPADD+=	${LIBARCH}
LDADD+=	-li386

install:

regress:
	-@if ./testldt; \
	then echo PASSED; \
	else echo FAILED; fi


.include <bsd.prog.mk>

