#	$OpenBSD: Makefile.inc,v 1.1 2008/06/14 23:09:05 todd Exp $

MAKEDEVSRC=	${.CURDIR}/../MAKEDEV.mi
MAKEDEVSRC+=	${.CURDIR}/../MAKEDEV.sub
MAKEDEVSRC+=	${.CURDIR}/../MAKEDEV.common
MAKEDEVSRC+=	MAKEDEV.md

MAKEDEV: ${MAKEDEVSRC}
	@echo m4 -I.. ../MAKEDEV.mi '>' MAKEDEV
	@m4 -I${.CURDIR}/.. \
		${.CURDIR}/../MAKEDEV.mi > .MAKEDEV.tmp && \
		mv .MAKEDEV.tmp MAKEDEV || \
		rm .MAKEDEV.tmp
