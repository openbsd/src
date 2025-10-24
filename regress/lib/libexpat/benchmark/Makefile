# $OpenBSD: Makefile,v 1.2 2025/10/24 05:14:25 anton Exp $

# take any large xml file located in our source tree
XMLFILE?=	${BSDSRCDIR}/regress/usr.sbin/rpki-client/rrdp/notification.xml

run-regress-benchmark: ${PROG}
	time ./${PROG} ${XMLFILE} 65535 10000

.include <bsd.regress.mk>
