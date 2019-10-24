# $OpenBSD: Makefile,v 1.2 2019/10/24 22:55:07 bluhm Exp $

# Copyright (c) 2019 Alexander Bluhm <bluhm@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# test ifconfig address configuration for ethernet and point-to-point

IFCONFIG ?=	${SUDO} ${KTRACE} /sbin/ifconfig
IFADDR =	${SUDO} ${KTRACE} ./ifaddr

ETHER_IF ?=	vether99
ETHER_ADDR ?=	10.188.254.74
ETHER_NET =	${ETHER_ADDR:C/\.[0-9][0-9]*$//}
PPP_IF ?=	ppp99
PPP_ADDR ?=	10.188.253.74
PPP_DEST ?=	10.188.253.75
PPP_NET =	${PPP_ADDR:C/\.[0-9][0-9]*$//}

PROG =		ifaddr

CLEANFILES =	ifconfig.out ktrace.out

### ether

REGRESS_TARGETS +=	run-ether-addr
run-ether-addr:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_ADDR}
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} ' ifconfig.out

REGRESS_TARGETS +=	run-ether-inet
run-ether-inet:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} inet ${ETHER_ADDR}
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} ' ifconfig.out

REGRESS_TARGETS +=	run-ether-netmask
run-ether-netmask:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_ADDR} netmask 255.255.255.0
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} netmask 0xffffff00 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-prefixlen
run-ether-prefixlen:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_ADDR}/24
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} netmask 0xffffff00 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-hexmask
run-ether-hexmask:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_ADDR} netmask 0xffffff00
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} netmask 0xffffff00 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-broadcast
run-ether-broadcast:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_ADDR}/24
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} .* broadcast ${ETHER_NET}.255$$' ifconfig.out

REGRESS_TARGETS +=	run-ether-replace
run-ether-replace:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	! grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-duplicate
run-ether-duplicate:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep -c 'inet ' ifconfig.out | grep -q 1

REGRESS_TARGETS +=	run-ether-host
run-ether-host:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/32
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 netmask 0xffffffff$$' ifconfig.out
	grep -c 'inet ' ifconfig.out | grep -q 1

REGRESS_TARGETS +=	run-ether-alias
run-ether-alias:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-alias-duplicate
run-ether-alias-duplicate:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24 alias
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep -c 'inet ' ifconfig.out | grep -q 1

REGRESS_TARGETS +=	run-ether-replace-first
run-ether-replace-first:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.3/24
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	! grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 ' ifconfig.out
	grep 'inet ${ETHER_NET}.3 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-alias-host
run-ether-alias-host:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/32 alias
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 netmask 0xffffffff$$' ifconfig.out
	grep -c 'inet ' ifconfig.out | grep -q 1

REGRESS_TARGETS +=	run-ether-change-netmask
run-ether-change-netmask:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/32
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 netmask 0xffffffff$$' ifconfig.out
	grep 'inet ${ETHER_NET}.2 ' ifconfig.out
	grep -c 'inet ' ifconfig.out | grep -q 2

REGRESS_TARGETS +=	run-ether-delete-netmask
run-ether-delete-netmask:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	# XXX ifconfig deletes .1 and changes .2 netmask
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/32
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	! grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 netmask 0xffffffff$$' ifconfig.out
	grep -c 'inet ' ifconfig.out | grep -q 1

REGRESS_TARGETS +=	run-ether-alias-netmask
run-ether-alias-netmask:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/32 alias
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 netmask 0xffffffff$$' ifconfig.out
	grep -c 'inet ${ETHER_NET}.2 ' ifconfig.out | grep -q 1

REGRESS_TARGETS +=	run-ether-delete
run-ether-delete:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1 delete
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	! grep 'inet ' ifconfig.out

REGRESS_TARGETS +=	run-ether-delete-first
run-ether-delete-first:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1 delete
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	! grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-delete-second
run-ether-delete-second:
	@echo '======== $@ ========'
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	${IFCONFIG} ${ETHER_IF} ${ETHER_NET}.2 delete
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	! grep 'inet ${ETHER_NET}.2 ' ifconfig.out

### ppp

REGRESS_TARGETS +=	run-ppp-addr
run-ppp-addr:
	@echo '======== $@ ========'
	${IFCONFIG} ${PPP_IF} ${PPP_ADDR}
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_ADDR} ' ifconfig.out

REGRESS_TARGETS +=	run-ppp-inet
run-ppp-inet:
	@echo '======== $@ ========'
	${IFCONFIG} ${PPP_IF} inet ${PPP_ADDR}
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_ADDR} ' ifconfig.out

REGRESS_TARGETS +=	run-ppp-netmask
run-ppp-netmask:
	@echo '======== $@ ========'
	${IFCONFIG} ${PPP_IF} ${PPP_ADDR} netmask 255.255.255.0
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_ADDR} .* netmask 0xffffff00$$' ifconfig.out

REGRESS_TARGETS +=	run-ppp-prefixlen
run-ppp-prefixlen:
	@echo '======== $@ ========'
	${IFCONFIG} ${PPP_IF} ${PPP_ADDR}/24
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_ADDR} .* netmask 0xffffff00$$' ifconfig.out

REGRESS_TARGETS +=	run-ppp-destination
run-ppp-destination:
	@echo '======== $@ ========'
	${IFCONFIG} ${PPP_IF} ${PPP_ADDR}/24 ${PPP_DEST}
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_ADDR} --> ${PPP_DEST} ' ifconfig.out

