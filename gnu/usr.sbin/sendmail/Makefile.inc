#	$OpenBSD: Makefile.inc,v 1.7 2001/10/01 20:18:19 millert Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

ENVDEF+= -DSM_OMIT_BOGUS_WARNINGS -DNEWDB -DMAP_REGEX
ENVDEF+= -DNETINET6 -DNEEDSGETIPNODE
.if (${YP:L} == "yes")
ENVDEF+=-DNIS
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

.if defined(WANT_LIBSM)
LDADD+= -lsm
.if exists(${.CURDIR}/../libsm/${__objdir})
DPADD+= ${.CURDIR}/../libsm/${__objdir}/libsm.a
LDFLAGS+=-L${.CURDIR}/../libsm/${__objdir}
.else
DPADD+= ${.CURDIR}/../libsm/libsm.a
LDFLAGS+=-L${.CURDIR}/../libsm
.endif
.endif

.if defined(WANT_LIBMILTER)
LDADD+= -lmilter
.if exists(${.CURDIR}/../libmilter/${__objdir})
DPADD+= ${.CURDIR}/../libmilter/${__objdir}/libmilter.a
LDFLAGS+=-L${.CURDIR}/../libmilter/${__objdir}
.else
DPADD+= ${.CURDIR}/../libmilter/libmilter.a
LDFLAGS+=-L${.CURDIR}/../libmilter
.endif
.endif

.if defined(WANT_LIBWRAP)
.if (${TCP_WRAPPERS:L} == "yes")
ENVDEF+=-DTCPWRAPPERS
DPADD+= ${LIBWRAP}
LDADD+= -lwrap
.endif
.endif

CFLAGS+=${ENVDEF} -I${.CURDIR}/../sendmail -I${.CURDIR}/../include
