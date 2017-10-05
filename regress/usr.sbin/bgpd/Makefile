# $OpenBSD: Makefile,v 1.1 2017/10/05 08:19:24 phessler Exp $

BGPDTESTS=1 2

REGRESS_TARGETS = config

.for n in ${BGPDTESTS}
BGPD_TARGETS+=bgpd${n}
BGPD_UPDATES+=bgpd${n}-update

bgpd${n}:
	bgpd -nv -f /dev/stdin < ${.CURDIR}/bgpd.conf.${n}.in | \
	    sed 's/router-id .*/router-id 127.0.0.1/' | \
	    diff -u ${.CURDIR}/bgpd.conf.${n}.ok /dev/stdin

bgpd${n}-update:
	bgpd -nv -f /dev/stdin < ${.CURDIR}/bgpd.conf.${n}.in | \
	    sed 's/router-id .*/router-id 127.0.0.1/' > \
	    ${.CURDIR}/bgpd.conf.${n}.ok
.endfor

config: bgpd-example ${BGPD_TARGETS}
bgpd-update: bgpd-example-update ${BGPD_UPDATES}

bgpd-example:
	bgpd -nvvf ${.CURDIR}/../../../etc/examples/bgpd.conf | \
	    sed 's/router-id .*/router-id 127.0.0.1/' | \
	    diff -u ${.CURDIR}/bgpd.conf.example.ok /dev/stdin

bgpd-example-update:
	bgpd -nvvf ${.CURDIR}/../../../etc/examples/bgpd.conf | \
	    sed 's/router-id .*/router-id 127.0.0.1/' > \
	    ${.CURDIR}/bgpd.conf.example.ok

.include <bsd.regress.mk>
