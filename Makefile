#	$NetBSD: Makefile,v 1.25 1995/10/09 02:11:28 thorpej Exp $

# NOTE THAT etc *DOES NOT* BELONG IN THE LIST BELOW

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share games
SUBDIR+= gnu

SUBDIR+= sys

.if exists(domestic) && !defined(EXPORTABLE_SYSTEM)
SUBDIR+= domestic
.endif

.if exists(regress)
.ifmake !(install)
SUBDIR+= regress
.endif

regression-tests:
	@echo Running regression tests...
	@(cd ${.CURDIR}/regress && ${MAKE} regress)
.endif

.include <bsd.own.mk>	# for NOMAN, if it's there.

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
.if exists(domestic)
	(cd ${.CURDIR}/domestic/libcrypt && ${MAKE} depend && ${MAKE} && ${MAKE} install)
.endif
.if exists(kerberosIV)
	(cd ${.CURDIR}/kerberosIV && ${MAKE} depend && ${MAKE} && ${MAKE} install)
.endif
	${MAKE} depend && ${MAKE} && ${MAKE} install

.include <bsd.subdir.mk>
