#	$OpenBSD: Makefile.inc,v 1.3 2000/06/20 08:01:20 fgsch Exp $

CFLAGS+=	-I${.CURDIR}/..

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -ltcfs
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libtcfs.a
.else
LDADD+=         -L${.CURDIR}/../lib -ltcfs
DPADD+=         ${.CURDIR}/../lib/libtcfs.a
.endif
