# $OpenBSD: Makefile,v 1.1 2005/11/22 15:59:28 markus Exp $

.PHONY: ${REGRESS_TARGETS}

REGRESS_TARGETS+=t1
t1:
	merge -p /etc/motd /etc/motd /etc/motd | diff /etc/motd -

.include <bsd.regress.mk>
