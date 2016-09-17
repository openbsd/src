#	$OpenBSD: Makefile,v 1.4 2016/09/17 15:03:39 florian Exp $

.include <bsd.own.mk>

SUBDIR=		bgplg bgplgsh bgpctl ping traceroute

INCFILES=	bgplg.head \
		bgplg.foot \
		bgplg.css

WWWDIR=		${DESTDIR}/var/www/htdocs/bgplg
WWWFILES=	openbgpd.gif \
		index.html

distribution:
	for i in ${INCFILES}; do \
		${INSTALL} -C -o root -g wheel -m 0644 $$i ${DESTDIR}/var/www/conf/; \
	done
	${INSTALL} -d -o root -g wheel -m 0755 ${WWWDIR}
	for i in ${WWWFILES}; do \
		${INSTALL} -C -o root -g wheel -m 0644 $$i ${WWWDIR}; \
	done

.include <bsd.subdir.mk>