REGRESS_TARGETS +=	run-ppp-replace
run-ppp-replace:
	@echo '======== $@ ========'
	${IFCONFIG} ${PPP_IF} ${PPP_NET}.1/24 ${PPP_DEST}
	${IFCONFIG} ${PPP_IF} ${PPP_NET}.2/24 ${PPP_DEST}
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_NET}.2 --> ${PPP_DEST} ' ifconfig.out
	! grep 'inet ${PPP_NET}.1 --> ${PPP_DEST} ' ifconfig.out

REGRESS_TARGETS +=	run-ppp-alias
run-ppp-alias:
	@echo '======== $@ ========'
	${IFCONFIG} ${PPP_IF} ${PPP_NET}.1/24 ${PPP_DEST}
	${IFCONFIG} ${PPP_IF} ${PPP_NET}.2/24 ${PPP_DEST} alias
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_NET}.1 --> ${PPP_DEST} ' ifconfig.out
	grep 'inet ${PPP_NET}.2 --> ${PPP_DEST} ' ifconfig.out

### ifaddr

REGRESS_TARGETS +=	run-ether-ifaddr-set
run-ether-ifaddr-set:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_ADDR}
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} ' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-get
run-ether-ifaddr-get:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_ADDR}
	${KTRACE} ./ifaddr ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} ' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-netmask
run-ether-ifaddr-netmask:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_ADDR} netmask 255.255.255.0
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} netmask 0xffffff00 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-prefixlen
run-ether-ifaddr-prefixlen:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_ADDR}/24
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} netmask 0xffffff00 ' ifconfig.out

REGRESS_TARGETS +=	run-ppp-ifaddr-destination
run-ppp-ifaddr-destination:
	@echo '======== $@ ========'
	${IFADDR} ${PPP_IF} ${PPP_ADDR} ${PPP_DEST}
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_ADDR} --> ${PPP_DEST} ' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-broadcast
run-ether-ifaddr-broadcast:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_ADDR} broadcast ${ETHER_NET}.255
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_ADDR} .* broadcast ${ETHER_NET}.255$$' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-alias
run-ether-ifaddr-alias:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-change-netmask
run-ether-ifaddr-change-netmask:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.2/24 alias
	${IFADDR} ${ETHER_IF} netmask 255.255.255.255
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 netmask 0xffffffff ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 netmask 0xffffff00 ' ifconfig.out

REGRESS_TARGETS +=	run-ppp-ifaddr-change-destination
run-ppp-ifaddr-change-destination:
	@echo '======== $@ ========'
	${IFADDR} ${PPP_IF} ${PPP_NET}.1 ${PPP_NET}.11
	${IFADDR} ${PPP_IF} ${PPP_NET}.2 ${PPP_NET}.12 alias
	${IFADDR} ${PPP_IF} ipdst ${PPP_NET}.13
	/sbin/ifconfig ${PPP_IF} >ifconfig.out
	grep 'inet ${PPP_NET}.1 --> ${PPP_NET}.13 ' ifconfig.out
	grep 'inet ${PPP_NET}.2 --> ${PPP_NET}.12 ' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-change-broadcast
run-ether-ifaddr-change-broadcast:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.1/24 broadcast ${ETHER_NET}.255
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.2/24 broadcast ${ETHER_NET}.255 alias
	${IFADDR} ${ETHER_IF} broadcast 255.255.255.255
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	grep 'inet ${ETHER_NET}.1 .* broadcast 255.255.255.255$$' ifconfig.out
	grep 'inet ${ETHER_NET}.2 .* broadcast ${ETHER_NET}.255$$' ifconfig.out

REGRESS_TARGETS +=	run-ether-ifaddr-duplicate
run-ether-ifaddr-duplicate:
	@echo '======== $@ ========'
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.1/24
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.2/16 alias
	# XXX replace the first address and create two identical addresses
	${IFADDR} ${ETHER_IF} ${ETHER_NET}.2/24
	/sbin/ifconfig ${ETHER_IF} >ifconfig.out
	! grep 'inet ${ETHER_NET}.1 ' ifconfig.out
	grep 'inet ${ETHER_NET}.2 netmask 0xffffff00 ' ifconfig.out
	grep -c 'inet ' ifconfig.out | grep -q 2

### setup cleanup

REGRESS_ROOT_TARGETS =	${REGRESS_TARGETS}

${REGRESS_TARGETS:Mrun-*-ifaddr-*}: ifaddr

${REGRESS_TARGETS:Mrun-ether-*}: setup-ether
setup-ether:
	@echo '======== $@ ========'
	${SUDO} /sbin/ifconfig ${ETHER_IF} destroy 2>/dev/null || true
	${SUDO} /sbin/ifconfig ${ETHER_IF} create

${REGRESS_TARGETS:Mrun-ppp-*}: setup-ppp
setup-ppp:
	@echo '======== $@ ========'
	${SUDO} /sbin/ifconfig ${PPP_IF} destroy 2>/dev/null || true
	${SUDO} /sbin/ifconfig ${PPP_IF} create

REGRESS_CLEANUP =	cleanup
cleanup:
	@echo '======== $@ ========'
	${SUDO} /sbin/ifconfig ${ETHER_IF} destroy || true
	${SUDO} /sbin/ifconfig ${PPP_IF} destroy || true

### check

check: check-targets

check-targets:
	# REGRESS_TARGETS must not contain duplicates, prevent copy paste error
	! echo ${REGRESS_TARGETS} | tr ' ' '\n' | sort | uniq -d | grep .

.include <bsd.regress.mk>
