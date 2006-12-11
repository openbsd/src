#	$OpenBSD: Makefile,v 1.1 2006/12/11 23:10:10 reyk Exp $

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
