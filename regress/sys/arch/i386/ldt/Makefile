#	$OpenBSD: Makefile,v 1.3 2002/02/22 23:33:59 art Exp $
#	$NetBSD: Makefile,v 1.3 1995/04/20 22:42:19 cgd Exp $
#
# C Optimizer (-O) breaks this program - don't use !!
CFLAGS=-g
PROG=	testldt

DPADD+=	${LIBARCH}
LDADD+=	-li386

.if ${MACHINE_ARCH} != "i386"
REGRESSSKIP="yes"
.endif

.include <bsd.regress.mk>

