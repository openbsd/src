CFLAGS+=	-I${.CURDIR}/..

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -ltcfs
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libtcfs.a
.else
LDADD+=         -L${.CURDIR}/../lib -ltcfs
DPADD+=         ${.CURDIR}/../lib/libtcfs.a
.endif
