#	$OpenBSD: Makefile,v 1.5 2004/03/02 07:07:34 david Exp $
#	$NetBSD: Makefile,v 1.3 1995/04/20 22:42:19 cgd Exp $
#
# C Optimizer (-O) breaks this program - don't use !!
CFLAGS=-g
PROG=	testldt

DPADD+=	${LIBARCH}
LDADD+=	-li386

USERLDT!=sysctl -n machdep.userldt

.if ${MACHINE_ARCH} != "i386" || $(USERLDT) == 0
REGRESS_SKIP="yes"
.endif

.include <bsd.regress.mk>

