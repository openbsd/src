# $OpenBSD: Makefile,v 1.5 2014/11/26 11:55:08 mpi Exp $

RDOMAIN?=	5

NET=	192.0.3.0
MASK=	255.255.255.0
GW=	192.0.2.1

.MAIN: all

.ifmake !obj && !clean && !cleandir && !depend && !regress
.if (${.TARGET} != all && ! make(all)) || (${.TARGET} == all)

.SILENT: .BEGIN .END

.BEGIN:
	-${SUDO} ifconfig lo10001 up rdomain ${RDOMAIN} 192.0.2.1/32
	-${SUDO} ifconfig lo10002 up rdomain ${RDOMAIN} 192.0.2.2/32
	-${SUDO} ifconfig lo10003 up rdomain ${RDOMAIN} 192.0.2.3/32
	-${SUDO} ifconfig lo10004 up rdomain ${RDOMAIN} 192.0.2.4/32

.END:
	-${SUDO} ifconfig lo10001 destroy
	-${SUDO} ifconfig lo10002 destroy
	-${SUDO} ifconfig lo10003 destroy
	-${SUDO} ifconfig lo10004 destroy

.INTERRUPT:
	-${SUDO} ifconfig lo10001 destroy
	-${SUDO} ifconfig lo10002 destroy
	-${SUDO} ifconfig lo10003 destroy
	-${SUDO} ifconfig lo10004 destroy

.endif
.endif

netmask:
.for cmd in add delete
.for mod in -net -dst
	@${SUDO} route -nt ${cmd} ${mod} ${NET}/24 ${GW}
	@${SUDO} route -nt ${cmd} ${mod} ${NET} -netmask ${MASK} ${GW}
.endfor
.endfor

RCMD=		${SUDO} route -T ${RDOMAIN} -n

n=	1
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add 10/8 192.0.2.1
	${RCMD} add 10.0/16 192.0.2.4
	${RCMD} add 10.0/10 192.0.2.4
	${RCMD} add 10.8/16 192.0.2.3
	${RCMD} add 10.8.0/24 192.0.2.1
	${RCMD} add 10.8.1/24 192.0.2.2
	${RCMD} add 10.8.3/24 192.0.2.3
	${RCMD} add 10.8.4/24 192.0.2.4
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	2
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -mpath 10.8.1/24 192.0.2.3
	${RCMD} add -mpath 10.8.1/24 192.0.2.4
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	3
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -mpath 10.8.1/24 192.0.2.3
	${RCMD} add -mpath 10.8.1/24 192.0.2.3 && exit 1 || exit 0
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin


n=	4
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -priority 16 10.8.1/24 192.0.2.1
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2
	${RCMD} add -priority 18 10.8.1/24 192.0.2.3
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	5
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -mpath -priority 16 10.8.1/24 192.0.2.2
	${RCMD} add -mpath -priority 17 10.8.1/24 192.0.2.2
	${RCMD} add -mpath -priority 18 10.8.1/24 192.0.2.2
	${RCMD} add -mpath -priority 17 10.8.1/24 192.0.2.2 && exit 1 || exit 0
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	6
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -priority 16 10.8.1/24 192.0.2.1
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2
	${RCMD} add -priority 18 10.8.1/24 192.0.2.3
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2 && exit 1 || exit 0
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	7
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -priority 16 10.8.1/24 192.0.2.1
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2
	${RCMD} add -priority 18 10.8.1/24 192.0.2.3
	${RCMD} delete -priority 17 10.8.1/24
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	8
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -priority 16 10.8.1/24 192.0.2.1
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2
	${RCMD} add -priority 18 10.8.1/24 192.0.2.3
	${SUDO} ifconfig lo10002 down
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2 && exit 1 || exit 0
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	9
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -priority 16 10.8.1/24 192.0.2.1
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2
	${RCMD} add -priority 18 10.8.1/24 192.0.2.3
	${SUDO} ifconfig lo10002 down
	${RCMD} delete -priority 17 10.8.1/24
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	10
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${SUDO} ifconfig lo10002 down
	${RCMD} add -priority 16 10.8.1/24 192.0.2.1
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2
	${RCMD} add -priority 18 10.8.1/24 192.0.2.3
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	11
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${SUDO} ifconfig lo10002 down
	${RCMD} add -priority 16 10.8.1/24 192.0.2.1
	${RCMD} add -priority 18 10.8.1/24 192.0.2.3
	${RCMD} add -priority 17 10.8.1/24 192.0.2.2
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	12
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -mpath 10.8.1/24 192.0.2.3
	${RCMD} change -mpath 10.8.1/24 192.0.2.4
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

n=	13
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add -mpath 10.8.1/24 192.0.2.3
	${RCMD} add -mpath 10.8.1/24 192.0.2.4
	${RCMD} change -mpath 10.8.1/24 192.0.2.5 && exit 1 || exit 0

# Changing MTU should not change the ifa
n=	14
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${SUDO} ifconfig lo10001 inet6 eui64 
	${SUDO} ifconfig lo10002 inet6 fd88::1/64
	${SUDO} ifconfig lo10003 inet6 fd99::1/64
	${RCMD} add -inet6 fd88::666 fd99::1
	${RCMD} change fd88::666 -mtu 1500
	${RCMD} show -inet6 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

# Changing the ifa should not change the gw
n=	15
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${RCMD} add 10.0/16 192.0.2.4
	${RCMD} change 10.0/16 -ifa 192.0.2.2
	${RCMD} show -inet 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

# Changing ifp should not change the gw
n=	16
RTTEST_TARGETS+:=rttest${n}
rttest${n}:
	${SUDO} ifconfig lo10001 inet6 eui64 
	${SUDO} ifconfig lo10002 inet6 fd88::1/64
	${SUDO} ifconfig lo10003 inet6 fd99::1/64
	${RCMD} add -inet6 fd88::666 fd99::1
	${RCMD} change fd88::666 -ifp lo10002
	${RCMD} show -inet6 2>&1 | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

REGRESS_TARGETS=netmask ${RTTEST_TARGETS}
REGRESS_ROOT_TARGETS=${REGRESS_TARGETS}
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
