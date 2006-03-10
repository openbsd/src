#      $OpenBSD: Makefile,v 1.1 2006/03/10 05:22:46 ray Exp $

CAP_MKDB?=	cap_mkdb

REGRESS_TARGETS=shortname
CLEANFILES+=	*.db

shortname:
	${CAP_MKDB} -if shortname ${.CURDIR}/shortname.in

.include <bsd.regress.mk>
