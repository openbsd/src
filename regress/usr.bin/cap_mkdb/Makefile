#      $OpenBSD: Makefile,v 1.2 2006/03/22 02:46:17 ray Exp $

CAP_MKDB?=	cap_mkdb

REGRESS_TARGETS=shortname
CLEANFILES+=	*.db

shortname:
	${CAP_MKDB} -if shortname ${.CURDIR}/shortname.in || case "$$?" in 1) exit 0;; esac && exit 1

.include <bsd.regress.mk>
