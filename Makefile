#	$NetBSD: Makefile,v 1.25 1995/10/09 02:11:28 thorpej Exp $

# NOTE THAT etc *DOES NOT* BELONG IN THE LIST BELOW

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share games
SUBDIR+= gnu

SUBDIR+= sys

.include <bsd.own.mk>	# for NOMAN, if it's there.

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

#beforeinstall:
#	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR=/ distrib-dirs)

afterinstall:
.ifndef NOMAN
	(cd ${.CURDIR}/share/man && ${MAKE} makedb)
.endif

build:
	(cd ${.CURDIR}/include && ${MAKE} install)
	${MAKE} cleandir
	(cd ${.CURDIR}/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/gnu/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
.if defined(KERBEROS)
	(cd ${.CURDIR}/kerberosIV && ${MAKE} build)
.endif
	${MAKE} depend && ${MAKE} && ${MAKE} install

.include <bsd.subdir.mk>
