#	$OpenBSD: Makefile.inc,v 1.3 2000/05/15 16:16:38 millert Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

ENVDEF+= -DNEWDB -DMAP_REGEX
# XXX - IPv6 support breaks IPv4 support since we don't do RFC2553
#ENVDEF+= -DNETINET6 -DNEEDSGETIPNODE
.if (${YP} == "yes")
ENVDEF+=-DNIS
.endif

.if defined(WANT_LIBWRAP)
.if (${TCP_WRAPPERS} == "yes")
ENVDEF+=-DTCPWRAPPERS
DPADD+= ${LIBWRAP}
LDADD+= -lwrap
.endif
.endif

.if defined(WANT_LIBSMDB)
LDADD+= -lsmdb
.if exists(${.CURDIR}/../libsmdb/${__objdir})
DPADD+= ${.CURDIR}/../libsmdb/${__objdir}/libsmdb.a
LDFLAGS+=-L${.CURDIR}/../libsmdb/${__objdir}
.else
DPADD+= ${.CURDIR}/../libsmdb/libsmdb.a
LDFLAGS+=-L${.CURDIR}/../libsmdb
.endif
.endif

.if defined(WANT_LIBSMUTIL)
LDADD+= -lsmutil
.if exists(${.CURDIR}/../libsmutil/${__objdir})
DPADD+= ${.CURDIR}/../libsmutil/${__objdir}/libsmutil.a
LDFLAGS+=-L${.CURDIR}/../libsmutil/${__objdir}
.else
DPADD+= ${.CURDIR}/../libsmutil/libsmutil.a
LDFLAGS+=-L${.CURDIR}/../libsmutil
.endif
.endif

CFLAGS+=${ENVDEF} -I${.CURDIR}/../sendmail -I${.CURDIR}/../include
