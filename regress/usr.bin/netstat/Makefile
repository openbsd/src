# $OpenBSD: Makefile,v 1.3 2023/10/19 18:36:41 anton Exp $

ROUTE?=		/sbin/route
RDOMAIN?=	5
NETSTAT?=	/usr/bin/netstat

.MAIN: all

.ifmake !obj && !clean && !cleandir && !regress
.if (${.TARGET} != all && ! make(all)) || (${.TARGET} == all)

.SILENT: .BEGIN .END

.BEGIN:
	${SUDO} ifconfig vether99 destroy 2>/dev/null || true
	${SUDO} ifconfig lo${RDOMAIN} destroy 2>/dev/null || true

.END:
	${SUDO} ifconfig vether99 destroy 2>/dev/null || true
	${SUDO} ifconfig lo${RDOMAIN} destroy 2>/dev/null || true

.endif
.endif

RCMD=		${SUDO} ${ROUTE} -T ${RDOMAIN} -n


# Check that 0.0.0.0 host route is displayed properly.
# It was once displayed "default".
n=	1
NSTEST_TARGETS+:=nstest${n}
nstest${n}:
	${SUDO} ifconfig vether99 rdomain ${RDOMAIN} lladdr fe:e1:ba:d4:c8:1d up
	${SUDO} ifconfig vether99 192.168.89.5
	${RCMD} add 0.0.0.0 192.168.89.1
	${NETSTAT} -T ${RDOMAIN} -rnf inet 2>&1 | \
		sed -e "s,link\#[0-9 ]*U,link#              U," | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

REGRESS_TARGETS=${NSTEST_TARGETS}
REGRESS_ROOT_TARGETS=${REGRESS_TARGETS}
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
