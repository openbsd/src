#	$OpenBSD: Makefile.inc,v 1.1 2000/04/02 19:48:09 millert Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

ENVDEF= -DNEWDB -DMAP_REGEX
# XXX - adding IPv6 support seems to break IPv4 support in OpenBSD
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
