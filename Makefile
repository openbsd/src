#	$OpenBSD: Makefile,v 1.8 1996/05/06 21:44:03 deraadt Exp $
#	$NetBSD: Makefile,v 1.25 1995/10/09 02:11:28 thorpej Exp $

.include <bsd.own.mk>	# for NOMAN, if it's there.

# NOTE THAT etc *DOES NOT* BELONG IN THE LIST BELOW

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share games
SUBDIR+= gnu

SUBDIR+= sys lkm

.if defined(KERBEROS)
SUBDIR+= kerberosIV
.endif

.if exists(regress)
.ifmake !(install)
SUBDIR+= regress
.endif

regression-tests:
	@echo Running regression tests...
	@(cd ${.CURDIR}/regress && ${MAKE} regress)
.endif

beforeinstall:
.ifndef DESTDIR
	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR=/ distrib-dirs)
.else
	(cd ${.CURDIR}/etc && ${MAKE} distrib-dirs)
.endif

afterinstall:
.ifndef NOMAN
	(cd ${.CURDIR}/share/man && ${MAKE} makedb)
.endif

build:
	(cd ${.CURDIR}/share/mk && ${MAKE} install)
	(cd ${.CURDIR}/include; ${MAKE} includes)
.if defined(KERBEROS)
	(cd ${.CURDIR}/kerberosIV/include && ${MAKE} install)
.endif
	${MAKE} cleandir
	(cd ${.CURDIR}/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/gnu/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
.if defined(KERBEROS)
	(cd ${.CURDIR}/kerberosIV && ${MAKE} build)
.endif
	${MAKE} depend && ${MAKE} && ${MAKE} install

.include <bsd.subdir.mk>
