#	$OpenBSD: Makefile,v 1.4 2002/09/02 20:01:43 avsm Exp $
#	$NetBSD: Makefile,v 1.3 1995/04/20 22:42:19 cgd Exp $
#
# C Optimizer (-O) breaks this program - don't use !!
CFLAGS=-g
PROG=	testldt

DPADD+=	${LIBARCH}
LDADD+=	-li386

.if ${MACHINE_ARCH} != "i386"
REGRESS_SKIP="yes"
.endif

.include <bsd.regress.mk>

