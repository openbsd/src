#	$OpenBSD: Makefile.inc,v 1.15 2004/05/17 14:52:58 millert Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

.if (${MACHINE_ARCH} != "vax")
ENVDEF+=-DSM_OMIT_BOGUS_WARNINGS
.endif
ENVDEF+= -DNEWDB -DMAP_REGEX
ENVDEF+= -DNETINET6 -DNEEDSGETIPNODE -DSM_CONF_SHM
ENVDEF+= -D_FFR_QUEUERETURN_DSN
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

.if defined(WANT_LIBWRAP)
.if (${TCP_WRAPPERS:L} == "yes")
ENVDEF+=-DTCPWRAPPERS
DPADD+= ${LIBWRAP}
LDADD+= -lwrap
.endif
.endif

# To build with LDAP map support define WANT_LDAP in /etc/mk.conf (unsupported)
.if defined(WANT_LDAP)
ENVDEF+= -DLDAPMAP
LDADD+=-lldap -llber
CFLAGS+=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib
.endif

CFLAGS+=${ENVDEF} -I${.CURDIR}/../sendmail -I${.CURDIR}/../include
